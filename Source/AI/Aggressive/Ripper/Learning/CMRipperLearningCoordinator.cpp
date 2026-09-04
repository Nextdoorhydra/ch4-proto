#include "Aggressive/Ripper/Learning/CMRipperLearningCoordinator.h"

#include "Aggressive/Ripper/CMRipperPawn.h"
#include "Aggressive/Common/Behavior/CMAggressiveBehaviorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Aggressive/Common/Learning/CMAggressiveLearningInteractor.h"
#include "Aggressive/Common/Learning/CMAggressiveLearningSnapshot.h"
#include "LearningAgentsCommunicator.h"
#include "LearningAgentsCritic.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsNeuralNetwork.h"
#include "LearningAgentsPolicy.h"
#include "LearningAgentsPPOTrainer.h"
#include "LearningNeuralNetwork.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "TimerManager.h"

// Actor Tick 없이 Ripper AI 학습을 관리할 중앙 Manager를 생성한다.
ACMRipperLearningCoordinator::ACMRipperLearningCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    LearningManager = CreateDefaultSubobject<ULearningAgentsManager>(TEXT("RipperLearningManager"));
    LearningManager->PrimaryComponentTick.bCanEverTick = true;
    LearningManager->PrimaryComponentTick.bStartWithTickEnabled = false;
    GoalSettings.AcceptanceRadius = 50.0f;
    RewardSettings.MaximumFastArrivalReward = 10.0f;
    RewardSettings.YawAngularVelocityPenaltyScale = 0.0016f;
    RewardSettings.FacingProgressRewardScale = 0.5f;
    RewardSettings.StepPenalty = 0.004f;
}

// Actor 종료 시 외부 PPO 프로세스와 판단 타이머를 종료한다.
void ACMRipperLearningCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopTraining();
    Super::EndPlay(EndPlayReason);
}

// Ripper AI 하나를 배열 호출로 전달해 학습을 시작한다.
bool ACMRipperLearningCoordinator::StartTraining(ACMRipperPawn* InTrainingAgent)
{
    TArray<ACMRipperPawn*> Agents;
    Agents.Add(InTrainingAgent);

    return StartTrainingAgents(Agents);
}

// 지정한 Ripper AI들을 하나의 세 다리 정책 에이전트로 등록하고 PPO 학습을 시작한다.
bool ACMRipperLearningCoordinator::StartTrainingAgents(const TArray<ACMRipperPawn*>& InTrainingAgents)
{
    if (IsTraining())
        return true;
    if (!LearningManager || LearningManager->GetAgentNum() > 0)
        return false;

    constexpr int32 MaximumTrainingAgentCount = 8;
    if (InTrainingAgents.IsEmpty() || InTrainingAgents.Num() > MaximumTrainingAgentCount)
        return false;

    TSet<const ACMRipperPawn*> UniqueAgents;
    for (const ACMRipperPawn* Agent : InTrainingAgents)
    {
        if (!Agent || UniqueAgents.Contains(Agent) || Agent->GetLegCount() != 3)
        {
            return false;
        }
        UniqueAgents.Add(Agent);
    }

    TrainingAgents.Reset(InTrainingAgents.Num());
    for (ACMRipperPawn* Agent : InTrainingAgents)
    {
        if (UCMAggressiveBehaviorComponent* Behavior = Agent->FindComponentByClass<UCMAggressiveBehaviorComponent>())
        {
            Behavior->SetBehaviorEnabled(false);
        }
        TrainingAgents.Add(Agent);
    }

    LearningManager->SetMaxAgentNum(TrainingAgents.Num());
    if (!InitializeLearningObjects())
        return false;

    ULearningAgentsNeuralNetwork* PolicyNetwork = Policy->GetPolicyNetworkAsset();
    if (!PolicyNetwork || !PolicyNetwork->NeuralNetworkData)
        return false;
    InitialPolicyContentHash = PolicyNetwork->NeuralNetworkData->GetContentHash();
    bHasReceivedPolicyUpdate = false;

    TArray<UObject*> AgentObjects;
    AgentObjects.Reserve(TrainingAgents.Num());
    for (ACMRipperPawn* Agent : TrainingAgents)
    {
        Agent->GetPathMovement()->SetPolicyControlEnabled(true);
        AgentObjects.Add(Agent);
    }
    LearningManager->AddAgents(TrainingAgentIds, AgentObjects);
    if (TrainingAgentIds.Contains(INDEX_NONE))
    {
        LearningManager->RemoveAllAgents();

        return false;
    }

    FLearningAgentsPPOTrainingSettings TrainingSettings;
    TrainingSettings.Device = ELearningAgentsTrainingDevice::CPU;
    PPOTrainer->RunTraining(TrainingSettings);
    if (!PPOTrainer->IsTraining() || PPOTrainer->HasTrainingFailed())
        return false;

    const double CurrentTime = GetWorld()->GetTimeSeconds();
    NextSnapshotSaveTime = CurrentTime + FMath::Max(SnapshotSaveIntervalSeconds, 1.0f);
    LearningManager->SetComponentTickInterval(FMath::Max(DecisionInterval, 0.01f));
    LearningManager->SetComponentTickEnabled(true);
    GetWorldTimerManager().SetTimer(TrainingTimerHandle, this, &ThisClass::RunTrainingStep, FMath::Max(DecisionInterval, 0.01f), true);

    return true;
}

// 현재 레벨의 Ripper AI를 한 번 검색해 병렬 학습을 시작한다.
bool ACMRipperLearningCoordinator::StartTrainingAllAgents()
{
    UWorld* World = GetWorld();
    if (!World)
        return false;

    TArray<ACMRipperPawn*> Agents;
    for (TActorIterator<ACMRipperPawn> It(World); It; ++It)
        Agents.Add(*It);

    return StartTrainingAgents(Agents);
}

// PPO 학습을 끝내고 최신 정책을 저장한 뒤 에이전트 등록을 비운다.
void ACMRipperLearningCoordinator::StopTraining()
{
    GetWorldTimerManager().ClearTimer(TrainingTimerHandle);
    if (LearningManager)
        LearningManager->SetComponentTickEnabled(false);

    if (PPOTrainer && PPOTrainer->IsTraining())
    {
        PPOTrainer->EndTraining();
        RefreshPolicyUpdateState();
        SaveTrainingSnapshots();
    }

    for (ACMRipperPawn* Agent : TrainingAgents)
    {
        if (!Agent)
            continue;
        Agent->StopPathMove();
    }
    if (LearningManager && LearningManager->GetAgentNum() > 0)
        LearningManager->RemoveAllAgents();
    TrainingAgentIds.Reset();
    TrainingAgents.Reset();
}

// 이어서 학습하고 추론에 사용할 최신 Ripper AI 네트워크를 저장한다.
bool ACMRipperLearningCoordinator::SaveTrainingSnapshots()
{
    if (!Policy || !Critic)
        return false;

    RefreshPolicyUpdateState();
    if (!bHasReceivedPolicyUpdate)
        return false;

    const FString Directory = GetSnapshotDirectory();
    const bool bSaved = CMAggressiveLearningSnapshot::SaveTrainingNetworks(ECMAggressiveLearningSnapshotProfile::Ripper, *Policy, *Critic, Directory);
    return bSaved;
}

// 외부 PPO 프로세스가 정상 학습 중인지 반환한다.
bool ACMRipperLearningCoordinator::IsTraining() const
{
    return PPOTrainer && PPOTrainer->IsTraining() && !PPOTrainer->HasTrainingFailed();
}

// Ripper AI의 최신 학습 스냅샷 절대 경로를 반환한다.
FString ACMRipperLearningCoordinator::GetSnapshotDirectory() const
{
    return CMAggressiveLearningSnapshot::GetLatestDirectory(ECMAggressiveLearningSnapshotProfile::Ripper);
}

// 현재 Ripper AI 정책에 등록된 에이전트 수를 반환한다.
int32 ACMRipperLearningCoordinator::GetTrainingAgentCount() const
{
    return TrainingAgentIds.Num();
}

// 외부 PPO 학습기로부터 갱신된 정책을 받았는지 반환한다.
bool ACMRipperLearningCoordinator::HasReceivedPolicyUpdate() const
{
    return bHasReceivedPolicyUpdate;
}

// 세 다리 Interactor와 공용 보상 환경 및 PPO 트레이너를 생성한다.
bool ACMRipperLearningCoordinator::InitializeLearningObjects()
{
    ULearningAgentsManager* Manager = LearningManager;
    Interactor = UCMAggressiveLearningInteractor::MakeAggressiveInteractor(Manager, 3, TEXT("RipperInteractor"));
    if (!Interactor)
        return false;

    ULearningAgentsInteractor* BaseInteractor = Interactor;
    Policy = ULearningAgentsPolicy::MakePolicy(Manager, BaseInteractor, ULearningAgentsPolicy::StaticClass(), TEXT("RipperPolicy"));
    if (!Policy)
        return false;

    TrainingEnvironment = UCMAggressiveLearningTrainingEnvironment::MakeAggressiveTrainingEnvironment(Manager, RewardSettings, GoalSettings, TEXT("RipperTrainingEnvironment"));
    if (!TrainingEnvironment)
        return false;

    ULearningAgentsPolicy* BasePolicy = Policy;
    Critic = ULearningAgentsCritic::MakeCritic(Manager, BaseInteractor, BasePolicy, ULearningAgentsCritic::StaticClass(), TEXT("RipperCritic"));
    if (!Critic)
        return false;

    const FString LatestDirectory = GetSnapshotDirectory();
    if (bResumeExistingSnapshots && CMAggressiveLearningSnapshot::HasAnyTrainingSnapshots(ECMAggressiveLearningSnapshotProfile::Ripper, LatestDirectory))
    {
        if (!CMAggressiveLearningSnapshot::HasCompleteTrainingSnapshots(ECMAggressiveLearningSnapshotProfile::Ripper, LatestDirectory))
            return false;
        if (!CMAggressiveLearningSnapshot::LoadTrainingNetworks(ECMAggressiveLearningSnapshotProfile::Ripper, *Policy, *Critic, LatestDirectory))
            return false;
    }
    else if (bResumeExistingSnapshots)
    {
        const FString BootstrapDirectory = CMAggressiveLearningSnapshot::GetBootstrapDirectory(ECMAggressiveLearningSnapshotProfile::Ripper);
        if (!CMAggressiveLearningSnapshot::HasCompleteTrainingSnapshots(ECMAggressiveLearningSnapshotProfile::Ripper, BootstrapDirectory) || !CMAggressiveLearningSnapshot::LoadTrainingNetworks(ECMAggressiveLearningSnapshotProfile::Ripper, *Policy, *Critic, BootstrapDirectory))
            return false;
    }

    const FLearningAgentsCommunicator Communicator = ULearningAgentsCommunicatorLibrary::MakeSharedMemoryTrainingProcess();
    ULearningAgentsTrainingEnvironment* BaseEnvironment = TrainingEnvironment;
    ULearningAgentsCritic* BaseCritic = Critic;
    FLearningAgentsPPOTrainerSettings TrainerSettings;
    const int32 TwoFullEpisodeDecisionCount = FMath::CeilToInt(RewardSettings.MaxEpisodeSeconds / FMath::Max(DecisionInterval, 0.01f)) * TrainingAgents.Num() * 2;
    TrainerSettings.MaximumRecordedStepsPerIteration = FMath::Max(MaximumRecordedStepsPerIteration, TwoFullEpisodeDecisionCount);
    PPOTrainer = ULearningAgentsPPOTrainer::MakePPOTrainer(Manager, BaseInteractor, BaseEnvironment, BasePolicy, BaseCritic, Communicator, ULearningAgentsPPOTrainer::StaticClass(), TEXT("RipperPPOTrainer"), TrainerSettings);

    return PPOTrainer != nullptr;
}

// 초기 정책과 현재 정책의 내용 해시가 달라졌는지 확인한다.
void ACMRipperLearningCoordinator::RefreshPolicyUpdateState()
{
    if (bHasReceivedPolicyUpdate || !Policy)
        return;

    ULearningAgentsNeuralNetwork* PolicyNetwork = Policy->GetPolicyNetworkAsset();
    if (!PolicyNetwork || !PolicyNetwork->NeuralNetworkData)
        return;
    bHasReceivedPolicyUpdate = PolicyNetwork->NeuralNetworkData->GetContentHash() != InitialPolicyContentHash;
}

// 현재 경험을 PPO에 전달하고 로그 및 자동 저장 시점을 확인한다.
void ACMRipperLearningCoordinator::RunTrainingStep()
{
    if (!PPOTrainer || PPOTrainer->HasTrainingFailed())
    {
        StopTraining();

        return;
    }

    PPOTrainer->RunTraining();
    RefreshPolicyUpdateState();
    SaveTrainingSnapshotsIfNeeded();
}

// 설정된 시간이 되면 최신 Ripper AI 학습 네트워크를 저장한다.
void ACMRipperLearningCoordinator::SaveTrainingSnapshotsIfNeeded()
{
    const double CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime < NextSnapshotSaveTime)
        return;
    NextSnapshotSaveTime = CurrentTime + FMath::Max(SnapshotSaveIntervalSeconds, 1.0f);
    SaveTrainingSnapshots();
}
