#include "Aggressive/Common/Learning/CMAggressiveLearningTrainingEnvironment.h"

#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "LearningAgentsManager.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMAggressiveLearningEpisode, Log, All);

namespace
{
    // 두 위치 사이의 높이 차이를 제외한 거리를 반환한다.
    float CalculatePlanarDistance(const FVector& FirstLocation, const FVector& SecondLocation)
    {
        const FVector Difference = SecondLocation - FirstLocation;
        return FVector2D(Difference.X, Difference.Y).Size();
    }

    // 몸통 전방과 목표 방향이 평면에서 정렬된 정도를 -1부터 1까지 반환한다.
    float CalculateFacingAlignment(const UPrimitiveComponent& Body, const FVector& GoalLocation)
    {
        FVector ForwardDirection = Body.GetForwardVector();
        ForwardDirection.Z = 0.0f;
        FVector GoalDirection = GoalLocation - Body.GetComponentLocation();
        GoalDirection.Z = 0.0f;
        if (ForwardDirection.IsNearlyZero() || GoalDirection.IsNearlyZero())
            return 1.0f;
        return FVector::DotProduct(ForwardDirection.GetSafeNormal(), GoalDirection.GetSafeNormal());
    }

    // 공격적 AI 에피소드 종료 원인을 로그에 사용할 한글 이름으로 변환한다.
    const TCHAR* GetEpisodeEndReasonName(ECMAggressiveLearningEpisodeEndReason Reason)
    {
        switch (Reason)
        {
        case ECMAggressiveLearningEpisodeEndReason::Arrival:
            return TEXT("도착");
        case ECMAggressiveLearningEpisodeEndReason::Overturned:
            return TEXT("전복");
        case ECMAggressiveLearningEpisodeEndReason::Timeout:
            return TEXT("시간 초과");
        case ECMAggressiveLearningEpisodeEndReason::PolicyUpdate:
            return TEXT("정책 업데이트");
        default:
            return TEXT("없음");
        }
    }
} // namespace

// 목표 접근량과 과회전 및 남은 제한시간으로 한 판단 단계의 보상을 계산한다.
float CMAggressiveLearningReward::CalculateMovementReward(float PreviousDistance, float CurrentDistance, float YawAngularVelocity, bool bReachedGoal, float EpisodeTime, const FCMAggressiveLearningRewardSettings& Settings)
{
    const float SafeDistanceScale = FMath::Max(Settings.ProgressDistanceScale, UE_KINDA_SMALL_NUMBER);
    const float ProgressReward = (PreviousDistance - CurrentDistance) / SafeDistanceScale;
    const float RotationPenalty = FMath::Abs(YawAngularVelocity) * FMath::Max(Settings.YawAngularVelocityPenaltyScale, 0.0f);
    const float TimePenalty = FMath::Max(Settings.StepPenalty, 0.0f);
    const float GoalReward = bReachedGoal ? FMath::Max(Settings.ArrivalReward, 0.0f) : 0.0f;
    const float RemainingTimeRatio = 1.0f - FMath::Clamp(EpisodeTime / FMath::Max(Settings.MaxEpisodeSeconds, 0.1f), 0.0f, 1.0f);
    const float FastArrivalReward = bReachedGoal ? FMath::Max(Settings.MaximumFastArrivalReward, 0.0f) * RemainingTimeRatio : 0.0f;
    return ProgressReward + GoalReward + FastArrivalReward - RotationPenalty - TimePenalty;
}

// 목표 방향 정렬도가 증가하면 양수이고 감소하면 음수인 회전 진행 보상을 반환한다.
float CMAggressiveLearningReward::CalculateFacingProgressReward(float PreviousAlignment, float CurrentAlignment, const FCMAggressiveLearningRewardSettings& Settings)
{
    const float SafePreviousAlignment = FMath::Clamp(PreviousAlignment, -1.0f, 1.0f);
    const float SafeCurrentAlignment = FMath::Clamp(CurrentAlignment, -1.0f, 1.0f);

    return (SafeCurrentAlignment - SafePreviousAlignment) * FMath::Max(Settings.FacingProgressRewardScale, 0.0f);
}

// 목표 도착, 전복, 제한시간 순서로 에피소드 완료 상태를 판정한다.
ELearningAgentsCompletion CMAggressiveLearningReward::ResolveEpisodeCompletion(bool bReachedGoal, float UprightDot, float EpisodeTime, const FCMAggressiveLearningRewardSettings& Settings)
{
    const ECMAggressiveLearningEpisodeEndReason EndReason = CMAggressiveLearningEpisode::ResolveEndReason(bReachedGoal, UprightDot, EpisodeTime, Settings);
    if (EndReason == ECMAggressiveLearningEpisodeEndReason::Arrival || EndReason == ECMAggressiveLearningEpisodeEndReason::Overturned)
        return ELearningAgentsCompletion::Termination;
    if (EndReason == ECMAggressiveLearningEpisodeEndReason::Timeout)
        return ELearningAgentsCompletion::Truncation;
    return ELearningAgentsCompletion::Running;
}

// 목표 도착과 전복 및 제한시간 순서로 에피소드 종료 원인을 판정한다.
ECMAggressiveLearningEpisodeEndReason CMAggressiveLearningEpisode::ResolveEndReason(bool bReachedGoal, float UprightDot, float EpisodeTime, const FCMAggressiveLearningRewardSettings& Settings)
{
    if (bReachedGoal)
        return ECMAggressiveLearningEpisodeEndReason::Arrival;
    if (UprightDot < FMath::Clamp(Settings.MinimumUprightDot, -1.0f, 1.0f))
        return ECMAggressiveLearningEpisodeEndReason::Overturned;
    if (EpisodeTime >= FMath::Max(Settings.MaxEpisodeSeconds, 0.1f))
        return ECMAggressiveLearningEpisodeEndReason::Timeout;
    return ECMAggressiveLearningEpisodeEndReason::None;
}

// 최근 자연 종료 에피소드 중 목표에 도착한 비율을 반환한다.
float CMAggressiveLearningEpisode::CalculateArrivalRate(const TArray<uint8>& ArrivalHistory)
{
    if (ArrivalHistory.IsEmpty())
        return 0.0f;

    int32 ArrivalCount = 0;
    for (const uint8 bArrived : ArrivalHistory)
    {
        if (bArrived != 0)
            ++ArrivalCount;
    }
    return static_cast<float>(ArrivalCount) / static_cast<float>(ArrivalHistory.Num());
}

// 시작 몸통의 Yaw를 기준으로 로컬 8방향 목표를 월드 평면 위치로 변환한다.
FVector CMAggressiveLearningGoal::ResolveWorldGoalLocation(const FTransform& StartTransform, ECMAggressiveMoveDirection Direction, float GoalDistance)
{
    const FVector LocalDirection = CMAggressiveDirection::ToLocalUnitVector(Direction);
    const float BodyYawRadians = FMath::DegreesToRadians(StartTransform.Rotator().Yaw);
    const FQuat BodyYawRotation(FVector::UpVector, BodyYawRadians);

    return StartTransform.GetLocation() + BodyYawRotation.RotateVector(LocalDirection) * FMath::Max(GoalDistance, 0.0f);
}

// 지정한 보상 설정과 에이전트 수에 맞는 학습 환경을 생성한다.
UCMAggressiveLearningTrainingEnvironment* UCMAggressiveLearningTrainingEnvironment::MakeAggressiveTrainingEnvironment(ULearningAgentsManager*& InManager, FCMAggressiveLearningRewardSettings InRewardSettings, FCMAggressiveLearningGoalSettings InGoalSettings, FName Name)
{
    if (!InManager)
        return nullptr;

    const FName UniqueName = MakeUniqueObjectName(InManager, StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);
    UCMAggressiveLearningTrainingEnvironment* TrainingEnvironment = NewObject<UCMAggressiveLearningTrainingEnvironment>(InManager, StaticClass(), UniqueName);
    if (!TrainingEnvironment)
        return nullptr;

    const int32 MaxAgentNum = InManager->GetMaxAgentNum();
    TrainingEnvironment->RewardSettings = InRewardSettings;
    TrainingEnvironment->GoalSettings = InGoalSettings;
    TrainingEnvironment->InitialBodyTransforms.SetNum(MaxAgentNum);
    TrainingEnvironment->PreviousGoalLocations.SetNum(MaxAgentNum);
    TrainingEnvironment->PreviousGoalDistances.SetNumZeroed(MaxAgentNum);
    TrainingEnvironment->PreviousFacingAlignments.SetNumZeroed(MaxAgentNum);
    TrainingEnvironment->NextGoalDirectionIndices.SetNumZeroed(MaxAgentNum);
    TrainingEnvironment->EpisodeNumbers.SetNumZeroed(MaxAgentNum);
    TrainingEnvironment->EpisodeStartDirections.Init(ECMAggressiveMoveDirection::None, MaxAgentNum);
    TrainingEnvironment->EpisodeCumulativeRewards.SetNumZeroed(MaxAgentNum);
    TrainingEnvironment->EpisodeMinimumDistances.Init(TNumericLimits<float>::Max(), MaxAgentNum);
    TrainingEnvironment->EpisodeStepCounts.SetNumZeroed(MaxAgentNum);
    TrainingEnvironment->PendingEndReasons.Init(ECMAggressiveLearningEpisodeEndReason::None, MaxAgentNum);
    TrainingEnvironment->RecentArrivalHistories.SetNum(MaxAgentNum);
    TrainingEnvironment->TotalNaturalEpisodeCounts.SetNumZeroed(MaxAgentNum);
    TrainingEnvironment->TotalArrivalCounts.SetNumZeroed(MaxAgentNum);
    TrainingEnvironment->HasInitialBodyTransform.Init(false, MaxAgentNum);
    TrainingEnvironment->HasPreviousGoalState.Init(false, MaxAgentNum);
    TrainingEnvironment->SetupTrainingEnvironment(InManager);

    return TrainingEnvironment->IsSetup() ? TrainingEnvironment : nullptr;
}

// 추가된 에이전트의 에피소드 시작 물리 상태와 목표 거리를 저장한다.
void UCMAggressiveLearningTrainingEnvironment::OnAgentsAdded_Implementation(const TArray<int32>& AgentIds)
{
    Super::OnAgentsAdded_Implementation(AgentIds);
    for (const int32 AgentId : AgentIds)
        CaptureInitialAgentState(AgentId);
}

// 제거된 에이전트가 사용하던 학습 상태를 비운다.
void UCMAggressiveLearningTrainingEnvironment::OnAgentsRemoved_Implementation(const TArray<int32>& AgentIds)
{
    Super::OnAgentsRemoved_Implementation(AgentIds);
    for (const int32 AgentId : AgentIds)
    {
        if (!HasInitialBodyTransform.IsValidIndex(AgentId) || !HasPreviousGoalState.IsValidIndex(AgentId))
            continue;
        HasInitialBodyTransform[AgentId] = false;
        HasPreviousGoalState[AgentId] = false;
        NextGoalDirectionIndices[AgentId] = 0;
        EpisodeNumbers[AgentId] = 0;
        RecentArrivalHistories[AgentId].Reset();
        TotalNaturalEpisodeCounts[AgentId] = 0;
        TotalArrivalCounts[AgentId] = 0;
        ResetEpisodeStatistics(AgentId);
    }
}

// 현재 목표까지의 거리 변화와 몸통 회전량으로 이동 보상을 수집한다.
void UCMAggressiveLearningTrainingEnvironment::GatherAgentReward_Implementation(float& OutReward, int32 AgentId)
{
    OutReward = 0.0f;
    AActor* Agent = nullptr;
    UPrimitiveComponent* Body = nullptr;
    UCMAggressiveMovementCommandComponent* MovementCommand = nullptr;
    if (!TryGetAgentComponents(AgentId, Agent, Body, MovementCommand) || !MovementCommand->HasMovementGoal() || !HasPreviousGoalState.IsValidIndex(AgentId))
    {
        return;
    }

    const FCMAggressiveMovementGoal Goal = MovementCommand->GetMovementGoal();
    const float CurrentDistance = CalculatePlanarDistance(Body->GetComponentLocation(), Goal.WorldLocation);
    const float CurrentFacingAlignment = CalculateFacingAlignment(*Body, Goal.WorldLocation);
    if (!HasPreviousGoalState[AgentId] || !PreviousGoalLocations[AgentId].Equals(Goal.WorldLocation, UE_KINDA_SMALL_NUMBER))
    {
        UpdatePreviousGoalState(AgentId, *Body, *MovementCommand);

        return;
    }

    const float YawAngularVelocity = Body->GetPhysicsAngularVelocityInRadians().Z;
    OutReward = CMAggressiveLearningReward::CalculateMovementReward(PreviousGoalDistances[AgentId], CurrentDistance, YawAngularVelocity, MovementCommand->HasReachedMovementGoal(), GetEpisodeTime(AgentId), RewardSettings);
    if (PreviousFacingAlignments.IsValidIndex(AgentId))
    {
        OutReward += CMAggressiveLearningReward::CalculateFacingProgressReward(PreviousFacingAlignments[AgentId], CurrentFacingAlignment, RewardSettings);
        PreviousFacingAlignments[AgentId] = CurrentFacingAlignment;
    }
    PreviousGoalDistances[AgentId] = CurrentDistance;
    if (EpisodeCumulativeRewards.IsValidIndex(AgentId) && EpisodeMinimumDistances.IsValidIndex(AgentId) && EpisodeStepCounts.IsValidIndex(AgentId))
    {
        EpisodeCumulativeRewards[AgentId] += OutReward;
        EpisodeMinimumDistances[AgentId] = FMath::Min(EpisodeMinimumDistances[AgentId], CurrentDistance);
        ++EpisodeStepCounts[AgentId];
    }
}

// 목표 도착과 몸통 전복 및 학습 제한시간으로 완료 상태를 수집한다.
void UCMAggressiveLearningTrainingEnvironment::GatherAgentCompletion_Implementation(ELearningAgentsCompletion& OutCompletion, int32 AgentId)
{
    AActor* Agent = nullptr;
    UPrimitiveComponent* Body = nullptr;
    UCMAggressiveMovementCommandComponent* MovementCommand = nullptr;
    if (!TryGetAgentComponents(AgentId, Agent, Body, MovementCommand))
    {
        OutCompletion = ELearningAgentsCompletion::Termination;
        return;
    }

    const bool bReachedGoal = MovementCommand->HasMovementGoal() && MovementCommand->HasReachedMovementGoal();
    const float UprightDot = FVector::DotProduct(Body->GetUpVector(), FVector::UpVector);
    const float EpisodeTime = GetEpisodeTime(AgentId);
    OutCompletion = CMAggressiveLearningReward::ResolveEpisodeCompletion(bReachedGoal, UprightDot, EpisodeTime, RewardSettings);
    if (PendingEndReasons.IsValidIndex(AgentId))
        PendingEndReasons[AgentId] = CMAggressiveLearningEpisode::ResolveEndReason(bReachedGoal, UprightDot, EpisodeTime, RewardSettings);
}

// 에이전트 몸통과 다리 쿨다운을 에피소드 시작 상태로 되돌린다.
void UCMAggressiveLearningTrainingEnvironment::ResetAgentEpisode_Implementation(int32 AgentId)
{
    if (!HasInitialBodyTransform.IsValidIndex(AgentId) || !HasInitialBodyTransform[AgentId])
        return;

    AActor* Agent = nullptr;
    UPrimitiveComponent* Body = nullptr;
    UCMAggressiveMovementCommandComponent* MovementCommand = nullptr;
    if (!TryGetAgentComponents(AgentId, Agent, Body, MovementCommand))
        return;

    LogEpisodeSummary(AgentId, *Body, *MovementCommand);
    Body->SetWorldTransform(InitialBodyTransforms[AgentId], false, nullptr, ETeleportType::TeleportPhysics);
    Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    Body->WakeAllRigidBodies();
    if (ICMAggressiveLegActuationAgent* LegAgent = Cast<ICMAggressiveLegActuationAgent>(Agent))
        LegAgent->ResetLegActuation();
    SetNextTrainingGoal(AgentId, *MovementCommand);
    UpdatePreviousGoalState(AgentId, *Body, *MovementCommand);
}

// 종료된 에피소드의 학습 품질 통계를 한 줄로 출력한다.
void UCMAggressiveLearningTrainingEnvironment::LogEpisodeSummary(int32 AgentId, UPrimitiveComponent& Body, UCMAggressiveMovementCommandComponent& MovementCommand)
{
    if (!EpisodeStepCounts.IsValidIndex(AgentId) || EpisodeStepCounts[AgentId] <= 0 || !PendingEndReasons.IsValidIndex(AgentId) || !RecentArrivalHistories.IsValidIndex(AgentId))
    {
        return;
    }

    ECMAggressiveLearningEpisodeEndReason EndReason = PendingEndReasons[AgentId];
    if (EndReason == ECMAggressiveLearningEpisodeEndReason::None)
        EndReason = ECMAggressiveLearningEpisodeEndReason::PolicyUpdate;

    TArray<uint8>& ArrivalHistory = RecentArrivalHistories[AgentId];
    if (EndReason != ECMAggressiveLearningEpisodeEndReason::PolicyUpdate)
    {
        ArrivalHistory.Add(EndReason == ECMAggressiveLearningEpisodeEndReason::Arrival ? 1 : 0);
        ++TotalNaturalEpisodeCounts[AgentId];
        if (EndReason == ECMAggressiveLearningEpisodeEndReason::Arrival)
            ++TotalArrivalCounts[AgentId];
        if (ArrivalHistory.Num() > 8)
            ArrivalHistory.RemoveAt(0, ArrivalHistory.Num() - 8, EAllowShrinking::No);
    }

    const float EndDistance = MovementCommand.HasMovementGoal() ? CalculatePlanarDistance(Body.GetComponentLocation(), MovementCommand.GetMovementGoal().WorldLocation) : 0.0f;
    const float EpisodeTime = GetEpisodeTime(AgentId);
    const float RecentArrivalRate = CMAggressiveLearningEpisode::CalculateArrivalRate(ArrivalHistory) * 100.0f;
    const float TotalArrivalRate = TotalNaturalEpisodeCounts[AgentId] > 0 ? static_cast<float>(TotalArrivalCounts[AgentId]) / static_cast<float>(TotalNaturalEpisodeCounts[AgentId]) * 100.0f : 0.0f;
    UE_LOG(
        LogCMAggressiveLearningEpisode,
        Display,
        TEXT("공격적 AI 학습 에피소드 종료 - 에이전트: %d, 번호: %lld, 시작 방향: %s, 판단: %d, 경과: %.2f초, 누적 보상: %.4f, 최소 거리: %.1fcm, 종료 거리: %.1fcm, 종료 이유: %s, 최근 %d회 도착률: %.1f%%, 전체 %lld회 도착률: %.1f%%"),
        AgentId,
        EpisodeNumbers[AgentId],
        CMAggressiveDirection::GetKoreanDisplayName(EpisodeStartDirections[AgentId]),
        EpisodeStepCounts[AgentId],
        EpisodeTime,
        EpisodeCumulativeRewards[AgentId],
        EpisodeMinimumDistances[AgentId],
        EndDistance,
        GetEpisodeEndReasonName(EndReason),
        ArrivalHistory.Num(),
        RecentArrivalRate,
        TotalNaturalEpisodeCounts[AgentId],
        TotalArrivalRate
    );
}

// 다음 에피소드가 사용할 누적 통계와 종료 원인을 초기화한다.
void UCMAggressiveLearningTrainingEnvironment::ResetEpisodeStatistics(int32 AgentId)
{
    if (!EpisodeCumulativeRewards.IsValidIndex(AgentId) || !EpisodeMinimumDistances.IsValidIndex(AgentId) || !EpisodeStepCounts.IsValidIndex(AgentId) || !PendingEndReasons.IsValidIndex(AgentId))
    {
        return;
    }

    EpisodeCumulativeRewards[AgentId] = 0.0f;
    EpisodeMinimumDistances[AgentId] = FMath::Max(GoalSettings.GoalDistance, 0.0f);
    EpisodeStepCounts[AgentId] = 0;
    PendingEndReasons[AgentId] = ECMAggressiveLearningEpisodeEndReason::None;
}

// 에이전트 번호로 공격적 AI의 몸통과 이동 명령 컴포넌트를 가져온다.
bool UCMAggressiveLearningTrainingEnvironment::TryGetAgentComponents(int32 AgentId, AActor*& OutAgent, UPrimitiveComponent*& OutBody, UCMAggressiveMovementCommandComponent*& OutMovementCommand)
{
    OutAgent = Cast<AActor>(GetAgent(AgentId));
    ICMAggressiveMovementAgent* MovementAgent = Cast<ICMAggressiveMovementAgent>(OutAgent);
    OutBody = MovementAgent ? MovementAgent->GetAggressiveMovementBody() : nullptr;
    OutMovementCommand = OutAgent ? OutAgent->FindComponentByClass<UCMAggressiveMovementCommandComponent>() : nullptr;
    return OutAgent && OutBody && OutMovementCommand;
}

// 에이전트의 현재 몸통 위치를 이후 에피소드가 돌아갈 시작 상태로 저장한다.
void UCMAggressiveLearningTrainingEnvironment::CaptureInitialAgentState(int32 AgentId)
{
    if (!HasInitialBodyTransform.IsValidIndex(AgentId))
        return;

    AActor* Agent = nullptr;
    UPrimitiveComponent* Body = nullptr;
    UCMAggressiveMovementCommandComponent* MovementCommand = nullptr;
    if (!TryGetAgentComponents(AgentId, Agent, Body, MovementCommand))
        return;

    InitialBodyTransforms[AgentId] = Body->GetComponentTransform();
    HasInitialBodyTransform[AgentId] = true;
    NextGoalDirectionIndices[AgentId] = AgentId % 8;
    EpisodeNumbers[AgentId] = 0;
    TotalNaturalEpisodeCounts[AgentId] = 0;
    TotalArrivalCounts[AgentId] = 0;
    ResetEpisodeStatistics(AgentId);
    UpdatePreviousGoalState(AgentId, *Body, *MovementCommand);
}

// 에이전트별 시작 방향부터 시계 방향 순서로 다음 8방향 목표를 설정한다.
void UCMAggressiveLearningTrainingEnvironment::SetNextTrainingGoal(int32 AgentId, UCMAggressiveMovementCommandComponent& MovementCommand)
{
    if (!NextGoalDirectionIndices.IsValidIndex(AgentId) || !InitialBodyTransforms.IsValidIndex(AgentId))
        return;

    constexpr int32 DirectionCount = 8;
    const int32 DirectionIndex = NextGoalDirectionIndices[AgentId] % DirectionCount;
    const ECMAggressiveMoveDirection Direction = static_cast<ECMAggressiveMoveDirection>(DirectionIndex);
    const FVector GoalLocation = CMAggressiveLearningGoal::ResolveWorldGoalLocation(InitialBodyTransforms[AgentId], Direction, GoalSettings.GoalDistance);
    MovementCommand.SetMovementGoal(GoalLocation, GoalSettings.AcceptanceRadius);
    ++EpisodeNumbers[AgentId];
    EpisodeStartDirections[AgentId] = Direction;
    ResetEpisodeStatistics(AgentId);
    NextGoalDirectionIndices[AgentId] = (DirectionIndex + 1) % DirectionCount;
}

// 현재 목표 위치와 몸통에서 목표까지의 평면 거리를 다음 보상 계산용으로 저장한다.
void UCMAggressiveLearningTrainingEnvironment::UpdatePreviousGoalState(int32 AgentId, UPrimitiveComponent& Body, UCMAggressiveMovementCommandComponent& MovementCommand)
{
    if (!HasPreviousGoalState.IsValidIndex(AgentId))
        return;

    HasPreviousGoalState[AgentId] = MovementCommand.HasMovementGoal();
    if (!HasPreviousGoalState[AgentId])
        return;

    const FCMAggressiveMovementGoal Goal = MovementCommand.GetMovementGoal();
    PreviousGoalLocations[AgentId] = Goal.WorldLocation;
    PreviousGoalDistances[AgentId] = CalculatePlanarDistance(Body.GetComponentLocation(), Goal.WorldLocation);
    if (PreviousFacingAlignments.IsValidIndex(AgentId))
        PreviousFacingAlignments[AgentId] = CalculateFacingAlignment(Body, Goal.WorldLocation);
}
