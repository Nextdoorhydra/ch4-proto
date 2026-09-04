#include "Aggressive/Tetra/Learning/CMTetraLearningInferenceCoordinator.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"
#include "Aggressive/Common/Behavior/CMAggressiveBehaviorComponent.h"
#include "Aggressive/Tetra/CMTetraPawn.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "Aggressive/Tetra/Learning/CMTetraLearningInteractor.h"
#include "Aggressive/Common/Learning/CMAggressiveLearningSnapshot.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsPolicy.h"
#include "Aggressive/Common/Movement/CMAggressiveAccelerationMovementComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "Player/CMChimera.h"
#include "Testing/CMAggressiveChaseTestTarget.h"
#include "TimerManager.h"

// Actor Tick 없이 Tetra 정책 추론과 방해 행동을 타이머로 관리한다.
ACMTetraLearningInferenceCoordinator::ACMTetraLearningInferenceCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    LearningManager = CreateDefaultSubobject<ULearningAgentsManager>(TEXT("TetraInferenceManager"));
    LearningManager->PrimaryComponentTick.bCanEverTick = false;
    LearningManager->PrimaryComponentTick.bStartWithTickEnabled = false;
}

// Actor 종료 시 경로 추론과 방해 상태를 모두 정리한다.
void ACMTetraLearningInferenceCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopInferencePath();
    StopInterference();
    Super::EndPlay(EndPlayReason);
}

// 저장 정책을 불러와 Tetra의 NavMesh 경로 목표를 가속도 추론으로 따라가게 한다.
bool ACMTetraLearningInferenceCoordinator::StartInferencePath(ACMTetraPawn* InInferenceAgent, const FVector WorldGoal, const float AcceptanceRadius)
{
    if (!HasAuthority() || bInferencePathRunning || State != ECMTetraInterferenceState::Inactive || !IsValid(InInferenceAgent) || !LearningManager || LearningManager->GetAgentNum() > 0)
    {
        return false;
    }
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
        Policy = nullptr;
        Interactor = nullptr;
        return false;
    }

    if (UCMAggressiveAccelerationMovementComponent* Movement = InferenceAgent->GetAccelerationMovement())
    {
        Movement->SetPolicyMovementEnabled(true);
    }
    if (UCMAggressiveOmnidirectionalPathComponent* Path = InferenceAgent->GetPathMovement())
    {
        Path->SetPolicyControlEnabled(true);
    }
    ActiveInferenceGoal = WorldGoal;
    ActiveInferenceAcceptanceRadius = FMath::Max(AcceptanceRadius, 0.0f);
    bInferencePathRunning = true;
    InferenceAgent->OnPathMoveCompleted.AddUniqueDynamic(this, &ThisClass::HandleInferencePathCompleted);
    if (!InferenceAgent->StartPathMoveToLocation(ActiveInferenceGoal, ActiveInferenceAcceptanceRadius))
    {
        FinishInferencePath();

        return false;
    }
    if (!bInferencePathRunning || !Policy)
    {
        return true;
    }

    Policy->RunInference(0.0f);
    GetWorldTimerManager().SetTimer(InterferenceTimerHandle, this, &ThisClass::RunPathInferenceStep, FMath::Max(DecisionInterval, 0.01f), true);

    return true;
}

// 실행 중인 정책은 유지하면서 최종 경로 목적지만 다시 설정한다.
bool ACMTetraLearningInferenceCoordinator::UpdateInferenceGoal(const FVector WorldGoal)
{
    if (!bInferencePathRunning || !InferenceAgent)
    {
        return false;
    }
    ActiveInferenceGoal = WorldGoal;
    return InferenceAgent->StartPathMoveToLocation(ActiveInferenceGoal, ActiveInferenceAcceptanceRadius);
}

void ACMTetraLearningInferenceCoordinator::StopInferencePath()
{
    FinishInferencePath();
}

// 플레이어 추격·후진·돌진·휴식으로 구성된 Tetra 방해 상태 머신을 시작한다.
bool ACMTetraLearningInferenceCoordinator::StartInterference(ACMTetraPawn* InInferenceAgent, APawn* InTargetPlayer)
{
    if (!HasAuthority() || bInferencePathRunning || State != ECMTetraInterferenceState::Inactive || !IsValid(InInferenceAgent) || !IsValid(InTargetPlayer) || !LearningManager || LearningManager->GetAgentNum() > 0)
        return false;

    if (GetOwner() != InInferenceAgent)
    {
        if (UCMAggressiveBehaviorComponent* Behavior = InInferenceAgent->FindComponentByClass<UCMAggressiveBehaviorComponent>())
        {
            Behavior->SetBehaviorEnabled(false);
        }
    }

    TargetPlayer = InTargetPlayer;
    bChasingTestTarget = false;
    if (!StartTargetTracking(InInferenceAgent))
    {
        TargetPlayer = nullptr;
        return false;
    }
    return true;
}

bool ACMTetraLearningInferenceCoordinator::StartInterferenceWithNearestPlayer(ACMTetraPawn* InInferenceAgent)
{
    return StartInterference(InInferenceAgent, FindNearestPlayerPawn(InInferenceAgent));
}

// 돌진 없이 이동 정책과 재경로 동작만 검증하도록 테스트 목표 추격을 시작한다.
bool ACMTetraLearningInferenceCoordinator::StartChasingTestTarget(ACMTetraPawn* InInferenceAgent, ACMAggressiveChaseTestTarget* InChaseTarget, float AcceptanceRadius)
{
    if (!HasAuthority() || State != ECMTetraInterferenceState::Inactive || !IsValid(InInferenceAgent) || !IsValid(InChaseTarget) || !LearningManager || LearningManager->GetAgentNum() > 0)
        return false;

    ChaseTarget = InChaseTarget;
    TestTargetAcceptanceRadius = FMath::Max(AcceptanceRadius, 0.0f);
    bChasingTestTarget = true;
    if (!StartTargetTracking(InInferenceAgent))
    {
        ChaseTarget = nullptr;
        bChasingTestTarget = false;
        return false;
    }

    return true;
}

// 충돌 구독과 이동 제어 및 학습 에이전트를 해제하고 방해 상태를 초기화한다.
void ACMTetraLearningInferenceCoordinator::StopInterference()
{
    GetWorldTimerManager().ClearTimer(InterferenceTimerHandle);
    if (InferenceAgent)
    {
        if (UBoxComponent* Body = InferenceAgent->GetPhysicsRoot())
            Body->OnComponentHit.RemoveDynamic(this, &ThisClass::HandleAgentHit);
        InferenceAgent->StopPathMove();
        if (UCMAggressiveOmnidirectionalPathComponent* Path = InferenceAgent->GetPathMovement())
            Path->SetPolicyControlEnabled(false);
        if (UCMAggressiveAccelerationMovementComponent* Movement = InferenceAgent->GetAccelerationMovement())
        {
            Movement->StopMovement();
            Movement->SetPolicyMovementEnabled(true);
        }
    }
    if (LearningManager && LearningManager->GetAgentNum() > 0)
        LearningManager->RemoveAllAgents();

    State = ECMTetraInterferenceState::Inactive;
    InferenceAgentId = INDEX_NONE;
    InferenceAgent = nullptr;
    TargetPlayer = nullptr;
    ChaseTarget = nullptr;
    LastChaseGoal = FVector::ZeroVector;
    ChaseProgressLocation = FVector::ZeroVector;
    BackUpStartLocation = FVector::ZeroVector;
    BackUpDirection = FVector::ZeroVector;
    DashDirection = FVector::ZeroVector;
    StateStartTime = 0.0;
    ChaseProgressStartTime = 0.0;
    TestTargetAcceptanceRadius = 50.0f;
    bChasingTestTarget = false;
}

bool ACMTetraLearningInferenceCoordinator::IsInterferenceRunning() const
{
    return State != ECMTetraInterferenceState::Inactive;
}

ECMTetraInterferenceState ACMTetraLearningInferenceCoordinator::GetInterferenceState() const
{
    return State;
}

FString ACMTetraLearningInferenceCoordinator::GetSnapshotDirectory() const
{
    return CMAggressiveLearningSnapshot::GetLatestDirectory(ECMAggressiveLearningSnapshotProfile::Tetra);
}

// Tetra 관측·행동 Interactor와 저장 정책을 단일 추론 에이전트용으로 생성한다.
bool ACMTetraLearningInferenceCoordinator::InitializeInferenceObjects()
{
    ULearningAgentsManager* Manager = LearningManager;
    Interactor = UCMTetraLearningInteractor::MakeTetraInteractor(Manager, TEXT("TetraInferenceInteractor"));
    if (!Interactor)
        return false;

    ULearningAgentsInteractor* BaseInteractor = Interactor;
    const FLearningAgentsPolicySettings PolicySettings = UCMTetraLearningInteractor::GetPolicySettings();
    Policy = ULearningAgentsPolicy::MakePolicy(Manager, BaseInteractor, ULearningAgentsPolicy::StaticClass(), TEXT("TetraInferencePolicy"), nullptr, nullptr, nullptr, true, true, true, PolicySettings);

    return Policy && CMAggressiveLearningSnapshot::LoadInferenceNetworks(ECMAggressiveLearningSnapshotProfile::Tetra, *Policy);
}

// 타깃 충돌을 구독하고 첫 추격 경로와 방해 판단 타이머를 시작한다.
bool ACMTetraLearningInferenceCoordinator::StartTargetTracking(ACMTetraPawn* InInferenceAgent)
{
    if (!HasAuthority() || State != ECMTetraInterferenceState::Inactive || !IsValid(InInferenceAgent) || !IsValid(GetActiveTarget()) || !LearningManager || LearningManager->GetAgentNum() > 0)
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

    if (UBoxComponent* Body = InferenceAgent->GetPhysicsRoot())
    {
        Body->SetNotifyRigidBodyCollision(true);
        Body->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleAgentHit);
    }
    if (!BeginChase())
    {
        StopInterference();

        return false;
    }

    GetWorldTimerManager().SetTimer(InterferenceTimerHandle, this, &ThisClass::RunInterferenceStep, FMath::Max(DecisionInterval, 0.01f), true);

    return true;
}

// 정책 이동과 경로 제어를 활성화하고 현재 타깃을 향한 추격 상태로 전환한다.
bool ACMTetraLearningInferenceCoordinator::BeginChase()
{
    AActor* ActiveTarget = GetActiveTarget();
    if (!IsValid(InferenceAgent) || !IsValid(ActiveTarget))
        return false;

    if (UCMAggressiveAccelerationMovementComponent* Movement = InferenceAgent->GetAccelerationMovement())
        Movement->SetPolicyMovementEnabled(true);
    if (UCMAggressiveOmnidirectionalPathComponent* Path = InferenceAgent->GetPathMovement())
        Path->SetPolicyControlEnabled(true);

    LastChaseGoal = ActiveTarget->GetActorLocation();
    const float AcceptanceRadius = bChasingTestTarget ? TestTargetAcceptanceRadius : ApproachDistance;
    if (!InferenceAgent->StartPathMoveToLocation(LastChaseGoal, AcceptanceRadius))
        return false;

    State = ECMTetraInterferenceState::Chasing;
    StateStartTime = GetWorld()->GetTimeSeconds();
    ResetChaseProgress();
    Policy->RunInference(0.0f);

    return true;
}

// 타깃 반대 방향으로 거리를 확보하는 후진 단계로 전환한다.
void ACMTetraLearningInferenceCoordinator::BeginBackUp()
{
    AActor* ActiveTarget = GetActiveTarget();
    const FVector Direction = InferenceAgent->GetAggressiveNavigationReferenceLocation() - ActiveTarget->GetActorLocation();
    BeginCodeControlledBackUp(Direction, ECMTetraInterferenceState::BackingUp);
}

// 경로 요구 방향의 반대쪽을 우선 선택해 정체 후진 복구를 시작한다.
void ACMTetraLearningInferenceCoordinator::BeginStuckRecovery()
{
    FVector Direction = FVector::ZeroVector;
    UPrimitiveComponent* Body = InferenceAgent->GetAggressiveMovementBody();
    if (const UCMAggressiveOmnidirectionalPathComponent* Path = InferenceAgent->GetPathMovement())
    {
        const FVector LocalMoveDirection = CMAggressiveDirection::ToLocalUnitVector(Path->GetRequestedMoveDirection());
        if (!LocalMoveDirection.IsNearlyZero() && Body)
        {
            const FQuat BodyYaw(FVector::UpVector, FMath::DegreesToRadians(Body->GetComponentRotation().Yaw));
            Direction = -BodyYaw.RotateVector(LocalMoveDirection);
        }
    }
    if (Direction.IsNearlyZero() && Body)
        Direction = -Body->GetPhysicsLinearVelocity();
    if (Direction.IsNearlyZero())
        Direction = InferenceAgent->GetAggressiveNavigationReferenceLocation() - GetActiveTarget()->GetActorLocation();

    BeginCodeControlledBackUp(Direction, ECMTetraInterferenceState::RecoveringFromStuck);
}

// 정책 경로를 멈추고 직접 평면 속도로 제어하는 후진 상태를 시작한다.
void ACMTetraLearningInferenceCoordinator::BeginCodeControlledBackUp(FVector Direction, ECMTetraInterferenceState BackUpState)
{
    // 직접 속도 제어 중에는 정책 가속도가 후진 속도를 덮어쓰지 않게 한다.
    InferenceAgent->StopPathMove();
    if (UCMAggressiveAccelerationMovementComponent* Movement = InferenceAgent->GetAccelerationMovement())
    {
        Movement->StopMovement();
        Movement->SetPolicyMovementEnabled(false);
    }

    Direction.Z = 0.0f;
    if (!Direction.Normalize())
        Direction = -InferenceAgent->GetActorForwardVector();
    BackUpStartLocation = InferenceAgent->GetAggressiveNavigationReferenceLocation();
    BackUpDirection = Direction;
    State = BackUpState;
    StateStartTime = GetWorld()->GetTimeSeconds();
    SetCodePlanarVelocity(BackUpDirection, BackUpSpeed);
}

// 후진으로 확보한 공간에서 타깃을 향한 직접 속도 돌진을 시작한다.
void ACMTetraLearningInferenceCoordinator::BeginDash()
{
    DashDirection = GetActiveTarget()->GetActorLocation() - InferenceAgent->GetAggressiveNavigationReferenceLocation();
    DashDirection.Z = 0.0f;
    if (!DashDirection.Normalize())
        DashDirection = -BackUpDirection;
    State = ECMTetraInterferenceState::Dashing;
    StateStartTime = GetWorld()->GetTimeSeconds();
    SetCodePlanarVelocity(DashDirection, DashSpeed);
}

void ACMTetraLearningInferenceCoordinator::BeginRest()
{
    if (UCMAggressiveAccelerationMovementComponent* Movement = InferenceAgent->GetAccelerationMovement())
        Movement->StopMovement();
    State = ECMTetraInterferenceState::Resting;
    StateStartTime = GetWorld()->GetTimeSeconds();
}

// 현재 방해 상태에 해당하는 추격·후진·돌진·휴식 단계를 한 번 갱신한다.
void ACMTetraLearningInferenceCoordinator::RunInterferenceStep()
{
    if (!IsValid(InferenceAgent) || !IsValid(GetActiveTarget()) || !Policy)
    {
        StopInterference();

        return;
    }

    switch (State)
    {
    case ECMTetraInterferenceState::Chasing:
        UpdateChase();
        break;
    case ECMTetraInterferenceState::BackingUp:
        UpdateBackUp();
        break;
    case ECMTetraInterferenceState::RecoveringFromStuck:
        UpdateStuckRecovery();
        break;
    case ECMTetraInterferenceState::Dashing:
        UpdateDash();
        break;
    case ECMTetraInterferenceState::Resting:
        UpdateRest();
        break;
    default:
        break;
    }
}

// 타깃 이동에 맞춰 경로와 정책을 갱신하고 진행이 없으면 정체 복구로 전환한다.
void ACMTetraLearningInferenceCoordinator::UpdateChase()
{
    AActor* ActiveTarget = GetActiveTarget();
    const FVector AgentLocation = InferenceAgent->GetAggressiveNavigationReferenceLocation();
    const float AcceptanceRadius = bChasingTestTarget ? TestTargetAcceptanceRadius : ApproachDistance;
    if (FVector::DistSquared2D(AgentLocation, ActiveTarget->GetActorLocation()) <= FMath::Square(FMath::Max(AcceptanceRadius, 0.0f)))
    {
        // 테스트 추격은 도착 상태를 유지하고 실제 방해 행동만 후진·돌진으로 이어간다.
        if (bChasingTestTarget)
        {
            if (UCMAggressiveAccelerationMovementComponent* Movement = InferenceAgent->GetAccelerationMovement())
                Movement->StopMovement();

            return;
        }
        BeginBackUp();

        return;
    }

    const FVector TargetLocation = ActiveTarget->GetActorLocation();
    const UCMAggressiveOmnidirectionalPathComponent* Path = InferenceAgent->GetPathMovement();
    const bool bNeedsRestart = bChasingTestTarget && (!Path || !Path->IsPathMoving());
    if (bNeedsRestart || FVector::DistSquared2D(TargetLocation, LastChaseGoal) >= FMath::Square(FMath::Max(ChaseRepathDistance, 1.0f)))
    {
        LastChaseGoal = TargetLocation;
        if (!InferenceAgent->StartPathMoveToLocation(LastChaseGoal, AcceptanceRadius))
        {
            StopInterference();

            return;
        }
        ResetChaseProgress();
    }

    if (FVector::DistSquared2D(AgentLocation, ChaseProgressLocation) >= FMath::Square(FMath::Max(StuckMovementDistance, 0.0f)))
        ResetChaseProgress();
    else if (GetWorld()->GetTimeSeconds() - ChaseProgressStartTime >= FMath::Max(StuckDetectionSeconds, 0.1f))
    {
        BeginStuckRecovery();

        return;
    }
    Policy->RunInference(0.0f);
}

void ACMTetraLearningInferenceCoordinator::UpdateBackUp()
{
    if (HasBackUpFinished())
    {
        BeginDash();

        return;
    }
    SetCodePlanarVelocity(BackUpDirection, BackUpSpeed);
}

void ACMTetraLearningInferenceCoordinator::UpdateStuckRecovery()
{
    if (HasBackUpFinished())
    {
        if (!BeginChase())
            StopInterference();

        return;
    }
    SetCodePlanarVelocity(BackUpDirection, BackUpSpeed);
}

void ACMTetraLearningInferenceCoordinator::UpdateDash()
{
    if (GetWorld()->GetTimeSeconds() - StateStartTime >= FMath::Max(MaximumDashSeconds, 0.1f))
    {
        BeginRest();

        return;
    }
    SetCodePlanarVelocity(DashDirection, DashSpeed);
}

void ACMTetraLearningInferenceCoordinator::UpdateRest()
{
    if (GetWorld()->GetTimeSeconds() - StateStartTime < FMath::Max(RestSeconds, 0.0f))
        return;
    if (!BeginChase())
        StopInterference();
}

bool ACMTetraLearningInferenceCoordinator::HasBackUpFinished() const
{
    const double Elapsed = GetWorld()->GetTimeSeconds() - StateStartTime;
    const float MovedDistance = FVector::Dist2D(BackUpStartLocation, InferenceAgent->GetAggressiveNavigationReferenceLocation());

    return MovedDistance >= FMath::Max(BackUpDistance, 0.0f) || Elapsed >= FMath::Max(MaximumBackUpSeconds, 0.1f);
}

void ACMTetraLearningInferenceCoordinator::ResetChaseProgress()
{
    ChaseProgressLocation = InferenceAgent->GetAggressiveNavigationReferenceLocation();
    ChaseProgressStartTime = GetWorld()->GetTimeSeconds();
}

void ACMTetraLearningInferenceCoordinator::SetCodePlanarVelocity(FVector Direction, float Speed)
{
    UPrimitiveComponent* Body = InferenceAgent ? InferenceAgent->GetAggressiveMovementBody() : nullptr;
    if (!Body)
        return;

    Direction.Z = 0.0f;
    const FVector PlanarVelocity = Direction.GetSafeNormal() * FMath::Max(Speed, 0.0f);
    FVector Velocity = Body->GetPhysicsLinearVelocity();
    Velocity.X = PlanarVelocity.X;
    Velocity.Y = PlanarVelocity.Y;
    Body->SetPhysicsLinearVelocity(Velocity);
}

void ACMTetraLearningInferenceCoordinator::ApplyKnockbackToTarget()
{
    if (ACMChimera* Chimera = Cast<ACMChimera>(TargetPlayer))
    {
        Chimera->ApplyPlanarKnockback(DashDirection, PlayerKnockbackSpeed);

        return;
    }

    UPrimitiveComponent* TargetBody = TargetPlayer ? Cast<UPrimitiveComponent>(TargetPlayer->GetRootComponent()) : nullptr;
    if (TargetBody && TargetBody->IsSimulatingPhysics())
        TargetBody->AddImpulse(DashDirection * FMath::Max(PlayerKnockbackSpeed, 0.0f), NAME_None, true);
}

// 경로 이동 중 저장 정책의 가속도 행동을 판단 주기마다 실행한다.
void ACMTetraLearningInferenceCoordinator::RunPathInferenceStep()
{
    if (!bInferencePathRunning || !InferenceAgent || !Policy)
    {
        FinishInferencePath();

        return;
    }
    Policy->RunInference(0.0f);
}

// 경로 완료 여부와 관계없이 타이머·이동·학습 에이전트 상태를 일괄 정리한다.
void ACMTetraLearningInferenceCoordinator::FinishInferencePath()
{
    if (!bInferencePathRunning)
    {
        return;
    }
    bInferencePathRunning = false;
    GetWorldTimerManager().ClearTimer(InterferenceTimerHandle);
    if (InferenceAgent)
    {
        InferenceAgent->OnPathMoveCompleted.RemoveDynamic(this, &ThisClass::HandleInferencePathCompleted);
        InferenceAgent->StopPathMove();
        if (UCMAggressiveAccelerationMovementComponent* Movement = InferenceAgent->GetAccelerationMovement())
        {
            Movement->StopMovement();
        }
    }
    if (LearningManager && LearningManager->GetAgentNum() > 0)
    {
        LearningManager->RemoveAllAgents();
    }
    InferenceAgentId = INDEX_NONE;
    InferenceAgent = nullptr;
    Policy = nullptr;
    Interactor = nullptr;
    ActiveInferenceGoal = FVector::ZeroVector;
    ActiveInferenceAcceptanceRadius = 50.0f;
}

void ACMTetraLearningInferenceCoordinator::HandleInferencePathCompleted(ECMAggressivePathMoveResult Result)
{
    FinishInferencePath();
}

APawn* ACMTetraLearningInferenceCoordinator::FindNearestPlayerPawn(const ACMTetraPawn* ReferenceAgent) const
{
    UWorld* World = GetWorld();
    if (!World || !ReferenceAgent)
        return nullptr;

    APawn* NearestPawn = nullptr;
    float NearestDistanceSquared = TNumericLimits<float>::Max();
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APawn* Candidate = It->Get() ? It->Get()->GetPawn() : nullptr;
        if (!IsValid(Candidate))
            continue;
        const float DistanceSquared = FVector::DistSquared2D(ReferenceAgent->GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared < NearestDistanceSquared)
        {
            NearestDistanceSquared = DistanceSquared;
            NearestPawn = Candidate;
        }
    }
    return NearestPawn;
}

AActor* ACMTetraLearningInferenceCoordinator::GetActiveTarget() const
{
    return bChasingTestTarget ? Cast<AActor>(ChaseTarget) : Cast<AActor>(TargetPlayer);
}

// 돌진 중 목표 플레이어와 충돌했을 때만 넉백을 적용하고 휴식으로 전환한다.
void ACMTetraLearningInferenceCoordinator::HandleAgentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (State != ECMTetraInterferenceState::Dashing || OtherActor != TargetPlayer)
        return;

    ApplyKnockbackToTarget();
    BeginRest();
}
