#include "Aggressive/Ripper/Learning/CMRipperLearningInferenceCoordinator.h"

#include "Aggressive/Ripper/CMRipperPawn.h"
#include "Aggressive/Common/Behavior/CMAggressiveBehaviorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Aggressive/Common/Learning/CMAggressiveLearningInteractor.h"
#include "Aggressive/Common/Learning/CMAggressiveLearningSnapshot.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsPolicy.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Testing/CMAggressiveChaseTestTarget.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMRipperInference, Log, All);

// Tick 없이 Ripper AI 정책 추론을 실행할 중앙 Manager를 생성한다.
ACMRipperLearningInferenceCoordinator::ACMRipperLearningInferenceCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    LearningManager = CreateDefaultSubobject<ULearningAgentsManager>(TEXT("RipperInferenceManager"));
    LearningManager->PrimaryComponentTick.bCanEverTick = false;
    LearningManager->PrimaryComponentTick.bStartWithTickEnabled = false;
}

// Actor 종료 시 정책 타이머와 경로 이동을 취소한다.
void ACMRipperLearningInferenceCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FinishInference(ECMRipperMoveResult::Cancelled, false);
    Super::EndPlay(EndPlayReason);
}

// 저장된 Ripper AI 정책을 불러와 NavMesh 경로 목적지 이동을 시작한다.
bool ACMRipperLearningInferenceCoordinator::StartInferencePath(ACMRipperPawn* InInferenceAgent, FVector WorldGoal, float AcceptanceRadius)
{
    if (bInferenceRunning || !IsValid(InInferenceAgent) || !LearningManager || LearningManager->GetAgentNum() > 0)
    {
        return false;
    }
    if (GetOwner() != InInferenceAgent)
    {
        if (UCMAggressiveBehaviorComponent* Behavior = InInferenceAgent->FindComponentByClass<UCMAggressiveBehaviorComponent>())
        {
            Behavior->SetBehaviorEnabled(false);
        }
    }

    InferenceAgent = InInferenceAgent;
    LearningManager->SetMaxAgentNum(1);
    if (!InitializeInferenceObjects())
    {
        InferenceAgent = nullptr;
        return false;
    }

    InferenceAgentId = LearningManager->AddAgent(InferenceAgent);
    if (InferenceAgentId == INDEX_NONE)
    {
        InferenceAgent = nullptr;
        return false;
    }

    ActiveWorldGoal = WorldGoal;
    ActiveAcceptanceRadius = FMath::Max(AcceptanceRadius, 0.0f);
    bInferenceRunning = true;
    InferenceAgent->OnPathMoveCompleted.AddUniqueDynamic(this, &ThisClass::HandlePathMoveCompleted);
    if (!InferenceAgent->StartPathMoveToLocation(ActiveWorldGoal, ActiveAcceptanceRadius))
    {
        FinishInference(ECMRipperMoveResult::Failed, false);

        return false;
    }

    if (!bInferenceRunning || !Policy || !InferenceAgent)
        return true;
    InferenceStartTime = GetWorld()->GetTimeSeconds();
    StuckRecoveryAttemptCount = 0;
    ResetStuckProgress();
    Policy->RunInference(0.0f);
    GetWorldTimerManager().SetTimer(InferenceTimerHandle, this, &ThisClass::RunInferenceStep, FMath::Max(DecisionInterval, 0.01f), true);
    return true;
}

bool ACMRipperLearningInferenceCoordinator::UpdateInferenceGoal(const FVector WorldGoal)
{
    if (!bInferenceRunning || bRecoveringFromStuck || !InferenceAgent)
    {
        return false;
    }
    ActiveWorldGoal = WorldGoal;
    if (!InferenceAgent->StartPathMoveToLocation(ActiveWorldGoal, ActiveAcceptanceRadius))
    {
        return false;
    }
    bWaitingForChaseTargetMove = false;
    ResetStuckProgress();

    return true;
}

// 저장 정책 추론을 유지하면서 순간이동하는 추격 테스트 목표를 계속 따라간다.
bool ACMRipperLearningInferenceCoordinator::StartChasingTestTarget(ACMRipperPawn* InInferenceAgent, ACMAggressiveChaseTestTarget* InChaseTarget, float AcceptanceRadius)
{
    if (bInferenceRunning || !IsValid(InInferenceAgent) || !IsValid(InChaseTarget))
    {
        return false;
    }

    ChaseTarget = InChaseTarget;
    bChasingTestTarget = true;
    bWaitingForChaseTargetMove = false;
    if (!StartInferencePath(InInferenceAgent, ChaseTarget->GetActorLocation(), AcceptanceRadius))
    {
        ChaseTarget = nullptr;
        bChasingTestTarget = false;
        return false;
    }

    NextChasePathRefreshTime = GetWorld()->GetTimeSeconds() + FMath::Max(ChasePathRefreshInterval, 0.05f);
    return true;
}

// 실행 중인 Ripper AI 정책 추론과 NavMesh 경로를 취소한다.
void ACMRipperLearningInferenceCoordinator::StopInference()
{
    FinishInference(ECMRipperMoveResult::Cancelled, true);
}

// Ripper AI 저장 정책 추론의 실행 여부를 반환한다.
bool ACMRipperLearningInferenceCoordinator::IsInferenceRunning() const
{
    return bInferenceRunning;
}

// 추론에 사용하는 Ripper AI 최신 스냅샷 폴더를 반환한다.
FString ACMRipperLearningInferenceCoordinator::GetSnapshotDirectory() const
{
    return CMAggressiveLearningSnapshot::GetInferenceDirectory(ECMAggressiveLearningSnapshotProfile::Ripper);
}

// 세 다리 Interactor와 저장된 Ripper AI 정책을 생성한다.
bool ACMRipperLearningInferenceCoordinator::InitializeInferenceObjects()
{
    ULearningAgentsManager* Manager = LearningManager;
    Interactor = UCMAggressiveLearningInteractor::MakeAggressiveInteractor(Manager, 3, TEXT("RipperInferenceInteractor"));
    if (!Interactor)
        return false;

    ULearningAgentsInteractor* BaseInteractor = Interactor;
    Policy = ULearningAgentsPolicy::MakePolicy(Manager, BaseInteractor, ULearningAgentsPolicy::StaticClass(), TEXT("RipperInferencePolicy"));

    return Policy && CMAggressiveLearningSnapshot::LoadInferenceNetworks(ECMAggressiveLearningSnapshotProfile::Ripper, *Policy);
}

// 추격 목표가 충분히 이동했을 때 정책은 유지하고 NavMesh 경로만 다시 생성한다.
bool ACMRipperLearningInferenceCoordinator::UpdateChaseTargetPath()
{
    UWorld* World = GetWorld();
    if (!World || !InferenceAgent || !IsValid(ChaseTarget))
    {
        return false;
    }

    NextChasePathRefreshTime = World->GetTimeSeconds() + FMath::Max(ChasePathRefreshInterval, 0.05f);
    const FVector CurrentTargetLocation = ChaseTarget->GetActorLocation();
    if (FVector::DistSquared2D(CurrentTargetLocation, ActiveWorldGoal) < FMath::Square(FMath::Max(ChaseRepathDistance, 1.0f)))
        return true;

    ActiveWorldGoal = CurrentTargetLocation;
    if (!InferenceAgent->StartPathMoveToLocation(ActiveWorldGoal, ActiveAcceptanceRadius))
        return false;

    bWaitingForChaseTargetMove = false;
    ResetStuckProgress();
    return true;
}

// 현재 경유점 진행량을 확인하고 벽 근처 정체 시 복구 이동을 시작한다.
bool ACMRipperLearningInferenceCoordinator::UpdateStuckDetection()
{
    UWorld* World = GetWorld();
    UCMAggressiveMovementCommandComponent* MovementCommand = InferenceAgent ? InferenceAgent->GetMovementCommand() : nullptr;
    if (!World || !MovementCommand || !MovementCommand->HasMovementGoal())
        return true;

    const FCMAggressiveMovementGoal MovementGoal = MovementCommand->GetMovementGoal();
    if (!MovementGoal.WorldLocation.Equals(LastStuckPathPoint, 1.0f))
    {
        ResetStuckProgress();

        return true;
    }

    const double CurrentTime = World->GetTimeSeconds();
    const double CheckElapsedSeconds = CurrentTime - LastStuckCheckTime;
    if (CheckElapsedSeconds < FMath::Max(StuckCheckInterval, 0.1f))
        return true;

    const float CurrentGoalDistance = FVector::Dist2D(InferenceAgent->GetAggressiveNavigationReferenceLocation(), MovementGoal.WorldLocation);
    const float CurrentGoalAngularError = CalculateCurrentGoalAngularError(MovementGoal.WorldLocation);
    const bool bMadeDistanceProgress = LastStuckGoalDistance - CurrentGoalDistance >= FMath::Max(StuckMinimumProgressDistance, 0.0f);
    const bool bMadeAngularProgress = LastStuckGoalAngularError - CurrentGoalAngularError >= FMath::DegreesToRadians(FMath::Max(StuckMinimumAngularProgressDegrees, 0.0f));
    if (bMadeDistanceProgress)
    {
        StuckNoProgressSeconds = 0.0f;
        StuckNoDistanceProgressSeconds = 0.0f;
        StuckRecoveryAttemptCount = 0;
    }
    else
    {
        StuckNoDistanceProgressSeconds += static_cast<float>(CheckElapsedSeconds);
        if (bMadeAngularProgress)
            StuckNoProgressSeconds = 0.0f;
        else
            StuckNoProgressSeconds += static_cast<float>(CheckElapsedSeconds);
    }

    LastStuckGoalDistance = CurrentGoalDistance;
    LastStuckGoalAngularError = CurrentGoalAngularError;
    LastStuckCheckTime = CurrentTime;
    const bool bNoCombinedProgress = StuckNoProgressSeconds >= FMath::Max(StuckMaximumNoProgressSeconds, 0.1f);
    const bool bNoDistanceProgress = StuckNoDistanceProgressSeconds >= FMath::Max(StuckMaximumNoDistanceProgressSeconds, 0.1f);
    if (!bNoCombinedProgress && !bNoDistanceProgress)
        return true;

    StuckNoProgressSeconds = 0.0f;
    StuckNoDistanceProgressSeconds = 0.0f;
    return BeginStuckRecovery();
}

// 벽 바깥쪽 복구 위치를 선택하고 기존 경로를 일시 중지한 뒤 물리 이탈을 시작한다.
bool ACMRipperLearningInferenceCoordinator::BeginStuckRecovery()
{
    FVector RecoveryLocation;
    FVector NewRecoveryDirection;
    if (!FindStuckRecoveryLocation(RecoveryLocation, NewRecoveryDirection))
        return StuckRecoveryAttemptCount > 0 ? RebuildPathAfterStuckRecovery() : true;
    if (StuckRecoveryAttemptCount >= FMath::Max(MaximumStuckRecoveryAttempts, 1))
    {
        UE_LOG(LogCMRipperInference, Warning, TEXT("Ripper AI가 최대 벽 이탈 복구 횟수를 초과했습니다."));

        return false;
    }

    UPrimitiveComponent* Body = InferenceAgent ? InferenceAgent->GetAggressiveMovementBody() : nullptr;
    UCMAggressiveOmnidirectionalPathComponent* PathMovement = InferenceAgent ? InferenceAgent->GetPathMovement() : nullptr;
    UWorld* World = GetWorld();
    if (!Body || !PathMovement || !World)
    {
        return false;
    }

    if (StuckRecoveryAttemptCount == 0)
    {
        RecoverySequenceStartLocation = InferenceAgent->GetAggressiveNavigationReferenceLocation();
        PathMovement->PausePathMoveForRecovery();
    }
    ++StuckRecoveryAttemptCount;
    StopBodyPlanarMotion();
    RecoveryStartLocation = InferenceAgent->GetAggressiveNavigationReferenceLocation();
    RecoveryTargetLocation = RecoveryLocation;
    RecoveryDirection = NewRecoveryDirection;
    ApplyStuckRecoveryImpulse();
    RecoveryEndTime = World->GetTimeSeconds() + FMath::Max(RecoveryDuration, 0.1f);
    bRecoveringFromStuck = true;
    return true;
}

// 반복 임펄스로 누적 이탈 거리를 확보한 뒤 최종 목적지 경로를 다시 생성한다.
bool ACMRipperLearningInferenceCoordinator::UpdateStuckRecovery()
{
    UWorld* World = GetWorld();
    if (!World || !InferenceAgent)
        return false;

    const FVector CurrentLocation = InferenceAgent->GetAggressiveNavigationReferenceLocation();
    const float CumulativeRecoveryDistance = FVector::Dist2D(RecoverySequenceStartLocation, CurrentLocation);
    if (CumulativeRecoveryDistance >= FMath::Max(RecoveryMinimumMoveDistance, 0.0f))
        return RebuildPathAfterStuckRecovery();
    if (World->GetTimeSeconds() < RecoveryEndTime)
    {
        ApplyStuckRecoveryImpulse();

        return true;
    }

    bRecoveringFromStuck = false;
    StopBodyPlanarMotion();
    UE_LOG(LogCMRipperInference, Warning, TEXT("Ripper AI 벽 이탈 거리가 부족해 다른 복구 방향을 다시 찾습니다. 누적 이동: %.1fcm"), CumulativeRecoveryDistance);
    if (StuckRecoveryAttemptCount >= FMath::Max(MaximumStuckRecoveryAttempts, 1))
        return false;
    return BeginStuckRecovery();
}

// 벽 이탈 뒤 최종 목적지까지의 새 경로를 만들고 정책 목표를 복구한다.
bool ACMRipperLearningInferenceCoordinator::RebuildPathAfterStuckRecovery()
{
    if (!InferenceAgent)
        return false;

    bRecoveringFromStuck = false;
    StopBodyPlanarMotion();
    const float CumulativeRecoveryDistance = FVector::Dist2D(RecoverySequenceStartLocation, InferenceAgent->GetAggressiveNavigationReferenceLocation());
    if (!InferenceAgent->StartPathMoveToLocation(ActiveWorldGoal, ActiveAcceptanceRadius))
        return false;

    ResetStuckProgress();
    StuckRecoveryAttemptCount = 0;
    return true;
}

// 복구 방향의 현재 속도가 목표보다 낮을 때 작은 임펄스를 반복 적용한다.
void ACMRipperLearningInferenceCoordinator::ApplyStuckRecoveryImpulse()
{
    UPrimitiveComponent* Body = InferenceAgent ? InferenceAgent->GetAggressiveMovementBody() : nullptr;
    if (!Body || RecoveryDirection.IsNearlyZero())
        return;

    const float DesiredRecoverySpeed = FMath::Max(RecoveryImpulseSpeed, 0.0f);
    const float CurrentRecoverySpeed = FVector::DotProduct(Body->GetPhysicsLinearVelocity(), RecoveryDirection);
    const float SpeedDelta = FMath::Clamp(DesiredRecoverySpeed - CurrentRecoverySpeed, 0.0f, DesiredRecoverySpeed * 0.25f);
    if (SpeedDelta > 0.0f)
        Body->AddImpulse(RecoveryDirection * Body->GetMass() * SpeedDelta);
}

// 주변 정적 벽의 반대 방향에서 전용 NavMesh에 포함되고 중심선이 막히지 않은 복구 위치를 찾는다.
bool ACMRipperLearningInferenceCoordinator::FindStuckRecoveryLocation(FVector& OutRecoveryLocation, FVector& OutRecoveryDirection) const
{
    OutRecoveryLocation = FVector::ZeroVector;
    OutRecoveryDirection = FVector::ZeroVector;
    UWorld* World = GetWorld();
    UPrimitiveComponent* Body = InferenceAgent ? InferenceAgent->GetAggressiveMovementBody() : nullptr;
    UCMAggressiveOmnidirectionalPathComponent* PathMovement = InferenceAgent ? InferenceAgent->GetPathMovement() : nullptr;
    UNavigationSystemV1* NavigationSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    if (!World || !Body || !PathMovement || !NavigationSystem)
    {
        return false;
    }

    const FVector BodyLocation = InferenceAgent->GetAggressiveNavigationReferenceLocation();
    FCollisionObjectQueryParams StaticObjectQuery;
    StaticObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMRipperStuckRecovery), false, InferenceAgent);
    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByObjectType(Overlaps, BodyLocation, FQuat::Identity, StaticObjectQuery, FCollisionShape::MakeSphere(FMath::Max(RecoveryWallSearchRadius, 1.0f)), QueryParams);

    FVector WallOutwardDirection = FVector::ZeroVector;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        const UPrimitiveComponent* Obstacle = Overlap.GetComponent();
        if (!Obstacle)
            continue;

        FVector ClosestPoint;
        const float SurfaceDistance = Obstacle->GetClosestPointOnCollision(BodyLocation, ClosestPoint);
        FVector SurfaceToBody = BodyLocation - ClosestPoint;
        SurfaceToBody.Z = 0.0f;
        if (SurfaceToBody.IsNearlyZero() && Obstacle->Bounds.BoxExtent.Z >= 50.0f)
        {
            SurfaceToBody = BodyLocation - Obstacle->Bounds.Origin;
            SurfaceToBody.Z = 0.0f;
        }
        if (SurfaceToBody.IsNearlyZero() || SurfaceDistance < 0.0f)
            continue;
        WallOutwardDirection += SurfaceToBody.GetSafeNormal() / FMath::Max(SurfaceDistance, 10.0f);
    }
    if (WallOutwardDirection.IsNearlyZero())
        return false;
    WallOutwardDirection.Normalize();

    const ANavigationData* NavigationData = nullptr;
    for (const FNavDataConfig& AgentConfig : NavigationSystem->GetSupportedAgents())
    {
        if (AgentConfig.Name != PathMovement->GetNavigationAgentName())
            continue;
        NavigationData = NavigationSystem->GetNavDataForProps(AgentConfig);
        break;
    }
    if (!NavigationData)
        return false;

    constexpr float CandidateAngles[] = {0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f};
    const float CandidateDistances[] = {FMath::Max(RecoveryTargetDistance, 1.0f), FMath::Max(RecoveryTargetDistance * 1.5f, 1.0f)};
    for (const float CandidateDistance : CandidateDistances)
    {
        for (const float CandidateAngle : CandidateAngles)
        {
            FVector CandidateDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(CandidateAngle)).RotateVector(WallOutwardDirection);
            CandidateDirection.Z = 0.0f;
            CandidateDirection.Normalize();
            const FVector CandidateLocation = BodyLocation + CandidateDirection * CandidateDistance;
            FNavLocation ProjectedLocation;
            if (!NavigationSystem->ProjectPointToNavigation(CandidateLocation, ProjectedLocation, FVector(100.0f, 100.0f, 200.0f), NavigationData))
                continue;
            if (FVector::DistSquared2D(BodyLocation, ProjectedLocation.Location) < FMath::Square(FMath::Max(RecoveryMinimumMoveDistance, 1.0f)))
                continue;

            FVector TraceEnd = ProjectedLocation.Location;
            TraceEnd.Z = BodyLocation.Z;
            FHitResult BlockingHit;
            if (World->LineTraceSingleByObjectType(BlockingHit, BodyLocation, TraceEnd, StaticObjectQuery, QueryParams))
                continue;

            OutRecoveryLocation = ProjectedLocation.Location;
            OutRecoveryDirection = (TraceEnd - BodyLocation).GetSafeNormal2D();

            return !OutRecoveryDirection.IsNearlyZero();
        }
    }
    return false;
}

// 추론 상태와 물리 이동을 정리하고 선택적으로 완료 결과를 알린다.
void ACMRipperLearningInferenceCoordinator::FinishInference(ECMRipperMoveResult Result, bool bBroadcastResult)
{
    const bool bWasRunning = bInferenceRunning;
    bInferenceRunning = false;
    GetWorldTimerManager().ClearTimer(InferenceTimerHandle);

    if (InferenceAgent)
    {
        InferenceAgent->OnPathMoveCompleted.RemoveDynamic(this, &ThisClass::HandlePathMoveCompleted);
        if (UCMAggressiveOmnidirectionalPathComponent* PathMovement = InferenceAgent->GetPathMovement())
        {
            if (PathMovement->IsPathMoving())
                InferenceAgent->StopPathMove();
        }
        if (UPrimitiveComponent* Body = InferenceAgent->GetAggressiveMovementBody())
        {
            FVector Velocity = Body->GetPhysicsLinearVelocity();
            Velocity.X = 0.0f;
            Velocity.Y = 0.0f;
            Body->SetPhysicsLinearVelocity(Velocity);
            Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        }
    }

    if (LearningManager && LearningManager->GetAgentNum() > 0)
        LearningManager->RemoveAllAgents();
    InferenceAgentId = INDEX_NONE;
    InferenceAgent = nullptr;
    ChaseTarget = nullptr;
    ActiveWorldGoal = FVector::ZeroVector;
    LastStuckPathPoint = FVector::ZeroVector;
    RecoveryStartLocation = FVector::ZeroVector;
    RecoverySequenceStartLocation = FVector::ZeroVector;
    RecoveryTargetLocation = FVector::ZeroVector;
    RecoveryDirection = FVector::ZeroVector;
    ActiveAcceptanceRadius = 50.0f;
    LastStuckGoalDistance = 0.0f;
    LastStuckGoalAngularError = PI;
    StuckNoProgressSeconds = 0.0f;
    StuckNoDistanceProgressSeconds = 0.0f;
    NextChasePathRefreshTime = 0.0;
    LastStuckCheckTime = 0.0;
    RecoveryEndTime = 0.0;
    StuckRecoveryAttemptCount = 0;
    bRecoveringFromStuck = false;
    bChasingTestTarget = false;
    bWaitingForChaseTargetMove = false;
    if (bBroadcastResult && bWasRunning)
        OnRipperMoveCompleted.Broadcast(Result);
}

// 현재 경유점을 기준으로 정체 진행 거리와 확인 시간을 초기화한다.
void ACMRipperLearningInferenceCoordinator::ResetStuckProgress()
{
    UWorld* World = GetWorld();
    UCMAggressiveMovementCommandComponent* MovementCommand = InferenceAgent ? InferenceAgent->GetMovementCommand() : nullptr;
    LastStuckCheckTime = World ? World->GetTimeSeconds() : 0.0;
    StuckNoProgressSeconds = 0.0f;
    StuckNoDistanceProgressSeconds = 0.0f;
    if (!InferenceAgent || !MovementCommand || !MovementCommand->HasMovementGoal())
    {
        LastStuckPathPoint = FVector::ZeroVector;
        LastStuckGoalDistance = 0.0f;
        LastStuckGoalAngularError = PI;
        return;
    }

    LastStuckPathPoint = MovementCommand->GetMovementGoal().WorldLocation;
    LastStuckGoalDistance = FVector::Dist2D(InferenceAgent->GetAggressiveNavigationReferenceLocation(), LastStuckPathPoint);
    LastStuckGoalAngularError = CalculateCurrentGoalAngularError(LastStuckPathPoint);
}

// 현재 몸통 전방과 목표 방향 사이의 평면 각도 오차를 라디안으로 반환한다.
float ACMRipperLearningInferenceCoordinator::CalculateCurrentGoalAngularError(FVector GoalLocation) const
{
    UPrimitiveComponent* Body = InferenceAgent ? InferenceAgent->GetAggressiveMovementBody() : nullptr;
    if (!Body)
        return PI;

    FVector ForwardDirection = Body->GetForwardVector();
    ForwardDirection.Z = 0.0f;
    FVector GoalDirection = GoalLocation - InferenceAgent->GetAggressiveNavigationReferenceLocation();
    GoalDirection.Z = 0.0f;
    if (ForwardDirection.IsNearlyZero() || GoalDirection.IsNearlyZero())
        return 0.0f;
    return FMath::Acos(FMath::Clamp(FVector::DotProduct(ForwardDirection.GetSafeNormal(), GoalDirection.GetSafeNormal()), -1.0f, 1.0f));
}

// 복구 전후에 몸통의 수직 속도만 남기고 평면 및 회전 속도를 제거한다.
void ACMRipperLearningInferenceCoordinator::StopBodyPlanarMotion() const
{
    UPrimitiveComponent* Body = InferenceAgent ? InferenceAgent->GetAggressiveMovementBody() : nullptr;
    if (!Body)
        return;

    FVector Velocity = Body->GetPhysicsLinearVelocity();
    Velocity.X = 0.0f;
    Velocity.Y = 0.0f;
    Body->SetPhysicsLinearVelocity(Velocity);
    Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
}

// 제한시간 전까지 저장 정책의 세 다리 행동을 판단 주기마다 실행한다.
void ACMRipperLearningInferenceCoordinator::RunInferenceStep()
{
    UWorld* World = GetWorld();
    if (!bInferenceRunning || !World || !Policy || !InferenceAgent)
    {
        FinishInference(ECMRipperMoveResult::Failed, true);

        return;
    }

    if (!bChasingTestTarget && World->GetTimeSeconds() - InferenceStartTime >= FMath::Max(MaximumInferenceSeconds, 0.1f))
    {
        FinishInference(ECMRipperMoveResult::TimedOut, true);

        return;
    }

    if (bRecoveringFromStuck)
    {
        if (!UpdateStuckRecovery())
            FinishInference(ECMRipperMoveResult::Failed, true);

        return;
    }

    if (bChasingTestTarget && !IsValid(ChaseTarget))
    {
        FinishInference(ECMRipperMoveResult::Failed, true);

        return;
    }
    if (bChasingTestTarget && World->GetTimeSeconds() >= NextChasePathRefreshTime)
    {
        if (!UpdateChaseTargetPath())
            FinishInference(ECMRipperMoveResult::Failed, true);
        if (!bInferenceRunning || bWaitingForChaseTargetMove)
            return;
    }
    if (bWaitingForChaseTargetMove)
        return;

    if (!UpdateStuckDetection())
    {
        FinishInference(ECMRipperMoveResult::Failed, true);

        return;
    }
    if (bRecoveringFromStuck)
        return;

    UCMAggressiveMovementCommandComponent* MovementCommand = InferenceAgent->GetMovementCommand();
    if (!MovementCommand || !MovementCommand->HasMovementGoal())
    {
        FinishInference(ECMRipperMoveResult::Failed, true);

        return;
    }
    Policy->RunInference(0.0f);
}

// Pawn의 NavMesh 경로 결과를 Ripper AI 추론 완료 결과로 변환한다.
void ACMRipperLearningInferenceCoordinator::HandlePathMoveCompleted(ECMAggressivePathMoveResult Result)
{
    if (Result == ECMAggressivePathMoveResult::ReachedGoal && bChasingTestTarget)
    {
        bWaitingForChaseTargetMove = true;
        return;
    }
    if (Result == ECMAggressivePathMoveResult::ReachedGoal)
        FinishInference(ECMRipperMoveResult::ReachedGoal, true);
    else if (Result == ECMAggressivePathMoveResult::Cancelled)
        FinishInference(ECMRipperMoveResult::Cancelled, true);
    else
        FinishInference(ECMRipperMoveResult::Failed, true);
}
