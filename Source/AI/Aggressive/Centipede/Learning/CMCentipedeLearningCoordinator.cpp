#include "Aggressive/Centipede/Learning/CMCentipedeLearningCoordinator.h"

#include "Aggressive/Centipede/CMCentipedePawn.h"
#include "Aggressive/Common/Behavior/CMAggressiveBehaviorComponent.h"
#include "EngineUtils.h"
#include "Aggressive/Centipede/Learning/CMCentipedeLearningInteractor.h"
#include "Aggressive/Common/Learning/CMAggressiveLearningSnapshot.h"
#include "Aggressive/Centipede/Learning/CMCentipedeLearningTrainingEnvironment.h"
#include "LearningAgentsCommunicator.h"
#include "LearningAgentsCritic.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsNeuralNetwork.h"
#include "LearningAgentsPolicy.h"
#include "LearningAgentsPPOTrainer.h"
#include "LearningNeuralNetwork.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMCentipedeLearning, Log, All);

// Actor Tick 없이 Centipede 병렬 PPO 학습을 관리할 중앙 Manager를 구성한다.
ACMCentipedeLearningCoordinator::ACMCentipedeLearningCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    LearningManager = CreateDefaultSubobject<ULearningAgentsManager>(TEXT("CentipedeLearningManager"));
    LearningManager->PrimaryComponentTick.bCanEverTick = true;
    LearningManager->PrimaryComponentTick.bStartWithTickEnabled = false;
    GoalSettings.GoalDistance = 1200.0f;
    GoalSettings.AcceptanceRadius = 120.0f;
    RewardSettings.MaxEpisodeSeconds = 30.0f;
    RewardSettings.MaximumFastArrivalReward = 5.0f;
    RewardSettings.YawAngularVelocityPenaltyScale = 0.001f;
    RewardSettings.StepPenalty = 0.003f;
}

// Actor 종료 시 외부 PPO 프로세스와 판단 타이머를 안전하게 종료한다.
void ACMCentipedeLearningCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopTraining();
    Super::EndPlay(EndPlayReason);
}

bool ACMCentipedeLearningCoordinator::StartTraining(ACMCentipedePawn* InTrainingAgent)
{
    return StartTrainingAgents({InTrainingAgent});
}

// Centipede들을 공용 정책 에이전트로 등록하고 병렬 PPO 학습을 시작한다.
bool ACMCentipedeLearningCoordinator::StartTrainingAgents(const TArray<ACMCentipedePawn*>& InTrainingAgents)
{
    if (IsTraining())
        return true;
    if (!LearningManager || LearningManager->GetAgentNum() > 0 || InTrainingAgents.IsEmpty() || InTrainingAgents.Num() > 8)
        return false;

    TSet<const ACMCentipedePawn*> UniqueAgents;
    for (const ACMCentipedePawn* Agent : InTrainingAgents)
    {
        if (!Agent || UniqueAgents.Contains(Agent) || Agent->GetBodySegmentCount() != 4 || Agent->GetJointCount() != 3 || Agent->GetLegCount() != 8)
            return false;
        UniqueAgents.Add(Agent);
    }

    TrainingAgents.Reset(InTrainingAgents.Num());
    for (ACMCentipedePawn* Agent : InTrainingAgents)
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
    for (ACMCentipedePawn* Agent : TrainingAgents)
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

    LearningManager->SetComponentTickInterval(FMath::Max(DecisionInterval, 0.01f));
    LearningManager->SetComponentTickEnabled(true);
    const double CurrentTime = GetWorld()->GetTimeSeconds();
    TotalTrainingStepCount = 0;
    TotalAgentDecisionCount = 0;
    NextProgressLogTime = CurrentTime + FMath::Max(ProgressLogIntervalSeconds, 0.1f);
    NextSnapshotSaveTime = CurrentTime + FMath::Max(SnapshotSaveIntervalSeconds, 1.0f);
    GetWorldTimerManager().SetTimer(TrainingTimerHandle, this, &ThisClass::RunTrainingStep, FMath::Max(DecisionInterval, 0.01f), true);
    UE_LOG(LogCMCentipedeLearning, Display, TEXT("Centipede AI PPO 학습을 시작했습니다. 에이전트=%d 관측=32 행동=8 판단주기=%.2f초 저장주기=%.1f초"), TrainingAgentIds.Num(), DecisionInterval, SnapshotSaveIntervalSeconds);

    return true;
}

bool ACMCentipedeLearningCoordinator::StartTrainingAllAgents()
{
    TArray<ACMCentipedePawn*> Agents;
    if (UWorld* World = GetWorld())
        for (TActorIterator<ACMCentipedePawn> It(World); It; ++It)
            Agents.Add(*It);

    return StartTrainingAgents(Agents);
}

// PPO 학습을 끝내고 최신 네트워크를 저장한 뒤 에이전트 등록을 비운다.
void ACMCentipedeLearningCoordinator::StopTraining()
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
    for (ACMCentipedePawn* Agent : TrainingAgents)
    {
        if (!Agent)
            continue;
        Agent->SetTrainingCurveProfile(0);
        Agent->StopPathMove();
        Agent->StopArticulatedBodyMotion();
    }
    if (LearningManager && LearningManager->GetAgentNum() > 0)
        LearningManager->RemoveAllAgents();
    TrainingAgentIds.Reset();
    TrainingAgents.Reset();
}

// 이어서 학습하고 추론에 사용할 최신 Centipede 네트워크를 저장한다.
bool ACMCentipedeLearningCoordinator::SaveTrainingSnapshots()
{
    if (!Policy || !Critic)
        return false;
    RefreshPolicyUpdateState();
    const bool bSaved = bHasReceivedPolicyUpdate && CMAggressiveLearningSnapshot::SaveTrainingNetworks(ECMAggressiveLearningSnapshotProfile::Centipede, *Policy, *Critic, GetSnapshotDirectory());
    if (bSaved)
        UE_LOG(LogCMCentipedeLearning, Display, TEXT("Centipede AI 학습 스냅샷 저장 - 판단=%lld 경로=%s"), TotalAgentDecisionCount, *GetSnapshotDirectory());

    return bSaved;
}

bool ACMCentipedeLearningCoordinator::IsTraining() const
{
    return PPOTrainer && PPOTrainer->IsTraining() && !PPOTrainer->HasTrainingFailed();
}

FString ACMCentipedeLearningCoordinator::GetSnapshotDirectory() const
{
    return CMAggressiveLearningSnapshot::GetLatestDirectory(ECMAggressiveLearningSnapshotProfile::Centipede);
}

int32 ACMCentipedeLearningCoordinator::GetTrainingAgentCount() const
{
    return TrainingAgentIds.Num();
}

// 관절 몸체 Interactor와 전용 보상 환경 및 PPO 트레이너를 생성한다.
bool ACMCentipedeLearningCoordinator::InitializeLearningObjects()
{
    ULearningAgentsManager* Manager = LearningManager;
    Interactor = UCMCentipedeLearningInteractor::MakeCentipedeInteractor(Manager);
    if (!Interactor)
        return false;

    ULearningAgentsInteractor* BaseInteractor = Interactor;
    Policy = ULearningAgentsPolicy::MakePolicy(Manager, BaseInteractor, ULearningAgentsPolicy::StaticClass(), TEXT("CentipedePolicy"));

    if (!Policy)
        return false;

    TrainingEnvironment = UCMCentipedeLearningTrainingEnvironment::MakeCentipedeTrainingEnvironment(Manager, RewardSettings, GoalSettings, JointTrackingPenaltyScale);

    if (!TrainingEnvironment)
        return false;

    ULearningAgentsPolicy* BasePolicy = Policy;
    Critic = ULearningAgentsCritic::MakeCritic(Manager, BaseInteractor, BasePolicy, ULearningAgentsCritic::StaticClass(), TEXT("CentipedeCritic"));

    if (!Critic)
        return false;

    const FString SnapshotDirectory = GetSnapshotDirectory();
    if (bResumeExistingSnapshots && CMAggressiveLearningSnapshot::HasAnyTrainingSnapshots(ECMAggressiveLearningSnapshotProfile::Centipede, SnapshotDirectory))
    {
        if (!CMAggressiveLearningSnapshot::HasCompleteTrainingSnapshots(ECMAggressiveLearningSnapshotProfile::Centipede, SnapshotDirectory) || !CMAggressiveLearningSnapshot::LoadTrainingNetworks(ECMAggressiveLearningSnapshotProfile::Centipede, *Policy, *Critic, SnapshotDirectory))
            return false;
    }

    const FLearningAgentsCommunicator Communicator = ULearningAgentsCommunicatorLibrary::MakeSharedMemoryTrainingProcess();
    ULearningAgentsTrainingEnvironment* BaseEnvironment = TrainingEnvironment;
    ULearningAgentsCritic* BaseCritic = Critic;
    FLearningAgentsPPOTrainerSettings TrainerSettings;

    const int32 TwoEpisodeSteps = FMath::CeilToInt(RewardSettings.MaxEpisodeSeconds / FMath::Max(DecisionInterval, 0.01f)) * TrainingAgents.Num() * 2;
    TrainerSettings.MaximumRecordedStepsPerIteration = FMath::Max(MaximumRecordedStepsPerIteration, TwoEpisodeSteps);
    PPOTrainer = ULearningAgentsPPOTrainer::MakePPOTrainer(Manager, BaseInteractor, BaseEnvironment, BasePolicy, BaseCritic, Communicator, ULearningAgentsPPOTrainer::StaticClass(), TEXT("CentipedePPOTrainer"), TrainerSettings);

    return PPOTrainer != nullptr;
}

// 현재 경험을 PPO에 전달하고 정책 갱신·로그·자동 저장 시점을 확인한다.
void ACMCentipedeLearningCoordinator::RunTrainingStep()
{
    if (!PPOTrainer || PPOTrainer->HasTrainingFailed())
    {
        StopTraining();

        return;
    }

    PPOTrainer->RunTraining();
    ++TotalTrainingStepCount;
    TotalAgentDecisionCount += TrainingAgentIds.Num();
    RefreshPolicyUpdateState();
    LogTrainingProgressIfNeeded();

    if (GetWorld()->GetTimeSeconds() >= NextSnapshotSaveTime)
    {
        NextSnapshotSaveTime = GetWorld()->GetTimeSeconds() + FMath::Max(SnapshotSaveIntervalSeconds, 1.0f);
        SaveTrainingSnapshots();
    }
}

void ACMCentipedeLearningCoordinator::LogTrainingProgressIfNeeded()
{
    const double CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime < NextProgressLogTime || !TrainingEnvironment)
        return;

    NextProgressLogTime = CurrentTime + FMath::Max(ProgressLogIntervalSeconds, 0.1f);
    UE_LOG(
        LogCMCentipedeLearning,
        Display,
        TEXT("Centipede AI 학습 상태 - 학습판단=%lld 에이전트판단=%lld 완료에피소드=%lld 성공=%lld 성공률=%.1f%%"),
        TotalTrainingStepCount,
        TotalAgentDecisionCount,
        TrainingEnvironment->GetCompletedEpisodeCount(),
        TrainingEnvironment->GetSuccessfulEpisodeCount(),
        TrainingEnvironment->GetSuccessRate() * 100.0f
    );
}

// 초기 정책과 현재 정책의 내용 해시를 비교해 갱신 수신 상태를 기록한다.
void ACMCentipedeLearningCoordinator::RefreshPolicyUpdateState()
{
    if (bHasReceivedPolicyUpdate || !Policy)
        return;

    ULearningAgentsNeuralNetwork* Network = Policy->GetPolicyNetworkAsset();
    if (Network && Network->NeuralNetworkData)
        bHasReceivedPolicyUpdate = Network->NeuralNetworkData->GetContentHash() != InitialPolicyContentHash;
}
