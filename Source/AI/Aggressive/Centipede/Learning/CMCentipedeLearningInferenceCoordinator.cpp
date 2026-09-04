#include "Aggressive/Centipede/Learning/CMCentipedeLearningInferenceCoordinator.h"

#include "Aggressive/Centipede/CMCentipedePawn.h"
#include "Aggressive/Common/Behavior/CMAggressiveBehaviorComponent.h"
#include "Components/BoxComponent.h"
#include "Aggressive/Centipede/Learning/CMCentipedeLearningInteractor.h"
#include "Aggressive/Common/Learning/CMAggressiveLearningSnapshot.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsPolicy.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "Testing/CMAggressiveChaseTestTarget.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMCentipedeInference, Log, All);

// Actor Tick 없이 Centipede 정책 추론과 정체 복구를 타이머로 관리한다.
ACMCentipedeLearningInferenceCoordinator::ACMCentipedeLearningInferenceCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    LearningManager = CreateDefaultSubobject<ULearningAgentsManager>(TEXT("CentipedeInferenceManager"));
    LearningManager->PrimaryComponentTick.bCanEverTick = false;
    LearningManager->PrimaryComponentTick.bStartWithTickEnabled = false;
}

// Actor 종료 시 실행 중인 정책·경로·관절 몸체 이동을 정리한다.
void ACMCentipedeLearningInferenceCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FinishInference(ECMCentipedeMoveResult::Cancelled, false);
    Super::EndPlay(EndPlayReason);
}

// 저장 정책을 불러와 유리한 말단이 NavMesh 목적지를 따라가도록 추론을 시작한다.
bool ACMCentipedeLearningInferenceCoordinator::StartInferencePath(ACMCentipedePawn* InInferenceAgent, FVector WorldGoal, float AcceptanceRadius)
{
    if (bInferenceRunning || !IsValid(InInferenceAgent) || !LearningManager || LearningManager->GetAgentNum() > 0)
        return false;
    // 외부 조정자가 제어하는 에이전트는 자체 행동 상태 머신과 이동 명령이 경쟁하지 않게 한다.
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
    InferenceAgent->SetTailLeading(false);
    InferenceAgent->SetTrainingCurveProfile(0);
    InferenceAgent->OnPathMoveCompleted.AddUniqueDynamic(this, &ThisClass::HandlePathMoveCompleted);
    if (!InferenceAgent->StartPathMoveToLocation(ActiveWorldGoal, ActiveAcceptanceRadius))
    {
        FinishInference(ECMCentipedeMoveResult::Failed, false);

        return false;
    }

    if (!bInferenceRunning || !Policy || !InferenceAgent)
        return true;
    InferenceStartTime = GetWorld()->GetTimeSeconds();
    LastProgressCheckTime = InferenceStartTime;
    LastGoalDistance = FVector::Dist2D(InferenceAgent->GetAggressiveNavigationReferenceLocation(), ActiveWorldGoal);
    NoProgressSeconds = 0.0f;
    LateralEscapeAttemptCount = 0;
    Policy->RunInference(0.0f);
    GetWorldTimerManager().SetTimer(InferenceTimerHandle, this, &ThisClass::RunInferenceStep, FMath::Max(DecisionInterval, 0.01f), true);

    return true;
}

// 측면 복구 중이 아닐 때 정책을 유지하며 최종 경로 목적지를 갱신한다.
bool ACMCentipedeLearningInferenceCoordinator::UpdateInferenceGoal(const FVector WorldGoal)
{
    if (!bInferenceRunning || bPerformingLateralEscape || !InferenceAgent)
    {
        return false;
    }
    ActiveWorldGoal = WorldGoal;
    if (!InferenceAgent->StartPathMoveToLocation(ActiveWorldGoal, ActiveAcceptanceRadius))
    {
        return false;
    }
    if (!bInferenceRunning || !InferenceAgent)
    {
        return true;
    }
    bWaitingForChaseTargetMove = false;
    LastProgressCheckTime = GetWorld()->GetTimeSeconds();
    LastGoalDistance = FVector::Dist2D(InferenceAgent->GetAggressiveNavigationReferenceLocation(), ActiveWorldGoal);
    NoProgressSeconds = 0.0f;
    LateralEscapeAttemptCount = 0;
    return true;
}

// 이동하는 테스트 목표에 맞춰 경로를 반복 갱신하는 추격 추론을 시작한다.
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

// Centipede 관측·다리 행동 Interactor와 저장 정책을 단일 에이전트용으로 생성한다.
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

// 테스트 목표가 임계 거리 이상 이동했을 때 진행 통계를 초기화하고 경로를 다시 만든다.
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
    if (!bInferenceRunning || !InferenceAgent)
        return true;

    bWaitingForChaseTargetMove = false;
    LastProgressCheckTime = World->GetTimeSeconds();
    LastGoalDistance = FVector::Dist2D(InferenceAgent->GetAggressiveNavigationReferenceLocation(), ActiveWorldGoal);
    NoProgressSeconds = 0.0f;
    LateralEscapeAttemptCount = 0;
    return true;
}

// 몸통 축과 목표 방향을 기준으로 막히지 않은 측면을 골라 전신 정체 복구를 시작한다.
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

    // 첫 방향이 충분한 이탈을 만들지 못한 재시도에서는 반대쪽 공간을 검사한다.
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
    return true;
}

// 최소 이탈 거리를 확보할 때까지 전신 측면 속도를 유지하고 실패하면 반대쪽을 재시도한다.
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

// 측면 이탈 위치에서 머리·꼬리 경로를 다시 비교하고 최종 목적지 추론을 재개한다.
bool ACMCentipedeLearningInferenceCoordinator::RebuildPathAfterLateralEscape()
{
    UWorld* World = GetWorld();
    if (!World || !InferenceAgent)
        return false;

    bPerformingLateralEscape = false;
    InferenceAgent->StopArticulatedBodyMotion();
    if (!InferenceAgent->StartPathMoveToLocation(ActiveWorldGoal, ActiveAcceptanceRadius))
        return false;
    if (!bInferenceRunning || !InferenceAgent)
        return true;

    LastProgressCheckTime = World->GetTimeSeconds();
    LastGoalDistance = FVector::Dist2D(InferenceAgent->GetAggressiveNavigationReferenceLocation(), ActiveWorldGoal);
    NoProgressSeconds = 0.0f;
    return true;
}

// 모든 몸통 세그먼트의 측면 탐색선이 정적 장애물을 피하는지 확인한다.
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

// 관절 형태를 보존하도록 모든 몸통 세그먼트에 같은 측면 평면 속도를 적용한다.
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

// 제한시간·추격 갱신·진행 정체를 확인한 뒤 저장 정책 행동을 한 번 실행한다.
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

    // 일시적인 관절 흔들림이 아니라 누적된 목표 거리 감소량으로 정체를 판정한다.
    if (CurrentTime - LastProgressCheckTime >= FMath::Max(ProgressCheckInterval, 0.1f))
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

// 추론 타이머와 경로·관절 이동 및 학습 에이전트를 정리하고 선택적으로 결과를 알린다.
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

// 경로 결과를 Centipede 결과로 변환하되 테스트 목표 도착은 다음 이동까지 대기한다.
void ACMCentipedeLearningInferenceCoordinator::HandlePathMoveCompleted(ECMAggressivePathMoveResult Result)
{
    // 움직이는 테스트 목표는 현재 위치 도착을 추론 종료가 아닌 대기 상태로 취급한다.
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
