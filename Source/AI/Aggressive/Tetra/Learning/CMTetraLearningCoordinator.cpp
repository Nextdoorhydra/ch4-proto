#include "Aggressive/Tetra/Learning/CMTetraLearningCoordinator.h"

#include "Aggressive/Tetra/CMTetraPawn.h"
#include "Aggressive/Common/Behavior/CMAggressiveBehaviorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Aggressive/Tetra/Learning/CMTetraLearningInteractor.h"
#include "Aggressive/Common/Learning/CMAggressiveLearningSnapshot.h"
#include "LearningAgentsCommunicator.h"
#include "LearningAgentsCritic.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsNeuralNetwork.h"
#include "LearningAgentsPolicy.h"
#include "LearningAgentsPPOTrainer.h"
#include "LearningNeuralNetwork.h"
#include "Aggressive/Common/Movement/CMAggressiveAccelerationMovementComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "TimerManager.h"

// Actor Tick 없이 Tetra AI 병렬 학습을 관리할 중앙 Manager를 생성한다.
ACMTetraLearningCoordinator::ACMTetraLearningCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    LearningManager = CreateDefaultSubobject<ULearningAgentsManager>(TEXT("TetraLearningManager"));
    LearningManager->PrimaryComponentTick.bCanEverTick = true;
    LearningManager->PrimaryComponentTick.bStartWithTickEnabled = false;
}

// Actor 종료 시 외부 PPO 프로세스와 판단 타이머를 안전하게 종료한다.
void ACMTetraLearningCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopTraining();
    Super::EndPlay(EndPlayReason);
}

// Tetra AI 하나를 배열 호출로 전달해 PPO 학습을 시작한다.
bool ACMTetraLearningCoordinator::StartTraining(ACMTetraPawn* InTrainingAgent)
{
    TArray<ACMTetraPawn*> Agents;
    Agents.Add(InTrainingAgent);

    return StartTrainingAgents(Agents);
}

// Tetra AI들을 공용 정책 에이전트로 등록하고 병렬 PPO 학습을 시작한다.
bool ACMTetraLearningCoordinator::StartTrainingAgents(const TArray<ACMTetraPawn*>& InTrainingAgents)
{
    if (IsTraining())
        return true;
    if (!LearningManager || LearningManager->GetAgentNum() > 0)
        return false;

    constexpr int32 MaximumTrainingAgentCount = 8;
    if (InTrainingAgents.IsEmpty() || InTrainingAgents.Num() > MaximumTrainingAgentCount)
        return false;

    TSet<const ACMTetraPawn*> UniqueAgents;
    for (const ACMTetraPawn* Agent : InTrainingAgents)
    {
        if (!Agent || UniqueAgents.Contains(Agent))
            return false;
        UniqueAgents.Add(Agent);
    }

    TrainingAgents.Reset(InTrainingAgents.Num());
    for (ACMTetraPawn* Agent : InTrainingAgents)
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
    CurrentPolicyContentHash = InitialPolicyContentHash;
    LastCheckpointPolicyContentHash = InitialPolicyContentHash;
    bHasReceivedPolicyUpdate = false;

    TArray<UObject*> AgentObjects;
    AgentObjects.Reserve(TrainingAgents.Num());
    for (ACMTetraPawn* Agent : TrainingAgents)
        AgentObjects.Add(Agent);
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
    TotalAgentDecisionCount = TrainingAgentIds.Num();
    NextCheckpointDecisionCount = FMath::Max(CheckpointDecisionInterval, 100);
    NextSnapshotSaveTime = CurrentTime + FMath::Max(SnapshotSaveIntervalSeconds, 1.0f);
    LearningManager->SetComponentTickInterval(FMath::Max(DecisionInterval, 0.01f));
    LearningManager->SetComponentTickEnabled(true);
    GetWorldTimerManager().SetTimer(TrainingTimerHandle, this, &ThisClass::RunTrainingStep, FMath::Max(DecisionInterval, 0.01f), true);

    return true;
}

// 현재 레벨의 Tetra AI를 한 번 검색해 병렬 학습 에이전트로 전달한다.
bool ACMTetraLearningCoordinator::StartTrainingAllAgents()
{
    UWorld* World = GetWorld();
    if (!World)
        return false;

    TArray<ACMTetraPawn*> Agents;
    for (TActorIterator<ACMTetraPawn> It(World); It; ++It)
        Agents.Add(*It);

    return StartTrainingAgents(Agents);
}

// PPO 학습을 끝내고 최신 정책과 마지막 초기 체크포인트를 저장한다.
void ACMTetraLearningCoordinator::StopTraining()
{
    GetWorldTimerManager().ClearTimer(TrainingTimerHandle);
    if (LearningManager)
        LearningManager->SetComponentTickEnabled(false);

    const bool bWasTraining = PPOTrainer && PPOTrainer->IsTraining();
    if (bWasTraining)
    {
        PPOTrainer->EndTraining();
        RefreshPolicyUpdateState();
        SaveTrainingSnapshots();
        SaveTrainingCheckpoint();
    }

    for (ACMTetraPawn* Agent : TrainingAgents)
    {
        if (!Agent)
            continue;
        if (UCMAggressiveAccelerationMovementComponent* Movement = Agent->GetAccelerationMovement())
            Movement->StopMovement();
        if (UCMAggressiveOmnidirectionalPathComponent* PathMovement = Agent->GetPathMovement())
            PathMovement->SetPolicyControlEnabled(false);
    }
    if (LearningManager && LearningManager->GetAgentNum() > 0)
        LearningManager->RemoveAllAgents();
    TrainingAgentIds.Reset();
    TrainingAgents.Reset();
}

// 이어서 학습할 최신 Tetra AI 네트워크 네 개를 저장한다.
bool ACMTetraLearningCoordinator::SaveTrainingSnapshots()
{
    if (!Policy || !Critic)
        return false;

    RefreshPolicyUpdateState();
    if (!bHasReceivedPolicyUpdate)
        return false;

    const FString Directory = GetSnapshotDirectory();
    const bool bSaved = CMAggressiveLearningSnapshot::SaveTrainingNetworks(ECMAggressiveLearningSnapshotProfile::Tetra, *Policy, *Critic, Directory);
    return bSaved;
}

// 현재 정책을 판단 횟수별 불변 체크포인트 폴더에 한 번만 저장한다.
bool ACMTetraLearningCoordinator::SaveTrainingCheckpoint()
{
    if (!Policy || !Critic)
        return false;

    RefreshPolicyUpdateState();
    if (!bHasReceivedPolicyUpdate || CurrentPolicyContentHash == LastCheckpointPolicyContentHash)
        return false;

    const FString Directory = CMAggressiveLearningSnapshot::GetCheckpointDirectory(ECMAggressiveLearningSnapshotProfile::Tetra, TotalAgentDecisionCount);
    if (CMAggressiveLearningSnapshot::HasAnyTrainingSnapshots(ECMAggressiveLearningSnapshotProfile::Tetra, Directory))
        return CMAggressiveLearningSnapshot::HasCompleteTrainingSnapshots(ECMAggressiveLearningSnapshotProfile::Tetra, Directory);

    const bool bSaved = CMAggressiveLearningSnapshot::SaveTrainingNetworks(ECMAggressiveLearningSnapshotProfile::Tetra, *Policy, *Critic, Directory);
    if (bSaved)
    {
        LastCheckpointPolicyContentHash = CurrentPolicyContentHash;
    }
    return bSaved;
}

// 외부 PPO 프로세스가 정상 학습 중인지 반환한다.
bool ACMTetraLearningCoordinator::IsTraining() const
{
    return PPOTrainer && PPOTrainer->IsTraining() && !PPOTrainer->HasTrainingFailed();
}

// Tetra AI 학습 에이전트를 관리하는 중앙 Manager를 반환한다.
ULearningAgentsManager* ACMTetraLearningCoordinator::GetLearningManager() const
{
    return LearningManager;
}

// Tetra AI가 이어서 학습할 최신 스냅샷 폴더를 반환한다.
FString ACMTetraLearningCoordinator::GetSnapshotDirectory() const
{
    return CMAggressiveLearningSnapshot::GetLatestDirectory(ECMAggressiveLearningSnapshotProfile::Tetra);
}

// Tetra AI의 덮어쓰지 않는 체크포인트 루트 폴더를 반환한다.
FString ACMTetraLearningCoordinator::GetCheckpointDirectory() const
{
    return CMAggressiveLearningSnapshot::GetCheckpointRootDirectory(ECMAggressiveLearningSnapshotProfile::Tetra);
}

// 한 번의 PPO 업데이트가 수집할 최대 경험 스텝 수를 반환한다.
int32 ACMTetraLearningCoordinator::GetMaximumRecordedStepsPerIteration() const
{
    return MaximumRecordedStepsPerIteration;
}

// 자동 체크포인트 사이의 전체 에이전트 판단 수를 반환한다.
int32 ACMTetraLearningCoordinator::GetCheckpointDecisionInterval() const
{
    return CheckpointDecisionInterval;
}

// 현재 Tetra AI 정책에 등록된 에이전트 수를 반환한다.
int32 ACMTetraLearningCoordinator::GetTrainingAgentCount() const
{
    return TrainingAgentIds.Num();
}

// 외부 PPO 학습기로부터 새 정책을 한 번 이상 받았는지 반환한다.
bool ACMTetraLearningCoordinator::HasReceivedPolicyUpdate() const
{
    return bHasReceivedPolicyUpdate;
}

// 작은 무기억 정책과 Critic 및 PPO 트레이너를 Tetra AI 스키마로 생성한다.
bool ACMTetraLearningCoordinator::InitializeLearningObjects()
{
    ULearningAgentsManager* Manager = LearningManager;
    Interactor = UCMTetraLearningInteractor::MakeTetraInteractor(Manager);
    if (!Interactor)
        return false;

    ULearningAgentsInteractor* BaseInteractor = Interactor;
    const FLearningAgentsPolicySettings PolicySettings = UCMTetraLearningInteractor::GetPolicySettings();
    Policy = ULearningAgentsPolicy::MakePolicy(Manager, BaseInteractor, ULearningAgentsPolicy::StaticClass(), TEXT("TetraPolicy"), nullptr, nullptr, nullptr, true, true, true, PolicySettings);
    if (!Policy)
        return false;

    TrainingEnvironment = UCMAggressiveLearningTrainingEnvironment::MakeAggressiveTrainingEnvironment(Manager, RewardSettings, GoalSettings, TEXT("TetraTrainingEnvironment"));
    if (!TrainingEnvironment)
        return false;

    ULearningAgentsPolicy* BasePolicy = Policy;
    FLearningAgentsCriticSettings CriticSettings;
    CriticSettings.HiddenLayerNum = 1;
    CriticSettings.HiddenLayerSize = 32;
    Critic = ULearningAgentsCritic::MakeCritic(Manager, BaseInteractor, BasePolicy, ULearningAgentsCritic::StaticClass(), TEXT("TetraCritic"), nullptr, true, CriticSettings);
    if (!Critic)
        return false;

    const FString LatestDirectory = GetSnapshotDirectory();
    if (bResumeExistingSnapshots && CMAggressiveLearningSnapshot::HasAnyTrainingSnapshots(ECMAggressiveLearningSnapshotProfile::Tetra, LatestDirectory))
    {
        if (!CMAggressiveLearningSnapshot::HasCompleteTrainingSnapshots(ECMAggressiveLearningSnapshotProfile::Tetra, LatestDirectory))
            return false;
        if (!CMAggressiveLearningSnapshot::LoadTrainingNetworks(ECMAggressiveLearningSnapshotProfile::Tetra, *Policy, *Critic, LatestDirectory))
            return false;
    }

    const FLearningAgentsCommunicator Communicator = ULearningAgentsCommunicatorLibrary::MakeSharedMemoryTrainingProcess();
    ULearningAgentsTrainingEnvironment* BaseEnvironment = TrainingEnvironment;
    ULearningAgentsCritic* BaseCritic = Critic;
    FLearningAgentsPPOTrainerSettings TrainerSettings;
    TrainerSettings.MaximumRecordedStepsPerIteration = FMath::Max(MaximumRecordedStepsPerIteration, 100);
    PPOTrainer = ULearningAgentsPPOTrainer::MakePPOTrainer(Manager, BaseInteractor, BaseEnvironment, BasePolicy, BaseCritic, Communicator, ULearningAgentsPPOTrainer::StaticClass(), TEXT("TetraPPOTrainer"), TrainerSettings);

    return PPOTrainer != nullptr;
}

// 초기 정책과 현재 정책 해시가 달라졌으면 PPO 업데이트 수신 상태를 갱신한다.
void ACMTetraLearningCoordinator::RefreshPolicyUpdateState()
{
    if (!Policy)
        return;

    ULearningAgentsNeuralNetwork* PolicyNetwork = Policy->GetPolicyNetworkAsset();
    if (!PolicyNetwork || !PolicyNetwork->NeuralNetworkData)
        return;
    CurrentPolicyContentHash = PolicyNetwork->NeuralNetworkData->GetContentHash();
    bHasReceivedPolicyUpdate = bHasReceivedPolicyUpdate || CurrentPolicyContentHash != InitialPolicyContentHash;
}

// 현재 경험을 PPO에 전달하고 로그와 두 종류의 저장 시점을 확인한다.
void ACMTetraLearningCoordinator::RunTrainingStep()
{
    if (!PPOTrainer || PPOTrainer->HasTrainingFailed())
    {
        StopTraining();

        return;
    }

    PPOTrainer->RunTraining();
    DrawTrainingGoals();
    TotalAgentDecisionCount += TrainingAgentIds.Num();
    RefreshPolicyUpdateState();
    SaveTrainingSnapshotsIfNeeded();
    SaveTrainingCheckpointIfNeeded();
}

// 학습 중 각 Tetra AI가 현재 향하는 목적지와 도착 허용 반경을 표시한다.
void ACMTetraLearningCoordinator::DrawTrainingGoals() const
{
    UWorld* World = GetWorld();
    if (!bDrawTrainingGoal || !World)
        return;

    const float Lifetime = FMath::Max(DecisionInterval * 1.5f, 0.05f);
    for (const ACMTetraPawn* Agent : TrainingAgents)
    {
        const UCMAggressiveMovementCommandComponent* MovementCommand = Agent ? Agent->FindComponentByClass<UCMAggressiveMovementCommandComponent>() : nullptr;
        if (!MovementCommand || !MovementCommand->HasMovementGoal())
            continue;

        const FCMAggressiveMovementGoal Goal = MovementCommand->GetMovementGoal();
        DrawDebugSphere(World, Goal.WorldLocation, FMath::Max(Goal.AcceptanceRadius, 10.0f), 24, FColor::Green, false, Lifetime, 0, 4.0f);
    }
}

// 설정된 시간이 되면 이어서 학습할 최신 네트워크를 저장한다.
void ACMTetraLearningCoordinator::SaveTrainingSnapshotsIfNeeded()
{
    const double CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime < NextSnapshotSaveTime)
        return;
    NextSnapshotSaveTime = CurrentTime + FMath::Max(SnapshotSaveIntervalSeconds, 1.0f);
    SaveTrainingSnapshots();
}

// 설정된 전체 판단 수를 넘고 새 정책이 있으면 불변 체크포인트를 저장한다.
void ACMTetraLearningCoordinator::SaveTrainingCheckpointIfNeeded()
{
    if (TotalAgentDecisionCount < NextCheckpointDecisionCount)
        return;
    if (!SaveTrainingCheckpoint())
        return;

    const int64 SafeInterval = FMath::Max(CheckpointDecisionInterval, 100);
    while (NextCheckpointDecisionCount <= TotalAgentDecisionCount)
        NextCheckpointDecisionCount += SafeInterval;
}
