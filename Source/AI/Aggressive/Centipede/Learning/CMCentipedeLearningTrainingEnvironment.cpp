#include "Aggressive/Centipede/Learning/CMCentipedeLearningTrainingEnvironment.h"

#include "Aggressive/Centipede/CMCentipedePawn.h"
#include "Components/BoxComponent.h"
#include "LearningAgentsManager.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMCentipedeEpisode, Log, All);

UCMCentipedeLearningTrainingEnvironment* UCMCentipedeLearningTrainingEnvironment::MakeCentipedeTrainingEnvironment(
    ULearningAgentsManager*& InManager,
    FCMAggressiveLearningRewardSettings InRewardSettings,
    FCMAggressiveLearningGoalSettings InGoalSettings,
    float InJointTrackingPenaltyScale,
    FName Name)
{
    if (!InManager)
        return nullptr;
    UCMCentipedeLearningTrainingEnvironment* Environment = NewObject<UCMCentipedeLearningTrainingEnvironment>(InManager, StaticClass(), MakeUniqueObjectName(InManager, StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique));
    if (!Environment)
        return nullptr;
    const int32 MaxAgentNum = InManager->GetMaxAgentNum();
    Environment->RewardSettings = InRewardSettings;
    Environment->GoalSettings = InGoalSettings;
    Environment->JointTrackingPenaltyScale = FMath::Max(InJointTrackingPenaltyScale, 0.0f);
    Environment->InitialHeadTransforms.SetNum(MaxAgentNum);
    Environment->PreviousGoalLocations.SetNum(MaxAgentNum);
    Environment->PreviousGoalDistances.SetNumZeroed(MaxAgentNum);
    Environment->NextDirectionIndices.SetNumZeroed(MaxAgentNum);
    Environment->EpisodeNumbers.SetNumZeroed(MaxAgentNum);
    Environment->PendingEndReasons.Init(ECMAggressiveLearningEpisodeEndReason::None, MaxAgentNum);
    Environment->HasInitialState.Init(false, MaxAgentNum);
    Environment->HasPreviousState.Init(false, MaxAgentNum);
    Environment->SetupTrainingEnvironment(InManager);
    return Environment->IsSetup() ? Environment : nullptr;
}

void UCMCentipedeLearningTrainingEnvironment::OnAgentsAdded_Implementation(const TArray<int32>& AgentIds)
{
    Super::OnAgentsAdded_Implementation(AgentIds);
    for (const int32 AgentId : AgentIds)
        CaptureInitialState(AgentId);
}

void UCMCentipedeLearningTrainingEnvironment::OnAgentsRemoved_Implementation(const TArray<int32>& AgentIds)
{
    Super::OnAgentsRemoved_Implementation(AgentIds);
    for (const int32 AgentId : AgentIds)
    {
        if (HasInitialState.IsValidIndex(AgentId))
            HasInitialState[AgentId] = false;
        if (HasPreviousState.IsValidIndex(AgentId))
            HasPreviousState[AgentId] = false;
        if (PendingEndReasons.IsValidIndex(AgentId))
            PendingEndReasons[AgentId] = ECMAggressiveLearningEpisodeEndReason::None;
    }
}

void UCMCentipedeLearningTrainingEnvironment::GatherAgentReward_Implementation(float& OutReward, int32 AgentId)
{
    OutReward = 0.0f;
    ACMCentipedePawn* Agent = GetCentipedeAgent(AgentId);
    UCMAggressiveMovementCommandComponent* Command = Agent ? Agent->GetMovementCommand() : nullptr;
    UBoxComponent* Head = Agent ? Agent->GetHeadBody() : nullptr;
    if (!Agent || !Command || !Head || !Command->HasMovementGoal() || !HasPreviousState.IsValidIndex(AgentId))
        return;

    Agent->RefreshJointTargets();
    const FCMAggressiveMovementGoal Goal = Command->GetMovementGoal();
    const float CurrentDistance = FVector::Dist2D(Head->GetComponentLocation(), Goal.WorldLocation);
    if (!HasPreviousState[AgentId] || !PreviousGoalLocations[AgentId].Equals(Goal.WorldLocation, UE_KINDA_SMALL_NUMBER))
    {
        UpdatePreviousState(AgentId);
        return;
    }

    OutReward = CMAggressiveLearningReward::CalculateMovementReward(
        PreviousGoalDistances[AgentId],
        CurrentDistance,
        Head->GetPhysicsAngularVelocityInRadians().Z,
        Command->HasReachedMovementGoal(),
        GetEpisodeTime(AgentId),
        RewardSettings);
    OutReward -= Agent->GetMeanNormalizedJointError() * JointTrackingPenaltyScale;
    PreviousGoalDistances[AgentId] = CurrentDistance;
}

void UCMCentipedeLearningTrainingEnvironment::GatherAgentCompletion_Implementation(ELearningAgentsCompletion& OutCompletion, int32 AgentId)
{
    ACMCentipedePawn* Agent = GetCentipedeAgent(AgentId);
    UCMAggressiveMovementCommandComponent* Command = Agent ? Agent->GetMovementCommand() : nullptr;
    if (!Agent || !Command)
    {
        OutCompletion = ELearningAgentsCompletion::Termination;
        return;
    }
    const bool bReachedGoal = Command->HasMovementGoal() && Command->HasReachedMovementGoal();
    const float UprightDot = Agent->GetMinimumSegmentUprightDot();
    const float EpisodeTime = GetEpisodeTime(AgentId);
    OutCompletion = CMAggressiveLearningReward::ResolveEpisodeCompletion(bReachedGoal, UprightDot, EpisodeTime, RewardSettings);
    if (OutCompletion != ELearningAgentsCompletion::Running && PendingEndReasons.IsValidIndex(AgentId))
        PendingEndReasons[AgentId] = CMAggressiveLearningEpisode::ResolveEndReason(bReachedGoal, UprightDot, EpisodeTime, RewardSettings);
}

void UCMCentipedeLearningTrainingEnvironment::ResetAgentEpisode_Implementation(int32 AgentId)
{
    ACMCentipedePawn* Agent = GetCentipedeAgent(AgentId);
    if (!Agent || !HasInitialState.IsValidIndex(AgentId) || !HasInitialState[AgentId])
        return;
    RecordPendingEpisodeResult(AgentId);
    Agent->ResetArticulatedBody(InitialHeadTransforms[AgentId]);
    SetNextGoal(AgentId);
    UpdatePreviousState(AgentId);
}

ACMCentipedePawn* UCMCentipedeLearningTrainingEnvironment::GetCentipedeAgent(int32 AgentId)
{
    return Cast<ACMCentipedePawn>(GetAgent(AgentId));
}

float UCMCentipedeLearningTrainingEnvironment::GetSuccessRate() const
{
    return TotalCompletedEpisodeCount > 0
        ? static_cast<float>(TotalSuccessfulEpisodeCount) / static_cast<float>(TotalCompletedEpisodeCount)
        : 0.0f;
}

void UCMCentipedeLearningTrainingEnvironment::CaptureInitialState(int32 AgentId)
{
    ACMCentipedePawn* Agent = GetCentipedeAgent(AgentId);
    UBoxComponent* Head = Agent ? Agent->GetHeadBody() : nullptr;
    if (!Agent || !Head || !InitialHeadTransforms.IsValidIndex(AgentId))
        return;
    InitialHeadTransforms[AgentId] = Head->GetComponentTransform();
    HasInitialState[AgentId] = true;
    NextDirectionIndices[AgentId] = AgentId % 8;
    EpisodeNumbers[AgentId] = 0;
    PendingEndReasons[AgentId] = ECMAggressiveLearningEpisodeEndReason::None;
    Agent->ResetArticulatedBody(InitialHeadTransforms[AgentId]);
    SetNextGoal(AgentId);
    UpdatePreviousState(AgentId);
}

void UCMCentipedeLearningTrainingEnvironment::RecordPendingEpisodeResult(int32 AgentId)
{
    if (!PendingEndReasons.IsValidIndex(AgentId))
        return;
    const ECMAggressiveLearningEpisodeEndReason EndReason = PendingEndReasons[AgentId];
    PendingEndReasons[AgentId] = ECMAggressiveLearningEpisodeEndReason::None;
    if (EndReason == ECMAggressiveLearningEpisodeEndReason::None || EndReason == ECMAggressiveLearningEpisodeEndReason::PolicyUpdate)
        return;

    ++TotalCompletedEpisodeCount;
    if (EndReason == ECMAggressiveLearningEpisodeEndReason::Arrival)
        ++TotalSuccessfulEpisodeCount;
    UE_LOG(LogCMCentipedeEpisode, Display,
        TEXT("Centipede AI 학습 결과 - 에이전트=%d 에피소드=%d 완료=%lld 성공=%lld 성공률=%.1f%%"),
        AgentId,
        EpisodeNumbers.IsValidIndex(AgentId) ? EpisodeNumbers[AgentId] : 0,
        TotalCompletedEpisodeCount,
        TotalSuccessfulEpisodeCount,
        GetSuccessRate() * 100.0f);
}

void UCMCentipedeLearningTrainingEnvironment::SetNextGoal(int32 AgentId)
{
    ACMCentipedePawn* Agent = GetCentipedeAgent(AgentId);
    UCMAggressiveMovementCommandComponent* Command = Agent ? Agent->GetMovementCommand() : nullptr;
    if (!Agent || !Command || !NextDirectionIndices.IsValidIndex(AgentId) || !InitialHeadTransforms.IsValidIndex(AgentId))
        return;
    const int32 DirectionIndex = NextDirectionIndices[AgentId] % 8;
    const ECMAggressiveMoveDirection Direction = static_cast<ECMAggressiveMoveDirection>(DirectionIndex);
    Command->SetMovementGoal(CMAggressiveLearningGoal::ResolveWorldGoalLocation(InitialHeadTransforms[AgentId], Direction, GoalSettings.GoalDistance), GoalSettings.AcceptanceRadius);
    Agent->SetTrainingCurveProfile(EpisodeNumbers[AgentId] % 4);
    ++EpisodeNumbers[AgentId];
    NextDirectionIndices[AgentId] = (DirectionIndex + 1) % 8;
}

void UCMCentipedeLearningTrainingEnvironment::UpdatePreviousState(int32 AgentId)
{
    ACMCentipedePawn* Agent = GetCentipedeAgent(AgentId);
    UCMAggressiveMovementCommandComponent* Command = Agent ? Agent->GetMovementCommand() : nullptr;
    UBoxComponent* Head = Agent ? Agent->GetHeadBody() : nullptr;
    if (!Agent || !Command || !Head || !HasPreviousState.IsValidIndex(AgentId))
        return;
    HasPreviousState[AgentId] = Command->HasMovementGoal();
    if (!HasPreviousState[AgentId])
        return;
    PreviousGoalLocations[AgentId] = Command->GetMovementGoal().WorldLocation;
    PreviousGoalDistances[AgentId] = FVector::Dist2D(Head->GetComponentLocation(), PreviousGoalLocations[AgentId]);
}
