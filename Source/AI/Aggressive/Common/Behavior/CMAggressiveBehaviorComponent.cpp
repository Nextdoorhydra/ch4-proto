#include "Aggressive/Common/Behavior/CMAggressiveBehaviorComponent.h"

#include "Aggressive/Centipede/CMCentipedePawn.h"
#include "Aggressive/Centipede/Learning/CMCentipedeLearningInferenceCoordinator.h"
#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"
#include "Aggressive/Common/Core/CMAggressivePawnBase.h"
#include "Aggressive/Common/Movement/CMAggressiveAccelerationMovementComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveKnockbackComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "Aggressive/Common/Perception/CMAggressiveSightComponent.h"
#include "Aggressive/Ripper/CMRipperPawn.h"
#include "Aggressive/Ripper/Learning/CMRipperLearningInferenceCoordinator.h"
#include "Aggressive/Tetra/CMTetraPawn.h"
#include "Aggressive/Tetra/Learning/CMTetraLearningInferenceCoordinator.h"
#include "DrawDebugHelpers.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "GameMode/CMGameState.h"
#include "Gore/CMDismemberableTarget.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Player/CMChimera.h"
#include "Player/CMControlTypes.h"
#include "Player/CMPartSlotComponent.h"
#include "Sacrifice/CMSacrificeCharacter.h"
#include "Sacrifice/CMSacrificeStateComponent.h"
#include "Sound/CMSoundPlayback.h"
#include "Sound/CMSoundTags.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMAggressiveBehavior, Log, All);

namespace CMAggressiveBehavior
{
    constexpr float UpdateInterval = 0.1f;
    constexpr float TetraWanderRadius = 1500.0f;
    constexpr float RipperWanderRadius = 1200.0f;
    constexpr float CentipedeWanderRadius = 1000.0f;
    constexpr float WanderAcceptanceRadius = 50.0f;
    constexpr float MinimumWanderTravelDistance = 200.0f;
    constexpr int32 WanderGoalSampleAttempts = 12;
    constexpr float ReturnAcceptanceRadius = 50.0f;
    constexpr float TetraDashStartDistance = 500.0f;
    constexpr float RepathDistance = 75.0f;
    constexpr float TetraSightTurnMinInterval = 3.0f;
    constexpr float TetraSightTurnMaxInterval = 5.0f;
    constexpr float TetraSightTurnDegrees = 90.0f;
    constexpr float TetraDashDuration = 1.0f;
    constexpr float TetraDashSpeedMultiplier = 3.0f;
    constexpr float TetraSelfKnockbackDistance = 300.0f;
    constexpr float TetraPlayerKnockbackDistance = 150.0f;
    constexpr float StuckDetectionSeconds = 3.0f;
    constexpr float StuckMovementDistance = 15.0f;
    constexpr float RipperStuckDetectionSeconds = 3.0f;
    constexpr float RipperStuckMovementDistance = 20.0f;
    constexpr float StuckReverseDuration = 0.75f;
    constexpr float StuckReverseSpeed = 250.0f;
    constexpr float FailedMoveRetryInterval = 1.0f;
    constexpr float RipperAttackCooldownSeconds = 10.0f;

#if !UE_BUILD_SHIPPING
    bool bDrawWanderGoalDebug = false;
    TSet<TWeakObjectPtr<UWorld>> InitializedWanderDebugWorlds;

    void InitializeWanderGoalDebugForWorld(UWorld* World)
    {
        if (!World)
        {
            return;
        }
        for (auto It = InitializedWanderDebugWorlds.CreateIterator(); It; ++It)
        {
            if (!It->IsValid())
            {
                It.RemoveCurrent();
            }
        }
        if (!InitializedWanderDebugWorlds.Contains(World))
        {
            InitializedWanderDebugWorlds.Add(World);
            bDrawWanderGoalDebug = false;
        }
    }

    void SetWanderGoalDebugDraw(const TArray<FString>& Args, UWorld* World)
    {
        if (Args.Num() != 1)
        {
            return;
        }

        const FString& Value = Args[0];
        bool bEnabled = false;
        if (Value.Equals(TEXT("on"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value == TEXT("1"))
        {
            bEnabled = true;
        }
        else if (Value.Equals(TEXT("off"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Value == TEXT("0"))
        {
            bEnabled = false;
        }
        else
        {
            UE_LOG(LogCMAggressiveBehavior, Warning, TEXT("Invalid value '%s'. Usage: CM.AI.WanderDebug on|off"), *Value);

            return;
        }

        bDrawWanderGoalDebug = bEnabled;
    }

    FAutoConsoleCommandWithWorldAndArgs WanderGoalDebugCommand(TEXT("CM.AI.WanderDebug"), TEXT("Draws hostile AI random wander goals. Usage: CM.AI.WanderDebug on|off"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetWanderGoalDebugDraw));
#endif
} // namespace CMAggressiveBehavior

bool CMAggressiveBehaviorRules::HasMadeGoalProgress(const float PreviousDistance, const float CurrentDistance, const float RequiredProgressDistance)
{
    return PreviousDistance - CurrentDistance >= FMath::Max(RequiredProgressDistance, 0.0f);
}

bool CMAggressiveBehaviorRules::IsRipperAttackReady(const double CurrentTime, const double NextAttackTime)
{
    return CurrentTime >= NextAttackTime;
}

UCMAggressiveBehaviorComponent::UCMAggressiveBehaviorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickInterval = CMAggressiveBehavior::UpdateInterval;
    SetIsReplicatedByDefault(true);
}

void UCMAggressiveBehaviorComponent::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    CMAggressiveBehavior::InitializeWanderGoalDebugForWorld(GetWorld());
#endif
    OwnerPawn = Cast<ACMAggressivePawnBase>(GetOwner());
    CachedMovementAgent = Cast<ICMAggressiveMovementAgent>(OwnerPawn);
    if (OwnerPawn)
    {
        OwnerPawn->GetComponents(SightComponents);
    }
    if (OwnerPawn && OwnerPawn->HasAuthority() && bAutoStartBehavior)
    {
        InitializeRuntimeBehavior();
    }
}

void UCMAggressiveBehaviorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SetBehaviorEnabled(false);
    DestroyInferenceCoordinator();
    SightComponents.Reset();
    CachedMovementAgent = nullptr;
    OwnerPawn = nullptr;
    Super::EndPlay(EndPlayReason);
}

void UCMAggressiveBehaviorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    DrawWanderGoalDebug();
}

void UCMAggressiveBehaviorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, LastWanderGoal);
    DOREPLIFETIME(ThisClass, bHasWanderGoal);
}

void UCMAggressiveBehaviorComponent::ConfigureProfile(const ECMAggressiveBehaviorProfile InProfile)
{
    Profile = InProfile;
}

// 서버 행동 상태 머신과 충돌 구독 및 반복 타이머를 일관되게 시작하거나 정리한다.
void UCMAggressiveBehaviorComponent::SetBehaviorEnabled(const bool bEnabled)
{
    if (!OwnerPawn || !OwnerPawn->HasAuthority() || bBehaviorEnabled == bEnabled)
    {
        return;
    }

    bBehaviorEnabled = bEnabled;
    if (Profile == ECMAggressiveBehaviorProfile::Tetra)
    {
        if (ACMTetraPawn* Tetra = Cast<ACMTetraPawn>(OwnerPawn))
        {
            Tetra->GetAccelerationMovement()->SetMaximumSpeedMultiplier(1.0f);
        }
    }
    if (!bEnabled)
    {
        GetWorld()->GetTimerManager().ClearTimer(BehaviorTimerHandle);
        bHasWanderGoal = false;
        StopMove();
        OwnerPawn->OnActorHit.RemoveDynamic(this, &ThisClass::HandleOwnerHit);
        CurrentTarget = nullptr;
        bReversingFromStuck = false;
        bCompletingStuckRecoveryMove = false;
        StuckProgressStartTime = 0.0;
        return;
    }

    SpawnLocation = GetNavigationLocation();
    State = ECMAggressiveAIState::Searching;
    NextActionTime = 0.0;
    NextRipperAttackTime = 0.0;
    NextTetraSightTurnTime = 0.0;
    bMoveIssued = false;
    bReturningHome = false;
    bHasWanderGoal = false;
    bReversingFromStuck = false;
    bCompletingStuckRecoveryMove = false;
    ResetStuckTracking();
    OwnerPawn->OnActorHit.AddUniqueDynamic(this, &ThisClass::HandleOwnerHit);
    if (UPrimitiveComponent* Body = GetMovementBody())
    {
        Body->SetNotifyRigidBodyCollision(true);
    }
    GetWorld()->GetTimerManager().SetTimer(BehaviorTimerHandle, this, &ThisClass::UpdateBehavior, CMAggressiveBehavior::UpdateInterval, true);
}

// AI 종류에 맞는 정책 추론 조정자를 생성하고 런타임 행동을 시작한다.
void UCMAggressiveBehaviorComponent::InitializeRuntimeBehavior()
{
    UWorld* World = GetWorld();
    if (!World || !OwnerPawn)
    {
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = OwnerPawn;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    switch (Profile)
    {
    case ECMAggressiveBehaviorProfile::Tetra:
        TetraCoordinator = World->SpawnActor<ACMTetraLearningInferenceCoordinator>(SpawnParameters);
        break;
    case ECMAggressiveBehaviorProfile::Ripper:
        RipperCoordinator = World->SpawnActor<ACMRipperLearningInferenceCoordinator>(SpawnParameters);
        break;
    case ECMAggressiveBehaviorProfile::Centipede:
        CentipedeCoordinator = World->SpawnActor<ACMCentipedeLearningInferenceCoordinator>(SpawnParameters);
        break;
    }
    SetBehaviorEnabled(true);
}

// 새 타깃 감지를 우선 처리한 뒤 정체 복구와 현재 탐색·추격·공격 행동을 갱신한다.
void UCMAggressiveBehaviorComponent::UpdateBehavior()
{
    UWorld* World = GetWorld();
    if (!bBehaviorEnabled || !World || !OwnerPawn)
    {
        return;
    }

    const double CurrentTime = World->GetTimeSeconds();
    RecoverToNavigation();
    UpdateTetraSightScan(CurrentTime);
    // 정체 복구 중에도 새 타깃 감지는 우선 처리해 눈앞의 희생양을 놓치지 않는다.
    if (State == ECMAggressiveAIState::Searching)
    {
        if (AActor* SeenTarget = FindVisibleTarget())
        {
            bReversingFromStuck = false;
            bCompletingStuckRecoveryMove = false;
            StopMove();
            BeginChasing(SeenTarget);
            return;
        }
    }
    // 복구 동작 중에는 일반 상태 머신이 새 이동 명령으로 복구를 덮어쓰지 않게 한다.
    if (UpdateStuckRecovery(CurrentTime) || UpdateStuckDetection(CurrentTime))
    {
        return;
    }
    switch (State)
    {
    case ECMAggressiveAIState::Searching:
        UpdateSearching(CurrentTime);
        break;
    case ECMAggressiveAIState::Chasing:
        UpdateChasing();
        break;
    case ECMAggressiveAIState::Attacking:
        UpdateAttacking(CurrentTime);
        break;
    case ECMAggressiveAIState::Waiting:
        UpdateWaiting(CurrentTime);
        break;
    default:
        break;
    }
}

// 물리 몸체가 전용 NavMesh 밖으로 밀리면 전체 몸체를 가장 가까운 유효 위치로 복귀시킨다.
bool UCMAggressiveBehaviorComponent::RecoverToNavigation()
{
    UCMAggressiveOmnidirectionalPathComponent* PathMovement = OwnerPawn ? OwnerPawn->FindComponentByClass<UCMAggressiveOmnidirectionalPathComponent>() : nullptr;
    FVector RecoveryLocation;
    if (!PathMovement || !PathMovement->FindNavigationRecoveryLocation(RecoveryLocation))
    {
        return false;
    }

    const FVector CurrentLocation = GetNavigationLocation();
    FVector RecoveryOffset = RecoveryLocation - CurrentLocation;
    RecoveryOffset.Z = 0.0f;
    StopMove();
    if (ACMCentipedePawn* Centipede = Cast<ACMCentipedePawn>(OwnerPawn))
    {
        for (UBoxComponent* Segment : Centipede->GetBodySegments())
        {
            if (!Segment)
            {
                continue;
            }
            Segment->SetWorldLocation(Segment->GetComponentLocation() + RecoveryOffset, false, nullptr, ETeleportType::TeleportPhysics);
            Segment->SetPhysicsLinearVelocity(FVector::ZeroVector);
            Segment->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        }
    }
    else if (UPrimitiveComponent* Body = GetMovementBody())
    {
        Body->SetWorldLocation(Body->GetComponentLocation() + RecoveryOffset, false, nullptr, ETeleportType::TeleportPhysics);
        Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
        Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    }

    if (Profile == ECMAggressiveBehaviorProfile::Tetra && State == ECMAggressiveAIState::Attacking)
    {
        CastChecked<ACMTetraPawn>(OwnerPawn)->GetAccelerationMovement()->SetMaximumSpeedMultiplier(1.0f);
        const bool bHasValidTarget = IsValidTarget(CurrentTarget);
        State = bHasValidTarget ? ECMAggressiveAIState::Chasing : ECMAggressiveAIState::Searching;
        bReturningHome = !bHasValidTarget;
        if (!bHasValidTarget)
        {
            CurrentTarget = nullptr;
        }
    }
    bMoveIssued = false;
    bReversingFromStuck = false;
    bCompletingStuckRecoveryMove = false;
    NextActionTime = 0.0;
    ResetStuckTracking();
    OwnerPawn->ForceNetUpdate();
    return true;
}

// 후진과 새 임의 이동으로 구성된 정체 복구 단계를 진행한다.
bool UCMAggressiveBehaviorComponent::UpdateStuckRecovery(const double CurrentTime)
{
    if (bReversingFromStuck)
    {
        ApplyStuckRecoveryReverseVelocity();
        if (CurrentTime < StuckReverseEndTime)
        {
            return true;
        }

        bReversingFromStuck = false;
        if (Profile == ECMAggressiveBehaviorProfile::Ripper && State == ECMAggressiveAIState::Chasing && IsValidTarget(CurrentTarget))
        {
            if (!CanPursueTarget(*CurrentTarget))
            {
                BeginReturningHome();
                ResetStuckTracking();
                return true;
            }
            bMoveIssued = StartMove(CurrentTarget->GetActorLocation(), GetAttackDistance());
            bCompletingStuckRecoveryMove = false;
            ResetStuckTracking();
            return bMoveIssued;
        }
        bMoveIssued = BeginRandomMove();
        bCompletingStuckRecoveryMove = bMoveIssued;
        ResetStuckTracking();

        return bCompletingStuckRecoveryMove;
    }

    if (!bCompletingStuckRecoveryMove)
    {
        return false;
    }
    if (IsMoveRunning())
    {
        return true;
    }

    bCompletingStuckRecoveryMove = false;
    bMoveIssued = false;
    NextActionTime = CurrentTime;
    ResetStuckTracking();

    return false;
}

// 이동 명령 중 목적지까지의 거리 감소가 일정 시간 부족하면 정체 복구를 시작한다.
bool UCMAggressiveBehaviorComponent::UpdateStuckDetection(const double CurrentTime)
{
    const bool bShouldTrack = bMoveIssued && IsMoveRunning() && (State == ECMAggressiveAIState::Searching || State == ECMAggressiveAIState::Chasing);
    if (!bShouldTrack)
    {
        StuckProgressStartTime = 0.0;
        return false;
    }

    const FVector CurrentLocation = GetNavigationLocation();
    const float RequiredProgressDistance = Profile == ECMAggressiveBehaviorProfile::Ripper ? CMAggressiveBehavior::RipperStuckMovementDistance : CMAggressiveBehavior::StuckMovementDistance;
    const float DetectionSeconds = Profile == ECMAggressiveBehaviorProfile::Ripper ? CMAggressiveBehavior::RipperStuckDetectionSeconds : CMAggressiveBehavior::StuckDetectionSeconds;
    if (StuckProgressStartTime <= 0.0)
    {
        ResetStuckTracking();
        return false;
    }
    const float PreviousGoalDistance = FVector::Dist2D(StuckProgressLocation, LastMoveGoal);
    const float CurrentGoalDistance = FVector::Dist2D(CurrentLocation, LastMoveGoal);
    if (CMAggressiveBehaviorRules::HasMadeGoalProgress(PreviousGoalDistance, CurrentGoalDistance, RequiredProgressDistance))
    {
        ResetStuckTracking();
        return false;
    }
    if (CurrentTime - StuckProgressStartTime < DetectionSeconds)
    {
        return false;
    }

    BeginStuckRecovery(CurrentTime);

    return true;
}

// 현재 목표 반대쪽으로 후진하도록 상태와 이동 명령을 복구 모드로 전환한다.
void UCMAggressiveBehaviorComponent::BeginStuckRecovery(const double CurrentTime)
{
    const FVector CurrentLocation = GetNavigationLocation();
    StuckReverseDirection = (CurrentLocation - LastMoveGoal).GetSafeNormal2D();
    if (StuckReverseDirection.IsNearlyZero())
    {
        const UPrimitiveComponent* Body = GetMovementBody();
        StuckReverseDirection = Body ? -Body->GetForwardVector().GetSafeNormal2D() : -OwnerPawn->GetActorForwardVector().GetSafeNormal2D();
    }

    StopMove();
    const bool bKeepCombatTarget = (Profile == ECMAggressiveBehaviorProfile::Ripper || Profile == ECMAggressiveBehaviorProfile::Centipede) && IsValidTarget(CurrentTarget);
    if (!bKeepCombatTarget)
    {
        CurrentTarget = nullptr;
        State = ECMAggressiveAIState::Searching;
    }
    bReturningHome = false;
    bHasWanderGoal = false;
    bReversingFromStuck = true;
    bCompletingStuckRecoveryMove = false;
    StuckReverseEndTime = CurrentTime + CMAggressiveBehavior::StuckReverseDuration;
    ApplyStuckRecoveryReverseVelocity();

}

void UCMAggressiveBehaviorComponent::ApplyStuckRecoveryReverseVelocity() const
{
    const FVector PlanarVelocity = StuckReverseDirection * CMAggressiveBehavior::StuckReverseSpeed;
    auto ApplyVelocity = [&PlanarVelocity](UPrimitiveComponent* Body)
    {
        if (!Body || !Body->IsSimulatingPhysics())
        {
            return;
        }
        FVector Velocity = Body->GetPhysicsLinearVelocity();
        Velocity.X = PlanarVelocity.X;
        Velocity.Y = PlanarVelocity.Y;
        Body->SetPhysicsLinearVelocity(Velocity);
        Body->WakeAllRigidBodies();
    };

    if (const ACMCentipedePawn* Centipede = Cast<ACMCentipedePawn>(OwnerPawn))
    {
        for (UBoxComponent* Segment : Centipede->GetBodySegments())
        {
            ApplyVelocity(Segment);
        }
        return;
    }
    ApplyVelocity(GetMovementBody());
}

void UCMAggressiveBehaviorComponent::ResetStuckTracking()
{
    StuckProgressLocation = GetNavigationLocation();
    const UWorld* World = GetWorld();
    StuckProgressStartTime = World ? World->GetTimeSeconds() : 0.0;
}

// 시야에 들어온 타깃을 추격하거나 배회와 귀환 행동을 이어간다.
void UCMAggressiveBehaviorComponent::UpdateSearching(const double CurrentTime)
{
    if (AActor* SeenTarget = FindVisibleTarget())
    {
        BeginChasing(SeenTarget);

        return;
    }

    if (bReturningHome && FVector::DistSquared2D(GetNavigationLocation(), SpawnLocation) <= FMath::Square(CMAggressiveBehavior::ReturnAcceptanceRadius))
    {
        bReturningHome = false;
        bMoveIssued = false;
        NextActionTime = CurrentTime;
    }

    if (bMoveIssued && IsMoveRunning())
    {
        return;
    }
    if (bMoveIssued)
    {
        bMoveIssued = false;
        if (bReturningHome)
        {
            bReturningHome = false;
            NextActionTime = CurrentTime;
        }
        else
        {
            const bool bTetra = Profile == ECMAggressiveBehaviorProfile::Tetra;
            NextActionTime = CurrentTime + FMath::FRandRange(bTetra ? 3.0f : 5.0f, bTetra ? 5.0f : 7.0f);
        }
    }
    if (CurrentTime < NextActionTime)
    {
        return;
    }

    if (bReturningHome)
    {
        bMoveIssued = StartMove(SpawnLocation, CMAggressiveBehavior::ReturnAcceptanceRadius);
    }
    else
    {
        bMoveIssued = BeginRandomMove();
    }
    if (!bMoveIssued)
    {
        NextActionTime = CurrentTime + 1.0f;
    }
}

// 더 적합한 타깃과 공격 거리를 확인하며 경로 목표를 갱신한다.
void UCMAggressiveBehaviorComponent::UpdateChasing()
{
    if (!IsValidTarget(CurrentTarget))
    {
        BeginReturningHome();

        return;
    }

    if (Profile == ECMAggressiveBehaviorProfile::Ripper)
    {
        ACMSacrificeCharacter* CurrentSacrifice = Cast<ACMSacrificeCharacter>(CurrentTarget);
        if (CurrentSacrifice)
        {
            if (ACMSacrificeCharacter* CloserSacrifice = FindCloserVisibleSacrifice(*CurrentSacrifice))
            {
                CurrentTarget = CloserSacrifice;
                bMoveIssued = StartMove(CurrentTarget->GetActorLocation(), GetAttackDistance());
            }
        }
    }

    const FVector TargetLocation = CurrentTarget->GetActorLocation();
    const float AttackDistance = GetAttackDistance();
    const float DistanceSquared = FVector::DistSquared2D(GetAttackOriginLocation(), GetAttackTargetLocation(*CurrentTarget));
    if (DistanceSquared <= FMath::Square(AttackDistance))
    {
        StopMove();
        if (Profile == ECMAggressiveBehaviorProfile::Ripper)
        {
            const double CurrentTime = GetWorld()->GetTimeSeconds();
            if (CMAggressiveBehaviorRules::IsRipperAttackReady(CurrentTime, NextRipperAttackTime))
            {
                if (PerformRipperAttack(*CurrentTarget))
                {
                    NextRipperAttackTime = CurrentTime + CMAggressiveBehavior::RipperAttackCooldownSeconds;
                }
                else
                {
                    BeginReturningHome();
                }
            }
            return;
        }
        State = ECMAggressiveAIState::Attacking;
        if (Profile == ECMAggressiveBehaviorProfile::Tetra)
        {
            BeginTetraDash();
        }
        else
        {
            const bool bTargetKilled = PerformCentipedeAttack(*CurrentTarget);
            if (bTargetKilled || !IsValidTarget(CurrentTarget))
            {
                BeginReturningHome();
            }
            else
            {
                BeginChasing(CurrentTarget);
            }
        }
        return;
    }

    if (!IsMoveRunning())
    {
        const double CurrentTime = GetWorld()->GetTimeSeconds();
        if (bMoveIssued)
        {
            bMoveIssued = false;
        }
        if (CurrentTime < NextActionTime)
        {
            return;
        }
        if (Profile == ECMAggressiveBehaviorProfile::Ripper && !CanPursueTarget(*CurrentTarget))
        {
            BeginReturningHome();
            return;
        }
        bMoveIssued = StartMove(TargetLocation, AttackDistance);
        if (!bMoveIssued)
        {
            NextActionTime = CurrentTime + CMAggressiveBehavior::FailedMoveRetryInterval;
        }
        else
        {
            NextActionTime = 0.0;
        }
    }
    else if (FVector::DistSquared2D(TargetLocation, LastMoveGoal) >= FMath::Square(CMAggressiveBehavior::RepathDistance))
    {
        if (Profile == ECMAggressiveBehaviorProfile::Ripper && !CanPursueTarget(*CurrentTarget))
        {
            BeginReturningHome();
            return;
        }
        if (!UpdateMoveGoal(TargetLocation))
        {
            bMoveIssued = false;
            NextActionTime = 0.0;
        }
    }
}

// Tetra 돌진의 제한시간과 타깃 유효성을 검사하며 물리 속도를 유지한다.
void UCMAggressiveBehaviorComponent::UpdateAttacking(const double CurrentTime)
{
    if (Profile != ECMAggressiveBehaviorProfile::Tetra)
    {
        return;
    }
    if (CurrentTime >= DashEndTime || !IsValidTarget(CurrentTarget))
    {
        FinishTetraDashWithoutHit();

        return;
    }

    UPrimitiveComponent* Body = GetMovementBody();
    if (!Body)
    {
        FinishTetraDashWithoutHit();

        return;
    }
    const float BaseSpeed = CastChecked<ACMTetraPawn>(OwnerPawn)->GetAccelerationMovement()->GetMaximumSpeed();
    FVector Velocity = Body->GetPhysicsLinearVelocity();
    Velocity.X = DashDirection.X * BaseSpeed * CMAggressiveBehavior::TetraDashSpeedMultiplier;
    Velocity.Y = DashDirection.Y * BaseSpeed * CMAggressiveBehavior::TetraDashSpeedMultiplier;
    Body->SetPhysicsLinearVelocity(Velocity);
}

void UCMAggressiveBehaviorComponent::UpdateWaiting(const double CurrentTime)
{
    if (CurrentTime < NextActionTime)
    {
        return;
    }
    if (AActor* SeenTarget = FindVisibleTarget())
    {
        BeginChasing(SeenTarget);
    }
    else
    {
        BeginReturningHome();
    }
}

// 비추격 상태의 Tetra 시야 방향을 일정 간격으로 회전시켜 주변을 탐색한다.
void UCMAggressiveBehaviorComponent::UpdateTetraSightScan(const double CurrentTime)
{
    if (Profile != ECMAggressiveBehaviorProfile::Tetra)
    {
        return;
    }

    const bool bTrackingPlayer = (State == ECMAggressiveAIState::Chasing || State == ECMAggressiveAIState::Attacking) && IsValidTarget(CurrentTarget);
    if (bTrackingPlayer)
    {
        NextTetraSightTurnTime = 0.0;
        return;
    }
    if (NextTetraSightTurnTime <= 0.0)
    {
        NextTetraSightTurnTime = CurrentTime + FMath::FRandRange(CMAggressiveBehavior::TetraSightTurnMinInterval, CMAggressiveBehavior::TetraSightTurnMaxInterval);

        return;
    }
    if (CurrentTime < NextTetraSightTurnTime)
    {
        return;
    }

    if (!SightComponents.IsEmpty())
    {
        SightComponents[0]->AddRelativeRotation(FRotator(0.0f, CMAggressiveBehavior::TetraSightTurnDegrees, 0.0f));
        OwnerPawn->ForceNetUpdate();
    }
    NextTetraSightTurnTime = CurrentTime + FMath::FRandRange(CMAggressiveBehavior::TetraSightTurnMinInterval, CMAggressiveBehavior::TetraSightTurnMaxInterval);
}

// 유효한 타깃을 저장하고 공격 거리까지의 정책 경로 추격을 시작한다.
void UCMAggressiveBehaviorComponent::BeginChasing(AActor* NewTarget)
{
    if (!IsValidTarget(NewTarget))
    {
        return;
    }
    if (Profile == ECMAggressiveBehaviorProfile::Ripper && !CanPursueTarget(*NewTarget))
    {
        if (State == ECMAggressiveAIState::Chasing)
        {
            BeginReturningHome();
        }
        return;
    }
    if (Profile == ECMAggressiveBehaviorProfile::Centipede && IsValidTarget(CurrentTarget) && CurrentTarget != NewTarget)
    {
        return;
    }
    const bool bNewlySpottedTarget =
        State == ECMAggressiveAIState::Searching
        && !IsValidTarget(CurrentTarget);
    CurrentTarget = NewTarget;
    State = ECMAggressiveAIState::Chasing;
    bReturningHome = false;
    NextActionTime = 0.0;
    bMoveIssued = StartMove(CurrentTarget->GetActorLocation(), GetAttackDistance());
    if (!bMoveIssued)
    {
        NextActionTime = GetWorld()->GetTimeSeconds() + CMAggressiveBehavior::FailedMoveRetryInterval;
    }
    if (bNewlySpottedTarget)
    {
        MulticastPlayTargetSpottedSound();
    }
}

void UCMAggressiveBehaviorComponent::MulticastPlayTargetSpottedSound_Implementation()
{
    FCMSoundPlayback::PlaySFXAtActor(
        OwnerPawn,
        CMSoundTags::AI_Aggressive_TargetSpotted);
}

// 현재 교전을 해제하고 탐색 상태에서 생성 위치로 돌아가도록 전환한다.
void UCMAggressiveBehaviorComponent::BeginReturningHome()
{
    StopMove();
    CurrentTarget = nullptr;
    State = ECMAggressiveAIState::Searching;
    bReturningHome = true;
    bMoveIssued = false;
    NextActionTime = 0.0;
}

void UCMAggressiveBehaviorComponent::BeginWaiting(const float Seconds)
{
    StopMove();
    State = ECMAggressiveAIState::Waiting;
    NextActionTime = GetWorld()->GetTimeSeconds() + FMath::Max(Seconds, 0.0f);
}

// 생성 위치 주변에서 충분히 떨어진 도달 가능 지점을 골라 배회 이동을 시작한다.
bool UCMAggressiveBehaviorComponent::BeginRandomMove()
{
    const UCMAggressiveOmnidirectionalPathComponent* PathMovement = OwnerPawn ? OwnerPawn->FindComponentByClass<UCMAggressiveOmnidirectionalPathComponent>() : nullptr;
    if (!PathMovement)
    {
        return false;
    }

    const float Radius = Profile == ECMAggressiveBehaviorProfile::Centipede ? CMAggressiveBehavior::CentipedeWanderRadius : Profile == ECMAggressiveBehaviorProfile::Ripper ? CMAggressiveBehavior::RipperWanderRadius : CMAggressiveBehavior::TetraWanderRadius;

    const FVector CurrentLocation = GetNavigationLocation();
    const float MinimumGoalDistance = CMAggressiveBehavior::MinimumWanderTravelDistance + CMAggressiveBehavior::WanderAcceptanceRadius;
    FVector RandomLocation = FVector::ZeroVector;
    bool bFoundGoal = false;
    for (int32 Attempt = 0; Attempt < CMAggressiveBehavior::WanderGoalSampleAttempts; ++Attempt)
    {
        if (PathMovement->FindRandomReachableLocation(SpawnLocation, Radius, RandomLocation) && FVector::DistSquared2D(CurrentLocation, RandomLocation) >= FMath::Square(MinimumGoalDistance))
        {
            bFoundGoal = true;
            break;
        }
    }
    if (!bFoundGoal)
    {
        return false;
    }
    LastWanderGoal = RandomLocation;
    bHasWanderGoal = true;
    OwnerPawn->ForceNetUpdate();

    return StartMove(RandomLocation, CMAggressiveBehavior::WanderAcceptanceRadius);
}

void UCMAggressiveBehaviorComponent::DrawWanderGoalDebug() const
{
#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
    UWorld* World = GetWorld();
    if (!CMAggressiveBehavior::bDrawWanderGoalDebug || !bHasWanderGoal || !World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    constexpr float DebugLifetime = CMAggressiveBehavior::UpdateInterval * 1.5f;
    const FVector GoalMarker = LastWanderGoal + FVector::UpVector * 75.0f;
    DrawDebugSphere(World, GoalMarker, 35.0f, 16, FColor::Green, false, DebugLifetime, 0, 4.0f);
    DrawDebugLine(World, GetNavigationLocation() + FVector::UpVector * 75.0f, GoalMarker, FColor::Green, false, DebugLifetime, 0, 3.0f);
    DrawDebugCrosshairs(World, GoalMarker, FRotator::ZeroRotator, 50.0f, FColor::Yellow, false, DebugLifetime, 0);
#endif
}

// AI 종류별 정책 추론 조정자에 동일한 월드 이동 목표를 전달한다.
bool UCMAggressiveBehaviorComponent::StartMove(const FVector Goal, const float AcceptanceRadius)
{
    StopMove();
    LastMoveGoal = Goal;
    switch (Profile)
    {
    case ECMAggressiveBehaviorProfile::Tetra:
        return TetraCoordinator && TetraCoordinator->StartInferencePath(Cast<ACMTetraPawn>(OwnerPawn), Goal, AcceptanceRadius);
    case ECMAggressiveBehaviorProfile::Ripper:
        return RipperCoordinator && RipperCoordinator->StartInferencePath(Cast<ACMRipperPawn>(OwnerPawn), Goal, AcceptanceRadius);
    case ECMAggressiveBehaviorProfile::Centipede:
        return CentipedeCoordinator && CentipedeCoordinator->StartInferencePath(Cast<ACMCentipedePawn>(OwnerPawn), Goal, AcceptanceRadius);
    default:
        return false;
    }
}

bool UCMAggressiveBehaviorComponent::UpdateMoveGoal(const FVector Goal)
{
    LastMoveGoal = Goal;
    switch (Profile)
    {
    case ECMAggressiveBehaviorProfile::Tetra:
        return TetraCoordinator && TetraCoordinator->UpdateInferenceGoal(Goal);
    case ECMAggressiveBehaviorProfile::Ripper:
        return RipperCoordinator && RipperCoordinator->UpdateInferenceGoal(Goal);
    case ECMAggressiveBehaviorProfile::Centipede:
        return CentipedeCoordinator && CentipedeCoordinator->UpdateInferenceGoal(Goal);
    default:
        return false;
    }
}

void UCMAggressiveBehaviorComponent::StopMove()
{
    if (TetraCoordinator && TetraCoordinator->IsInferencePathRunning())
    {
        TetraCoordinator->StopInferencePath();
    }
    if (RipperCoordinator && RipperCoordinator->IsInferenceRunning())
    {
        RipperCoordinator->StopInference();
    }
    if (CentipedeCoordinator && CentipedeCoordinator->IsInferenceRunning())
    {
        CentipedeCoordinator->StopInference();
    }
    bMoveIssued = false;
}

bool UCMAggressiveBehaviorComponent::IsMoveRunning() const
{
    return (TetraCoordinator && TetraCoordinator->IsInferencePathRunning()) || (RipperCoordinator && RipperCoordinator->IsInferenceRunning()) || (CentipedeCoordinator && CentipedeCoordinator->IsInferenceRunning());
}

float UCMAggressiveBehaviorComponent::GetAttackDistance() const
{
    switch (Profile)
    {
    case ECMAggressiveBehaviorProfile::Ripper:
        return FMath::Max(RipperAttackDistance, 0.0f);
    case ECMAggressiveBehaviorProfile::Centipede:
        return FMath::Max(CentipedeAttackDistance, 0.0f);
    default:
        return CMAggressiveBehavior::TetraDashStartDistance;
    }
}

ICMAggressiveMovementAgent* UCMAggressiveBehaviorComponent::GetMovementAgent() const
{
    return CachedMovementAgent;
}

FVector UCMAggressiveBehaviorComponent::GetNavigationLocation() const
{
    const ICMAggressiveMovementAgent* MovementAgent = GetMovementAgent();

    return MovementAgent ? MovementAgent->GetAggressiveNavigationReferenceLocation() : OwnerPawn ? OwnerPawn->GetActorLocation() : FVector::ZeroVector;
}

FVector UCMAggressiveBehaviorComponent::GetAttackOriginLocation() const
{
    const ACMCentipedePawn* Centipede = Cast<ACMCentipedePawn>(OwnerPawn);

    return Centipede ? Centipede->GetLeadingTipLocation() : GetNavigationLocation();
}

// Centipede는 플레이어 Actor 원점 대신 가장 가까운 생존 몸통 마디를 공격 거리 기준으로 사용한다.
FVector UCMAggressiveBehaviorComponent::GetAttackTargetLocation(const AActor& Target) const
{
    const ACMChimera* Chimera = Profile == ECMAggressiveBehaviorProfile::Centipede ? Cast<ACMChimera>(&Target) : nullptr;
    if (!Chimera)
    {
        return Target.GetActorLocation();
    }

    const FVector AttackOrigin = GetAttackOriginLocation();
    FVector ClosestLocation = Target.GetActorLocation();
    float ClosestDistanceSquared = TNumericLimits<float>::Max();
    for (int32 SegmentIndex = 0; SegmentIndex < Chimera->GetActiveSegmentCount(); ++SegmentIndex)
    {
        const UBoxComponent* Segment = Chimera->IsSegmentAlive(SegmentIndex) ? Chimera->GetBodySegmentComponent(SegmentIndex) : nullptr;
        if (!Segment)
        {
            continue;
        }

        const FVector SegmentLocation = Segment->GetComponentLocation();
        const float DistanceSquared = FVector::DistSquared2D(AttackOrigin, SegmentLocation);
        if (DistanceSquared < ClosestDistanceSquared)
        {
            ClosestLocation = SegmentLocation;
            ClosestDistanceSquared = DistanceSquared;
        }
    }
    return ClosestLocation;
}

UPrimitiveComponent* UCMAggressiveBehaviorComponent::GetMovementBody() const
{
    ICMAggressiveMovementAgent* MovementAgent = GetMovementAgent();

    return MovementAgent ? MovementAgent->GetAggressiveMovementBody() : nullptr;
}

// 플레이어와 공격 가능한 희생자 중 시야에 보이는 가장 가까운 타깃을 선택한다.
AActor* UCMAggressiveBehaviorComponent::FindVisibleTarget() const
{
    if (!OwnerPawn)
    {
        return nullptr;
    }

    AActor* BestTarget = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    const UWorld* World = GetWorld();
    const ACMGameState* GameState = World ? World->GetGameState<ACMGameState>() : nullptr;

    ACMChimera* Chimera = GameState ? GameState->SharedChimera : nullptr;
    if (IsValidTarget(Chimera) && CanSeeActor(*Chimera) && CanPursueTarget(*Chimera))
    {
        BestTarget = Chimera;
        BestDistanceSquared = FVector::DistSquared2D(GetNavigationLocation(), Chimera->GetActorLocation());
    }

    if (Profile == ECMAggressiveBehaviorProfile::Tetra || !World)
    {
        return BestTarget;
    }

    for (TActorIterator<ACMSacrificeCharacter> It(World); It; ++It)
    {
        ACMSacrificeCharacter* Sacrifice = *It;
        if (!IsValidTarget(Sacrifice) || !CanSeeActor(*Sacrifice) || !CanPursueTarget(*Sacrifice))
        {
            continue;
        }
        const float DistanceSquared = FVector::DistSquared2D(GetNavigationLocation(), Sacrifice->GetActorLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestTarget = Sacrifice;
            BestDistanceSquared = DistanceSquared;
        }
    }
    return BestTarget;
}

ACMSacrificeCharacter* UCMAggressiveBehaviorComponent::FindCloserVisibleSacrifice(const ACMSacrificeCharacter& CurrentSacrifice) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    const FVector NavigationLocation = GetNavigationLocation();
    float BestDistanceSquared = FVector::DistSquared2D(NavigationLocation, CurrentSacrifice.GetActorLocation());
    ACMSacrificeCharacter* BestSacrifice = nullptr;

    for (TActorIterator<ACMSacrificeCharacter> It(World); It; ++It)
    {
        ACMSacrificeCharacter* Candidate = *It;
        if (Candidate == &CurrentSacrifice || !IsValidTarget(Candidate) || !CanSeeActor(*Candidate)
            || !CanPursueTarget(*Candidate))
        {
            continue;
        }
        const float DistanceSquared = FVector::DistSquared2D(NavigationLocation, Candidate->GetActorLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestSacrifice = Candidate;
            BestDistanceSquared = DistanceSquared;
        }
    }
    return BestSacrifice;
}

bool UCMAggressiveBehaviorComponent::CanPursueTarget(const AActor& Target) const
{
    if (Profile != ECMAggressiveBehaviorProfile::Ripper)
    {
        return true;
    }
    const UCMAggressiveOmnidirectionalPathComponent* PathMovement = OwnerPawn ? OwnerPawn->FindComponentByClass<UCMAggressiveOmnidirectionalPathComponent>() : nullptr;
    return PathMovement && PathMovement->IsNavigationGoalReachable(Target.GetActorLocation(), GetAttackDistance());
}

bool UCMAggressiveBehaviorComponent::CanSeeActor(const AActor& Target) const
{
    for (const UCMAggressiveSightComponent* Sight : SightComponents)
    {
        if (Sight && Sight->CanSeeActor(&Target))
        {
            return true;
        }
    }
    return false;
}

bool UCMAggressiveBehaviorComponent::IsValidTarget(const AActor* Target) const
{
    if (!IsValid(Target))
    {
        return false;
    }
    if (const ACMChimera* Chimera = Cast<ACMChimera>(Target))
    {
        return !Chimera->AreAllSegmentsDead();
    }
    if (const ACMSacrificeCharacter* Sacrifice = Cast<ACMSacrificeCharacter>(Target))
    {
        const UCMSacrificeStateComponent* StateComponent = Sacrifice->GetSacrificeStateComponent();

        return StateComponent && StateComponent->IsAlive();
    }
    return false;
}

// Ripper가 희생자의 신체나 플레이어의 가장 가까운 부착 파츠를 절단해 소비한다.
bool UCMAggressiveBehaviorComponent::PerformRipperAttack(AActor& Target)
{
    if (ACMSacrificeCharacter* Sacrifice = Cast<ACMSacrificeCharacter>(&Target))
    {
        FCMDismembermentHitRequest Request;
        Request.Attacker = OwnerPawn;
        Request.SourcePart = OwnerPawn;
        Request.AttackId = FGuid::NewGuid();
        Request.bConsumeSeveredPart = true;
        Request.ImpactPoint = Sacrifice->GetActorLocation();
        Request.ImpactDirection = (Sacrifice->GetActorLocation() - OwnerPawn->GetActorLocation()).GetSafeNormal();

        return ICMDismemberableTarget::Execute_ReceiveDismembermentHit(Sacrifice, Request) > 0;
    }

    ACMChimera* Chimera = Cast<ACMChimera>(&Target);
    if (!Chimera)
    {
        return false;
    }

    UCMPartSlotComponent* ClosestSlot = nullptr;
    float ClosestDistanceSquared = TNumericLimits<float>::Max();

    for (int32 SegmentIndex = 0; SegmentIndex < Chimera->GetActiveSegmentCount(); ++SegmentIndex)
    {
        for (int32 PartSlotIndex = 0; PartSlotIndex < CMControl::PartSlotsPerSegment; ++PartSlotIndex)
        {
            FCMPartSlotAddress Address;
            Address.SegmentIndex = SegmentIndex;
            Address.PartSlotIndex = PartSlotIndex;
            UCMPartSlotComponent* Slot = Chimera->GetPartSlotComponent(Address);
            if (!Slot || !Slot->HasAttachedPart())
            {
                continue;
            }

            const float DistanceSquared = FVector::DistSquared(OwnerPawn->GetActorLocation(), Slot->GetComponentLocation());

            if (DistanceSquared < ClosestDistanceSquared)
            {
                ClosestSlot = Slot;
                ClosestDistanceSquared = DistanceSquared;
            }
        }
    }
    if (!ClosestSlot)
    {
        return false;
    }

    AActor* ConsumedPart = Chimera->DetachPartFromSlot(ClosestSlot->GetSlotAddress());
    if (ConsumedPart)
    {
        ConsumedPart->Destroy();

        return true;
    }
    return false;
}

// Centipede가 희생자를 즉사 절단하거나 플레이어의 생존 세그먼트를 전부 파괴한다.
bool UCMAggressiveBehaviorComponent::PerformCentipedeAttack(AActor& Target)
{
    const FVector AttackOrigin = GetAttackOriginLocation();

    if (ACMSacrificeCharacter* Sacrifice = Cast<ACMSacrificeCharacter>(&Target))
    {
        FCMDismembermentHitRequest Request;
        Request.Attacker = OwnerPawn;
        Request.SourcePart = OwnerPawn;
        Request.AttackId = FGuid::NewGuid();
        Request.ImpactPoint = Sacrifice->GetActorLocation();
        Request.ImpactDirection = (Sacrifice->GetActorLocation() - AttackOrigin).GetSafeNormal();
        UCMSacrificeStateComponent* StateComponent = Sacrifice->GetSacrificeStateComponent();

        return StateComponent && StateComponent->ResolveFatalDismembermentHit(Request) > 0;
    }

    ACMChimera* Chimera = Cast<ACMChimera>(&Target);
    if (!Chimera)
    {
        return false;
    }

    const TArray<FCMBodySegmentHealthState> HealthStates = Chimera->GetSegmentHealthStates();
    for (const FCMBodySegmentHealthState& Segment : HealthStates)
    {
        if (!Segment.bDead)
        {
            Chimera->ApplyDamageToSegment(Segment.SegmentIndex, FMath::Max(Segment.Health, Segment.MaxHealth));
        }
    }
    return Chimera->AreAllSegmentsDead();
}

// 현재 타깃을 향한 돌진 방향과 제한시간을 고정하고 최고속도를 높인다.
void UCMAggressiveBehaviorComponent::BeginTetraDash()
{
    if (!IsValidTarget(CurrentTarget))
    {
        BeginReturningHome();

        return;
    }

    DashDirection = (CurrentTarget->GetActorLocation() - GetNavigationLocation()).GetSafeNormal2D(SMALL_NUMBER, OwnerPawn->GetActorForwardVector());
    CastChecked<ACMTetraPawn>(OwnerPawn)->GetAccelerationMovement()->SetMaximumSpeedMultiplier(CMAggressiveBehavior::TetraDashSpeedMultiplier);
    DashEndTime = GetWorld()->GetTimeSeconds() + CMAggressiveBehavior::TetraDashDuration;
}

void UCMAggressiveBehaviorComponent::FinishTetraDashWithoutHit()
{
    CastChecked<ACMTetraPawn>(OwnerPawn)->GetAccelerationMovement()->SetMaximumSpeedMultiplier(1.0f);
    if (UPrimitiveComponent* Body = GetMovementBody())
    {
        const float VerticalSpeed = Body->GetPhysicsLinearVelocity().Z;
        Body->SetPhysicsLinearVelocity(FVector::UpVector * VerticalSpeed);
    }

    if (IsValidTarget(CurrentTarget))
    {
        BeginChasing(CurrentTarget);
    }

    else
    {
        BeginReturningHome();
    }
}

void UCMAggressiveBehaviorComponent::HandleTetraKnockbackFinished()
{
    if (!OwnerPawn)
    {
        return;
    }

    CastChecked<ACMTetraPawn>(OwnerPawn)->GetAccelerationMovement()->SetMaximumSpeedMultiplier(1.0f);
    OwnerPawn->SetKnockbackState(false);
    BeginWaiting(10.0f);
}

// Tetra 돌진이 플레이어와 충돌하면 양쪽 넉백을 적용하고 대기 상태로 전환한다.
void UCMAggressiveBehaviorComponent::HandleOwnerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
    if (Profile != ECMAggressiveBehaviorProfile::Tetra || State != ECMAggressiveAIState::Attacking || OtherActor != CurrentTarget)
    {
        return;
    }

    ACMChimera* Chimera = Cast<ACMChimera>(CurrentTarget);
    if (!Chimera)
    {
        return;
    }
    Chimera->StartPlanarKnockback(DashDirection, CMAggressiveBehavior::TetraPlayerKnockbackDistance);

    // 자체 넉백 완료 콜백만 다음 대기 시간을 시작하도록 일반 타이머 전환을 막는다.
    State = ECMAggressiveAIState::Waiting;
    NextActionTime = TNumericLimits<double>::Max();

    UCMAggressiveKnockbackComponent* Knockback = OwnerPawn->GetKnockbackComponent();

    if (!Knockback)
    {
        CastChecked<ACMTetraPawn>(OwnerPawn)->GetAccelerationMovement()->SetMaximumSpeedMultiplier(1.0f);
        BeginWaiting(10.0f);

        return;
    }
    OwnerPawn->SetKnockbackState(true);
    Knockback->OnKnockbackFinished.RemoveAll(this);
    Knockback->OnKnockbackFinished.AddUObject(this, &ThisClass::HandleTetraKnockbackFinished);
    if (!Knockback->StartKnockback(-DashDirection, CMAggressiveBehavior::TetraSelfKnockbackDistance))
    {
        HandleTetraKnockbackFinished();
    }
}

void UCMAggressiveBehaviorComponent::DestroyInferenceCoordinator()
{
    if (TetraCoordinator)
    {
        TetraCoordinator->Destroy();
        TetraCoordinator = nullptr;
    }
    if (RipperCoordinator)
    {
        RipperCoordinator->Destroy();
        RipperCoordinator = nullptr;
    }
    if (CentipedeCoordinator)
    {
        CentipedeCoordinator->Destroy();
        CentipedeCoordinator = nullptr;
    }
}
