#include "Aggressive/Centipede/Learning/CMCentipedeLearningInferenceCoordinator.h"

#include "Aggressive/Centipede/CMCentipedePawn.h"
#include "Components/BoxComponent.h"
#include "Aggressive/Centipede/Learning/CMCentipedeLearningInteractor.h"
#include "Aggressive/Common/Learning/CMAggressiveLearningSnapshot.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsPolicy.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "Testing/CMAggressiveChaseTestTarget.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMCentipedeInference, Log, All);

ACMCentipedeLearningInferenceCoordinator::ACMCentipedeLearningInferenceCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    LearningManager = CreateDefaultSubobject<ULearningAgentsManager>(TEXT("CentipedeInferenceManager"));
    LearningManager->PrimaryComponentTick.bCanEverTick = false;
    LearningManager->PrimaryComponentTick.bStartWithTickEnabled = false;
}

void ACMCentipedeLearningInferenceCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FinishInference(ECMCentipedeMoveResult::Cancelled, false);
    Super::EndPlay(EndPlayReason);
}

bool ACMCentipedeLearningInferenceCoordinator::StartInferencePath(ACMCentipedePawn* InInferenceAgent, FVector WorldGoal, float AcceptanceRadius)
{
    if (bInferenceRunning || !IsValid(InInferenceAgent) || !LearningManager || LearningManager->GetAgentNum() > 0)
        return false;
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
    InferenceAgent->SetTailLeading(false);
    InferenceAgent->SetTrainingCurveProfile(0);
    InferenceAgent->OnPathMoveCompleted.AddUniqueDynamic(this, &ThisClass::HandlePathMoveCompleted);
    if (!InferenceAgent->StartPathMoveToLocation(ActiveWorldGoal, ActiveAcceptanceRadius))
    {
        FinishInference(ECMCentipedeMoveResult::Failed, false);
        return false;
    }

    bInferenceRunning = true;
    InferenceStartTime = GetWorld()->GetTimeSeconds();
    LastProgressCheckTime = InferenceStartTime;
    LastGoalDistance = FVector::Dist2D(InferenceAgent->GetAggressiveNavigationReferenceLocation(), ActiveWorldGoal);
    NoProgressSeconds = 0.0f;
    LateralEscapeAttemptCount = 0;
    Policy->RunInference(0.0f);
    GetWorldTimerManager().SetTimer(InferenceTimerHandle, this, &ThisClass::RunInferenceStep, FMath::Max(DecisionInterval, 0.01f), true);
    return true;
}

bool ACMCentipedeLearningInferenceCoordinator::StartChasingTestTarget(ACMCentipedePawn* InInferenceAgent, ACMAggressiveChaseTestTarget* InChaseTarget, float AcceptanceRadius)
{
    if (bInferenceRunning || !IsValid(InInferenceAgent) || !IsValid(InChaseTarget))
        return false;

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
    UE_LOG(LogCMCentipedeInference, Display, TEXT("Centipede AI가 추격 테스트 목표 추적을 시작했습니다. 목표=%s"), *ChaseTarget->GetActorLocation().ToCompactString());
    return true;
}

void ACMCentipedeLearningInferenceCoordinator::StopInference()
{
    FinishInference(ECMCentipedeMoveResult::Cancelled, true);
}

FString ACMCentipedeLearningInferenceCoordinator::GetSnapshotDirectory() const
{
    return CMAggressiveLearningSnapshot::GetLatestDirectory(ECMAggressiveLearningSnapshotProfile::Centipede);
}

bool ACMCentipedeLearningInferenceCoordinator::InitializeInferenceObjects()
{
    ULearningAgentsManager* Manager = LearningManager;
    Interactor = UCMCentipedeLearningInteractor::MakeCentipedeInteractor(Manager, TEXT("CentipedeInferenceInteractor"));
    if (!Interactor)
        return false;
    ULearningAgentsInteractor* BaseInteractor = Interactor;
    Policy = ULearningAgentsPolicy::MakePolicy(Manager, BaseInteractor, ULearningAgentsPolicy::StaticClass(), TEXT("CentipedeInferencePolicy"));
    return Policy && CMAggressiveLearningSnapshot::LoadInferenceNetworks(ECMAggressiveLearningSnapshotProfile::Centipede, *Policy);
}

bool ACMCentipedeLearningInferenceCoordinator::UpdateChaseTargetPath()
{
    UWorld* World = GetWorld();
    if (!World || !InferenceAgent || !IsValid(ChaseTarget))
        return false;

    NextChasePathRefreshTime = World->GetTimeSeconds() + FMath::Max(ChasePathRefreshInterval, 0.05f);
    const FVector CurrentTargetLocation = ChaseTarget->GetActorLocation();
    if (FVector::DistSquared2D(CurrentTargetLocation, ActiveWorldGoal) < FMath::Square(FMath::Max(ChaseRepathDistance, 1.0f)))
        return true;

    ActiveWorldGoal = CurrentTargetLocation;
    if (!InferenceAgent->StartPathMoveToLocation(ActiveWorldGoal, ActiveAcceptanceRadius))
        return false;

    bWaitingForChaseTargetMove = false;
    LastProgressCheckTime = World->GetTimeSeconds();
    LastGoalDistance = FVector::Dist2D(InferenceAgent->GetAggressiveNavigationReferenceLocation(), ActiveWorldGoal);
    NoProgressSeconds = 0.0f;
    LateralEscapeAttemptCount = 0;
    UE_LOG(LogCMCentipedeInference, Display, TEXT("추격 목표 이동을 감지해 Centipede AI 경로를 갱신했습니다. 새 목표=%s"), *ActiveWorldGoal.ToCompactString());
    return true;
}

bool ACMCentipedeLearningInferenceCoordinator::BeginLateralEscape()
{
    UWorld* World = GetWorld();
    UCMAggressiveOmnidirectionalPathComponent* PathMovement = InferenceAgent ? InferenceAgent->GetPathMovement() : nullptr;
    const TArray<TObjectPtr<UBoxComponent>>* Segments = InferenceAgent ? &InferenceAgent->GetBodySegments() : nullptr;
    if (!World || !InferenceAgent || !PathMovement || !Segments || Segments->Num() < 2)
        return false;
    if (LateralEscapeAttemptCount >= FMath::Max(MaximumLateralEscapeAttempts, 1))
    {
        UE_LOG(LogCMCentipedeInference, Warning, TEXT("Centipede AI가 최대 측면 이탈 횟수를 초과했습니다."));
        return false;
    }

    const FVector BodyCenter = GetBodyCenter();
    FVector BodyAxis = (*Segments)[0]->GetComponentLocation() - Segments->Last()->GetComponentLocation();
    BodyAxis.Z = 0.0f;
    if (!BodyAxis.Normalize())
        BodyAxis = (*Segments)[0]->GetForwardVector().GetSafeNormal2D();
    FVector PreferredDirection = FVector::CrossProduct(FVector::UpVector, BodyAxis).GetSafeNormal2D();
    FVector GoalDirection = (ActiveWorldGoal - BodyCenter).GetSafeNormal2D();
    if (FVector::DotProduct(PreferredDirection, GoalDirection) < 0.0f)
        PreferredDirection *= -1.0f;

    FVector EscapeDirection = LateralEscapeAttemptCount == 0 ? PreferredDirection : -LateralEscapeDirection;
    if (!CanMoveBodyLaterally(EscapeDirection))
    {
        EscapeDirection *= -1.0f;
        if (LateralEscapeAttemptCount > 0 || !CanMoveBodyLaterally(EscapeDirection))
        {
            UE_LOG(LogCMCentipedeInference, Warning, TEXT("Centipede AI의 양쪽 측면 이탈 경로가 모두 정적 장애물에 막혔습니다."));
            return false;
        }
    }

    PathMovement->PausePathMoveForRecovery();
    InferenceAgent->StopArticulatedBodyMotion();
    LateralEscapeStartLocation = BodyCenter;
    LateralEscapeDirection = EscapeDirection;
    LateralEscapeEndTime = World->GetTimeSeconds() + FMath::Max(LateralEscapeDuration, 0.1f);
    ++LateralEscapeAttemptCount;
    bPerformingLateralEscape = true;
    ApplyLateralEscapeVelocity();
    UE_LOG(LogCMCentipedeInference, Display,
        TEXT("Centipede AI가 말단 교대 대신 몸 전체의 측면 이탈을 시작했습니다. 시도=%d/%d 방향=%s"),
        LateralEscapeAttemptCount,
        FMath::Max(MaximumLateralEscapeAttempts, 1),
        *LateralEscapeDirection.ToCompactString());
    return true;
}

bool ACMCentipedeLearningInferenceCoordinator::UpdateLateralEscape()
{
    UWorld* World = GetWorld();
    if (!World || !InferenceAgent)
        return false;

    const float EscapeDistance = FVector::Dist2D(LateralEscapeStartLocation, GetBodyCenter());
    if (EscapeDistance >= FMath::Max(LateralEscapeMinimumDistance, 0.0f))
        return RebuildPathAfterLateralEscape();
    if (World->GetTimeSeconds() < LateralEscapeEndTime)
    {
        ApplyLateralEscapeVelocity();
        return true;
    }

    bPerformingLateralEscape = false;
    InferenceAgent->StopArticulatedBodyMotion();
    if (EscapeDistance >= FMath::Max(LateralEscapeMinimumDistance * 0.5f, 1.0f))
        return RebuildPathAfterLateralEscape();
    return BeginLateralEscape();
}

bool ACMCentipedeLearningInferenceCoordinator::RebuildPathAfterLateralEscape()
{
    UWorld* World = GetWorld();
    if (!World || !InferenceAgent)
        return false;

    bPerformingLateralEscape = false;
    InferenceAgent->StopArticulatedBodyMotion();
    if (!InferenceAgent->StartPathMoveToLocation(ActiveWorldGoal, ActiveAcceptanceRadius))
        return false;

    LastProgressCheckTime = World->GetTimeSeconds();
    LastGoalDistance = FVector::Dist2D(InferenceAgent->GetAggressiveNavigationReferenceLocation(), ActiveWorldGoal);
    NoProgressSeconds = 0.0f;
    UE_LOG(LogCMCentipedeInference, Display,
        TEXT("Centipede AI가 측면 이탈 후 머리·꼬리 경로를 다시 비교했습니다. 선택 선두=%s 목표=%s"),
        InferenceAgent->IsTailLeading() ? TEXT("꼬리") : TEXT("머리"),
        *ActiveWorldGoal.ToCompactString());
    return true;
}

bool ACMCentipedeLearningInferenceCoordinator::CanMoveBodyLaterally(const FVector& Direction) const
{
    UWorld* World = GetWorld();
    if (!World || !InferenceAgent || Direction.IsNearlyZero())
        return false;

    FCollisionObjectQueryParams StaticObjects;
    StaticObjects.AddObjectTypesToQuery(ECC_WorldStatic);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMCentipedeLateralEscape), false, InferenceAgent);
    for (const UBoxComponent* Segment : InferenceAgent->GetBodySegments())
    {
        if (!Segment)
            continue;
        FHitResult Hit;
        const FVector Start = Segment->GetComponentLocation();
        const FVector End = Start + Direction * FMath::Max(LateralEscapeProbeDistance, 1.0f);
        if (World->LineTraceSingleByObjectType(Hit, Start, End, StaticObjects, QueryParams))
            return false;
    }
    return true;
}

FVector ACMCentipedeLearningInferenceCoordinator::GetBodyCenter() const
{
    FVector Center = FVector::ZeroVector;
    int32 ValidSegmentCount = 0;
    if (InferenceAgent)
    {
        for (const UBoxComponent* Segment : InferenceAgent->GetBodySegments())
        {
            if (!Segment)
                continue;
            Center += Segment->GetComponentLocation();
            ++ValidSegmentCount;
        }
    }
    return ValidSegmentCount > 0 ? Center / static_cast<float>(ValidSegmentCount) : FVector::ZeroVector;
}

void ACMCentipedeLearningInferenceCoordinator::ApplyLateralEscapeVelocity() const
{
    if (!InferenceAgent || LateralEscapeDirection.IsNearlyZero())
        return;
    const FVector PlanarVelocity = LateralEscapeDirection * FMath::Max(LateralEscapeSpeed, 0.0f);
    for (UBoxComponent* Segment : InferenceAgent->GetBodySegments())
    {
        if (!Segment || !Segment->IsSimulatingPhysics())
            continue;
        FVector Velocity = Segment->GetPhysicsLinearVelocity();
        Velocity.X = PlanarVelocity.X;
        Velocity.Y = PlanarVelocity.Y;
        Segment->SetPhysicsLinearVelocity(Velocity);
        Segment->WakeAllRigidBodies();
    }
}

void ACMCentipedeLearningInferenceCoordinator::RunInferenceStep()
{
    UWorld* World = GetWorld();
    if (!bInferenceRunning || !World || !Policy || !InferenceAgent)
    {
        FinishInference(ECMCentipedeMoveResult::Failed, true);
        return;
    }
    if (!bChasingTestTarget && World->GetTimeSeconds() - InferenceStartTime >= FMath::Max(MaximumInferenceSeconds, 1.0f))
    {
        FinishInference(ECMCentipedeMoveResult::TimedOut, true);
        return;
    }

    const double CurrentTime = World->GetTimeSeconds();
    if (bChasingTestTarget && !IsValid(ChaseTarget))
    {
        FinishInference(ECMCentipedeMoveResult::Failed, true);
        return;
    }
    if (bPerformingLateralEscape)
    {
        if (!UpdateLateralEscape())
            FinishInference(ECMCentipedeMoveResult::Failed, true);
        return;
    }
    if (bChasingTestTarget && CurrentTime >= NextChasePathRefreshTime)
    {
        if (!UpdateChaseTargetPath())
        {
            FinishInference(ECMCentipedeMoveResult::Failed, true);
            return;
        }
    }
    if (bWaitingForChaseTargetMove)
        return;

    if (InferenceAgent->IsAligningLeadingEnd())
    {
        LastProgressCheckTime = CurrentTime;
        LastGoalDistance = FVector::Dist2D(InferenceAgent->GetAggressiveNavigationReferenceLocation(), ActiveWorldGoal);
        NoProgressSeconds = 0.0f;
    }
    else if (CurrentTime - LastProgressCheckTime >= FMath::Max(ProgressCheckInterval, 0.1f))
    {
        const float CurrentDistance = FVector::Dist2D(InferenceAgent->GetAggressiveNavigationReferenceLocation(), ActiveWorldGoal);
        const float Elapsed = static_cast<float>(CurrentTime - LastProgressCheckTime);
        const bool bMadeProgress = LastGoalDistance - CurrentDistance >= FMath::Max(MinimumProgressDistance, 0.0f);
        NoProgressSeconds = bMadeProgress ? 0.0f : NoProgressSeconds + Elapsed;
        if (bMadeProgress)
            LateralEscapeAttemptCount = 0;
        LastGoalDistance = CurrentDistance;
        LastProgressCheckTime = CurrentTime;
        if (NoProgressSeconds >= FMath::Max(MaximumNoProgressSeconds, 0.1f))
        {
            NoProgressSeconds = 0.0f;
            if (!BeginLateralEscape())
            {
                FinishInference(ECMCentipedeMoveResult::Failed, true);
                return;
            }
            return;
        }
    }
    Policy->RunInference(0.0f);
}

void ACMCentipedeLearningInferenceCoordinator::FinishInference(ECMCentipedeMoveResult Result, bool bBroadcast)
{
    const bool bWasRunning = bInferenceRunning;
    bInferenceRunning = false;
    GetWorldTimerManager().ClearTimer(InferenceTimerHandle);
    if (InferenceAgent)
    {
        InferenceAgent->OnPathMoveCompleted.RemoveDynamic(this, &ThisClass::HandlePathMoveCompleted);
        InferenceAgent->StopPathMove();
        InferenceAgent->StopArticulatedBodyMotion();
        InferenceAgent->SetTailLeading(false);
    }
    if (LearningManager && LearningManager->GetAgentNum() > 0)
        LearningManager->RemoveAllAgents();
    InferenceAgentId = INDEX_NONE;
    InferenceAgent = nullptr;
    ChaseTarget = nullptr;
    Policy = nullptr;
    Interactor = nullptr;
    NextChasePathRefreshTime = 0.0;
    LateralEscapeStartLocation = FVector::ZeroVector;
    LateralEscapeDirection = FVector::ZeroVector;
    LateralEscapeEndTime = 0.0;
    LateralEscapeAttemptCount = 0;
    bChasingTestTarget = false;
    bWaitingForChaseTargetMove = false;
    bPerformingLateralEscape = false;
    if (bBroadcast && bWasRunning)
        OnCentipedeMoveCompleted.Broadcast(Result);
}

void ACMCentipedeLearningInferenceCoordinator::HandlePathMoveCompleted(ECMAggressivePathMoveResult Result)
{
    if (Result == ECMAggressivePathMoveResult::ReachedGoal && bChasingTestTarget)
    {
        bWaitingForChaseTargetMove = true;
        return;
    }
    if (Result == ECMAggressivePathMoveResult::ReachedGoal)
        FinishInference(ECMCentipedeMoveResult::ReachedGoal, true);
    else if (Result == ECMAggressivePathMoveResult::Cancelled)
        FinishInference(ECMCentipedeMoveResult::Cancelled, true);
    else
        FinishInference(ECMCentipedeMoveResult::Failed, true);
}
