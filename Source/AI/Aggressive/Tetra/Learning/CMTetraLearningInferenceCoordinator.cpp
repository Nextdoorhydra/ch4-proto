#include "Aggressive/Tetra/Learning/CMTetraLearningInferenceCoordinator.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogCMTetraInference, Log, All);

ACMTetraLearningInferenceCoordinator::ACMTetraLearningInferenceCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    LearningManager = CreateDefaultSubobject<ULearningAgentsManager>(TEXT("TetraInferenceManager"));
    LearningManager->PrimaryComponentTick.bCanEverTick = false;
    LearningManager->PrimaryComponentTick.bStartWithTickEnabled = false;
}

void ACMTetraLearningInferenceCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopInterference();
    Super::EndPlay(EndPlayReason);
}

bool ACMTetraLearningInferenceCoordinator::StartInterference(ACMTetraPawn* InInferenceAgent, APawn* InTargetPlayer)
{
    if (!HasAuthority() || State != ECMTetraInterferenceState::Inactive || !IsValid(InInferenceAgent) || !IsValid(InTargetPlayer) || !LearningManager || LearningManager->GetAgentNum() > 0)
        return false;

    TargetPlayer = InTargetPlayer;
    bChasingTestTarget = false;
    if (!StartTargetTracking(InInferenceAgent))
    {
        TargetPlayer = nullptr;
        return false;
    }
    const UCMAggressiveAccelerationMovementComponent* Movement = InferenceAgent->GetAccelerationMovement();
    UE_LOG(LogCMTetraInference, Display, TEXT("Tetra AI 방해 행동을 시작했습니다. 추격 최고속도: %.1fcm/s"), Movement ? Movement->GetMaximumSpeed() : 0.0f);
    return true;
}

bool ACMTetraLearningInferenceCoordinator::StartInterferenceWithNearestPlayer(ACMTetraPawn* InInferenceAgent)
{
    return StartInterference(InInferenceAgent, FindNearestPlayerPawn(InInferenceAgent));
}

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

    UE_LOG(LogCMTetraInference, Display, TEXT("Tetra AI가 추격 테스트 목표 추적을 시작했습니다. 목표: %s"), *ChaseTarget->GetActorLocation().ToCompactString());
    return true;
}

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

void ACMTetraLearningInferenceCoordinator::BeginBackUp()
{
    AActor* ActiveTarget = GetActiveTarget();
    const FVector Direction = InferenceAgent->GetAggressiveNavigationReferenceLocation() - ActiveTarget->GetActorLocation();
    BeginCodeControlledBackUp(Direction, ECMTetraInterferenceState::BackingUp);
}

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
    UE_LOG(LogCMTetraInference, Display, TEXT("Tetra AI가 추격 중 정체되어 후진 복구를 시작합니다."));
}

void ACMTetraLearningInferenceCoordinator::BeginCodeControlledBackUp(FVector Direction, ECMTetraInterferenceState BackUpState)
{
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

void ACMTetraLearningInferenceCoordinator::RunInterferenceStep()
{
    if (!IsValid(InferenceAgent) || !IsValid(GetActiveTarget()) || !Policy)
    {
        StopInterference();
        return;
    }

    switch (State)
    {
    case ECMTetraInterferenceState::Chasing: UpdateChase(); break;
    case ECMTetraInterferenceState::BackingUp: UpdateBackUp(); break;
    case ECMTetraInterferenceState::RecoveringFromStuck: UpdateStuckRecovery(); break;
    case ECMTetraInterferenceState::Dashing: UpdateDash(); break;
    case ECMTetraInterferenceState::Resting: UpdateRest(); break;
    default: break;
    }
}

void ACMTetraLearningInferenceCoordinator::UpdateChase()
{
    AActor* ActiveTarget = GetActiveTarget();
    const FVector AgentLocation = InferenceAgent->GetAggressiveNavigationReferenceLocation();
    const float AcceptanceRadius = bChasingTestTarget ? TestTargetAcceptanceRadius : ApproachDistance;
    if (FVector::DistSquared2D(AgentLocation, ActiveTarget->GetActorLocation()) <= FMath::Square(FMath::Max(AcceptanceRadius, 0.0f)))
    {
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

void ACMTetraLearningInferenceCoordinator::HandleAgentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (State != ECMTetraInterferenceState::Dashing || OtherActor != TargetPlayer)
        return;

    ApplyKnockbackToTarget();
    BeginRest();
}
