#include "Sacrifice/CMSacrificeAIController.h"

#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"
#include "Common/CMAINavigationRules.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "Sacrifice/CMSacrificeActionAbilities.h"
#include "Sacrifice/CMSacrificeCharacter.h"
#include "Sacrifice/CMSacrificeRules.h"
#include "Sacrifice/CMSacrificeStateComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

namespace CMSacrificePursuitNavigation
{
    constexpr float BoundaryClearanceCm = 50.0f;
    constexpr float ProjectionToleranceCm = 25.0f;
    constexpr float PathSampleSpacingCm = 50.0f;
    const FName RipperAgentName(TEXT("RipperAI"));
    const FName CentipedeAgentName(TEXT("CentipedeAI"));

    const ANavigationData* FindNavigationData(const UNavigationSystemV1& NavigationSystem, const FName AgentName)
    {
        for (const FNavDataConfig& AgentConfig : NavigationSystem.GetSupportedAgents())
        {
            if (AgentConfig.Name == AgentName)
                return NavigationSystem.GetNavDataForProps(AgentConfig);
        }
        return nullptr;
    }

    bool HasBoundaryClearance(const ANavigationData& NavigationData, const FVector Location, const UObject* Querier)
    {
        constexpr int32 DirectionCount = 16;
        for (int32 DirectionIndex = 0; DirectionIndex < DirectionCount; ++DirectionIndex)
        {
            const float Angle = UE_TWO_PI * static_cast<float>(DirectionIndex) / static_cast<float>(DirectionCount);
            const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
            FVector HitLocation;
            if (!NavigationData.Raycast(Location, Location + Direction * BoundaryClearanceCm, HitLocation, NavigationData.GetDefaultQueryFilter(), Querier))
                continue;
            if (FVector::Dist2D(Location, HitLocation) + 1.0f < BoundaryClearanceCm)
                return false;
        }
        return true;
    }

    bool IsPathSupportedByAgent(UNavigationSystemV1& NavigationSystem, const TArray<FNavPathPoint>& SourcePathPoints, const FName AgentName, const UObject* Querier)
    {
        const ANavigationData* NavigationData = FindNavigationData(NavigationSystem, AgentName);
        if (!NavigationData || SourcePathPoints.Num() < 2)
            return false;

        FNavLocation ProjectedStart;
        const FVector StartProjectionExtent(200.0f, 200.0f, 300.0f);
        if (!NavigationSystem.ProjectPointToNavigation(SourcePathPoints[0].Location, ProjectedStart, StartProjectionExtent, NavigationData))
            return false;

        FVector PreviousProjectedLocation = ProjectedStart.Location;
        const FVector SampleProjectionExtent(ProjectionToleranceCm, ProjectionToleranceCm, 300.0f);
        for (int32 PathPointIndex = 1; PathPointIndex < SourcePathPoints.Num(); ++PathPointIndex)
        {
            const FVector SegmentStart = SourcePathPoints[PathPointIndex - 1].Location;
            const FVector SegmentEnd = SourcePathPoints[PathPointIndex].Location;
            const int32 SampleCount = FMath::Max(FMath::CeilToInt(FVector::Dist2D(SegmentStart, SegmentEnd) / PathSampleSpacingCm), 1);
            for (int32 SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
            {
                const FVector SampleLocation = FMath::Lerp(SegmentStart, SegmentEnd, static_cast<float>(SampleIndex) / static_cast<float>(SampleCount));
                FNavLocation ProjectedSample;
                if (!NavigationSystem.ProjectPointToNavigation(SampleLocation, ProjectedSample, SampleProjectionExtent, NavigationData))
                    return false;
                if (!FCMAINavigationRules::IsWithinProjectionTolerance(SampleLocation, ProjectedSample.Location, ProjectionToleranceCm))
                    return false;
                if (!HasBoundaryClearance(*NavigationData, ProjectedSample.Location, Querier))
                    return false;

                FVector HitLocation;
                if (NavigationData->Raycast(PreviousProjectedLocation, ProjectedSample.Location, HitLocation, NavigationData->GetDefaultQueryFilter(), Querier))
                    return false;
                PreviousProjectedLocation = ProjectedSample.Location;
            }
        }
        return true;
    }
} // namespace CMSacrificePursuitNavigation

ACMSacrificeAIController::ACMSacrificeAIController()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
    bSetControlRotationFromPawnOrientation = false;
}

// Sacrifice를 소유하면 위협 추적과 상태 이벤트를 연결하고 평상 행동을 시작한다.
void ACMSacrificeAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    Sacrifice = Cast<ACMSacrificeCharacter>(InPawn);
    ThreatTracker.Initialize(Sacrifice);
    if (!Sacrifice || !HasAuthority())
    {
        return;
    }

    Sacrifice->bUseControllerRotationYaw = false;
    Sacrifice->GetCharacterMovement()->bOrientRotationToMovement = true;
    Sacrifice->GetCharacterMovement()->bUseControllerDesiredRotation = false;
    LastValidNavigationLocation = FVector::ZeroVector;
    bHasLastValidNavigationLocation = false;
    Sacrifice->OnSacrificeHitAccepted.AddDynamic(this, &ThisClass::HandleAcceptedHit);
    Sacrifice->GetSacrificeStateComponent()->OnSacrificeDied.AddDynamic(this, &ThisClass::HandleSacrificeDied);
    Phase = ECMSacrificeBehaviorPhase::Ambient;
    ScheduleAmbientAction();
}

void ACMSacrificeAIController::OnUnPossess()
{
    StopBehaviorTimers();
    if (Sacrifice)
    {
        Sacrifice->OnSacrificeHitAccepted.RemoveDynamic(this, &ThisClass::HandleAcceptedHit);
        Sacrifice->GetSacrificeStateComponent()->OnSacrificeDied.RemoveDynamic(this, &ThisClass::HandleSacrificeDied);
    }
    ThreatTracker.Reset();
    LastValidNavigationLocation = FVector::ZeroVector;
    bHasLastValidNavigationLocation = false;
    Sacrifice = nullptr;
    Super::OnUnPossess();
}

// 서버에서 이동 방향을 보정하고 행동 가능한 상태일 때 주기적으로 위협을 탐색한다.
void ACMSacrificeAIController::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Sacrifice || !HasAuthority())
    {
        return;
    }
    UpdateForwardMovementFacing();
    ThreatTracker.DrawDebug();
    if (CannotAct())
    {
        return;
    }
    if (Phase == ECMSacrificeBehaviorPhase::HitReact || Phase == ECMSacrificeBehaviorPhase::GettingUp)
    {
        StationaryMovementSeconds = 0.0f;
        return;
    }

    if (RecoverToNavigation())
    {
        return;
    }

    UpdateMovementProgress(DeltaSeconds);

    ThreatScanAccumulator += DeltaSeconds;
    if (ThreatScanAccumulator < 0.2f)
    {
        return;
    }
    ThreatScanAccumulator = 0.0f;
    ScanForThreats();
}

// 이동 중 NavMesh 경계 밖으로 밀리면 가장 가까운 유효 위치로 복귀하고 행동을 재시도한다.
bool ACMSacrificeAIController::RecoverToNavigation()
{
    const ECMSacrificeActionState ActionState = Sacrifice->GetSacrificeActionState();
    if (!FCMSacrificeRules::IsMovementActionState(ActionState))
    {
        return false;
    }

    UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    ANavigationData* NavigationData = NavigationSystem ? NavigationSystem->GetNavDataForProps(Sacrifice->GetNavAgentPropertiesRef()) : nullptr;
    if (!NavigationSystem || !NavigationData)
    {
        return false;
    }

    const FVector CurrentLocation = Sacrifice->GetActorLocation();
    FNavLocation ProjectedLocation;
    const FVector ContainmentExtent(NavigationContainmentToleranceCm, NavigationContainmentToleranceCm, 300.0f);
    if (NavigationSystem->ProjectPointToNavigation(CurrentLocation, ProjectedLocation, ContainmentExtent, NavigationData)
        && FCMAINavigationRules::IsWithinProjectionTolerance(CurrentLocation, ProjectedLocation.Location, NavigationContainmentToleranceCm))
    {
        LastValidNavigationLocation = CurrentLocation;
        bHasLastValidNavigationLocation = true;
        return false;
    }

    FVector RecoveryLocation;
    if (bHasLastValidNavigationLocation)
    {
        RecoveryLocation = FVector(LastValidNavigationLocation.X, LastValidNavigationLocation.Y, CurrentLocation.Z);
    }
    else if (NavigationSystem->ProjectPointToNavigation(CurrentLocation, ProjectedLocation, FVector(1000.0f, 1000.0f, 500.0f), NavigationData))
    {
        RecoveryLocation = FVector(ProjectedLocation.Location.X, ProjectedLocation.Location.Y, CurrentLocation.Z);
    }
    else
    {
        return false;
    }

    Sacrifice->SetActorLocation(RecoveryLocation, false, nullptr, ETeleportType::TeleportPhysics);
    Sacrifice->GetCharacterMovement()->StopMovementImmediately();
    RecoverStalledMovement();
    return true;
}

// 후진 기기는 이동 반대를, 일반 도주는 이동 방향을 바라보도록 회전을 제어한다.
void ACMSacrificeAIController::UpdateForwardMovementFacing()
{
    const ECMSacrificeActionState ActionState = Sacrifice->GetSacrificeActionState();

    const bool bBackCrawl = ActionState == ECMSacrificeActionState::BackCrawl;

    const bool bFacesMovement = ActionState == ECMSacrificeActionState::Wander || ActionState == ECMSacrificeActionState::Flee || ActionState == ECMSacrificeActionState::ExhaustedWalk || ActionState == ECMSacrificeActionState::InjuredCrawl;

    if (!bFacesMovement && !bBackCrawl)
    {
        return;
    }

    const FVector MoveDirection = Sacrifice->GetVelocity().GetSafeNormal2D();

    if (MoveDirection.IsNearlyZero())
    {
        return;
    }

    const FVector FacingDirection = bBackCrawl ? -MoveDirection : MoveDirection;

    Sacrifice->GetCharacterMovement()->bOrientRotationToMovement = false;
    Sacrifice->GetCharacterMovement()->bUseControllerDesiredRotation = true;

    const float Yaw = FacingDirection.Rotation().Yaw;
    SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
}

// 실제 이동이 멈춘 이동 행동을 빠르게 종료하고 현재 행동에 맞는 경로 재요청을 예약한다.
void ACMSacrificeAIController::UpdateMovementProgress(const float DeltaSeconds)
{
    const ECMSacrificeActionState ActionState = Sacrifice->GetSacrificeActionState();
    if (!FCMSacrificeRules::IsMovementActionState(ActionState))
    {
        StationaryMovementSeconds = 0.0f;
        return;
    }

    const bool bPathIdle = GetMoveStatus() == EPathFollowingStatus::Idle;
    if (!bPathIdle && Sacrifice->GetVelocity().SizeSquared2D() >= FMath::Square(MinimumMovingSpeedCmPerSecond))
    {
        StationaryMovementSeconds = 0.0f;
        return;
    }

    StationaryMovementSeconds += DeltaSeconds;
    if (StationaryMovementSeconds < MovementStallTimeoutSeconds)
    {
        return;
    }

    StationaryMovementSeconds = 0.0f;
    RecoverStalledMovement();
}

void ACMSacrificeAIController::RecoverStalledMovement()
{
    const ECMSacrificeActionState ActionState = Sacrifice->GetSacrificeActionState();
    StopMovementForTransition();
    if (Phase == ECMSacrificeBehaviorPhase::Ambient)
    {
        Sacrifice->CancelSacrificeActions();
        return;
    }
    if (Phase == ECMSacrificeBehaviorPhase::BackCrawl)
    {
        if (!bThreatWasVisible || bFinishGoalAfterThreatLost || !Sacrifice->GetCurrentThreat())
        {
            StartSafetyRecovery();
            return;
        }
        GetWorldTimerManager().SetTimer(MoveRetryTimer, this, &ThisClass::RetryBackCrawlMovement, 0.5f, false);
        return;
    }
    Sacrifice->CancelSacrificeActions();
    if (Phase == ECMSacrificeBehaviorPhase::Escape && ActionState == ECMSacrificeActionState::InjuredCrawl)
    {
        GetWorldTimerManager().SetTimer(MoveRetryTimer, this, &ThisClass::StartInjuredCrawl, 0.5f, false);
        return;
    }
    if (Phase == ECMSacrificeBehaviorPhase::Escape)
    {
        GetWorldTimerManager().SetTimer(MoveRetryTimer, this, &ThisClass::StartEscapeLeg, 0.5f, false);
    }
}

// 시야에 잡힌 가장 가까운 위협을 선택하고 기억을 갱신해 도주 또는 안전 복구를 결정한다.
void ACMSacrificeAIController::ScanForThreats()
{
    TArray<AActor*> VisibleThreats;
    ThreatTracker.GatherVisibleThreats(VisibleThreats);

    ThreatTracker.UpdateMemory(VisibleThreats);

    AActor* VisibleThreat = nullptr;
    float NearestDistanceSquared = TNumericLimits<float>::Max();

    for (AActor* Candidate : VisibleThreats)
    {
        const float DistanceSquared = FVector::DistSquared2D(Sacrifice->GetActorLocation(), Candidate->GetActorLocation());

        if (DistanceSquared < NearestDistanceSquared)
        {
            NearestDistanceSquared = DistanceSquared;
            VisibleThreat = Candidate;
        }
    }

    if (VisibleThreat)
    {
        bFinishGoalAfterThreatLost = false;

        if (!bThreatWasVisible || Sacrifice->GetCurrentThreat() != VisibleThreat)
        {
            AcquireThreat(VisibleThreat, false);
        }

        bThreatWasVisible = true;
        RefreshFleeMovement();

        return;
    }

    // 위협을 놓친 뒤에는 현재 이동 목표까지만 마치고 안전 회복 단계로 전환한다.
    if (bThreatWasVisible)
    {
        bThreatWasVisible = false;
        bFinishGoalAfterThreatLost = true;

        if (GetMoveStatus() == EPathFollowingStatus::Idle && Phase != ECMSacrificeBehaviorPhase::BackFall)
        {
            StartSafetyRecovery();
        }
    }
}

// 기억한 위협 분포가 크게 바뀐 경우에만 도주 방향과 경로를 다시 계산한다.
void ACMSacrificeAIController::RefreshFleeMovement()
{
    const bool bIsFleeing = Phase == ECMSacrificeBehaviorPhase::BackCrawl || Phase == ECMSacrificeBehaviorPhase::Escape;

    if (!bIsFleeing)
    {
        return;
    }

    TArray<FVector> ThreatLocations;
    ThreatTracker.GatherRememberedLocations(ThreatLocations);

    if (ThreatLocations.IsEmpty())
    {
        return;
    }

    const FVector Origin = Sacrifice->GetActorLocation();

    // 기존 진행 방향을 우선한다.
    FVector PreferredDirection = LastFleeDirection;

    // 아직 도주 방향이 정해진 적이 없다면
    // 현재 Threat의 반대 방향을 최초 기준으로 사용한다.
    if (PreferredDirection.IsNearlyZero())
    {
        if (AActor* CurrentThreat = Sacrifice->GetCurrentThreat())
        {
            PreferredDirection = Origin - CurrentThreat->GetActorLocation();
        }
        else
        {
            PreferredDirection = Sacrifice->GetActorForwardVector();
        }
    }

    const float FleeDistance = Phase == ECMSacrificeBehaviorPhase::BackCrawl ? 300.0f : 1000.0f;

    const FVector DesiredDirection = FCMSacrificeRules::CalculateFleeDirection(Origin, ThreatLocations, PreferredDirection, FleeDistance);

    const float MinimumDirectionDot = FMath::Cos(FMath::DegreesToRadians(FleeDirectionRefreshDegrees));

    if (!LastFleeDirection.IsNearlyZero() && FVector::DotProduct(LastFleeDirection.GetSafeNormal2D(), DesiredDirection.GetSafeNormal2D()) >= MinimumDirectionDot)
    {
        return;
    }

    MoveAwayFromThreat(FleeDistance);
}

// 새 위협을 현재 타깃으로 저장하고 필요할 때 최초 공포 반응을 시작한다.
void ACMSacrificeAIController::AcquireThreat(AActor* Threat, const bool bFromHit)
{
    if (!Threat || CannotAct())
    {
        return;
    }
    Sacrifice->SetCurrentThreat(Threat);
    bThreatWasVisible = true;

    const bool bAlreadyReacting = Phase == ECMSacrificeBehaviorPhase::BackFall || Phase == ECMSacrificeBehaviorPhase::BackCrawl || Phase == ECMSacrificeBehaviorPhase::Escape;
    if (!bAlreadyReacting || bFromHit)
    {
        if (!bAlreadyReacting && !bFromHit)
        {
            Sacrifice->PlayThreatScream();
        }
        StartThreatReaction();
    }
}

// 기존 행동을 중단하고 무작위로 넘어져 기거나 즉시 도주하는 반응을 선택한다.
void ACMSacrificeAIController::StartThreatReaction()
{
    StopBehaviorTimers();
    StopMovementForTransition();
    bFinishGoalAfterThreatLost = false;
    if (FMath::RandBool())
    {
        Phase = ECMSacrificeBehaviorPhase::BackFall;
        const float BackFallDuration = Sacrifice->BeginBackFallRootMotion();
        Sacrifice->ActivateSacrificeAction(UCMSacrificeBackFallAbility::StaticClass());
        GetWorldTimerManager().SetTimer(BackFallTimer, this, &ThisClass::StartBackCrawl, BackFallDuration > 0.0f ? BackFallDuration : Sacrifice->GetBackFallDuration(), false);
    }
    else
    {
        Phase = ECMSacrificeBehaviorPhase::Escape;
        StartEscapeLeg();
    }
}

// 넘어짐을 마친 뒤 위협을 마주 보는 후진 기기 상태와 단거리 도주를 시작한다.
void ACMSacrificeAIController::StartBackCrawl()
{
    if (CannotAct() || Phase != ECMSacrificeBehaviorPhase::BackFall)
    {
        return;
    }
    Sacrifice->FinishBackFallRootMotion();
    Phase = ECMSacrificeBehaviorPhase::BackCrawl;
    if (!bThreatWasVisible || bFinishGoalAfterThreatLost || !Sacrifice->GetCurrentThreat())
    {
        StartSafetyRecovery();
        return;
    }
    Sacrifice->ActivateSacrificeAction(UCMSacrificeBackCrawlAbility::StaticClass());
    ClearFocus(EAIFocusPriority::Gameplay);
    Sacrifice->GetCharacterMovement()->bOrientRotationToMovement = false;
    Sacrifice->GetCharacterMovement()->bUseControllerDesiredRotation = true;
    if (const AActor* CurrentThreat = Sacrifice->GetCurrentThreat())
    {
        const FVector ThreatDirection = (CurrentThreat->GetActorLocation() - Sacrifice->GetActorLocation()).GetSafeNormal2D();
        if (!ThreatDirection.IsNearlyZero())
        {
            SetControlRotation(FRotator(0.0f, ThreatDirection.Rotation().Yaw, 0.0f));
        }
    }
    const bool bMoveStarted = MoveAwayFromThreat(300.0f);
    if (!bMoveStarted && bFinishGoalAfterThreatLost)
    {
        StartSafetyRecovery();
    }
    else if (!bMoveStarted)
    {
        GetWorldTimerManager().SetTimer(MoveRetryTimer, this, &ThisClass::RetryBackCrawlMovement, 0.5f, false);
    }
}

// 이미 넘어져 기는 중에는 자세를 재진입하지 않고 경로만 다시 요청한다.
void ACMSacrificeAIController::RetryBackCrawlMovement()
{
    if (CannotAct() || Phase != ECMSacrificeBehaviorPhase::BackCrawl)
    {
        return;
    }
    if (!bThreatWasVisible || bFinishGoalAfterThreatLost || !Sacrifice->GetCurrentThreat())
    {
        StartSafetyRecovery();
        return;
    }
    if (Sacrifice->GetSacrificeActionState() != ECMSacrificeActionState::BackCrawl)
    {
        Sacrifice->ActivateSacrificeAction(UCMSacrificeBackCrawlAbility::StaticClass());
    }
    if (!MoveAwayFromThreat(300.0f))
    {
        GetWorldTimerManager().SetTimer(MoveRetryTimer, this, &ThisClass::RetryBackCrawlMovement, 0.5f, false);
    }
}

// 신체 손상과 남은 도주 횟수에 맞는 정상 도주 행동 및 경로를 시작한다.
void ACMSacrificeAIController::StartEscapeLeg()
{
    if (CannotAct())
    {
        return;
    }
    Phase = ECMSacrificeBehaviorPhase::Escape;
    if (Sacrifice->HasLostArmOrLeg())
    {
        StartInjuredCrawl();

        return;
    }
    ClearFocus(EAIFocusPriority::Gameplay);
    Sacrifice->GetCharacterMovement()->bOrientRotationToMovement = true;
    Sacrifice->GetCharacterMovement()->bUseControllerDesiredRotation = false;
    const bool bMoveStarted = MoveAwayFromThreat(1000.0f);
    if (bMoveStarted)
    {
        Sacrifice->ActivateSacrificeAction(Sacrifice->GetFleeCharges() > 0.0f ? UCMSacrificeFleeAbility::StaticClass() : UCMSacrificeExhaustedWalkAbility::StaticClass());
    }
    else if (bFinishGoalAfterThreatLost)
    {
        StartSafetyRecovery();
    }
    else
    {
        Sacrifice->CancelSacrificeActions();
        GetWorldTimerManager().SetTimer(MoveRetryTimer, this, &ThisClass::StartEscapeLeg, 0.5f, false);
    }
}

// 팔이나 다리를 잃은 Sacrifice가 부상 기기 상태로 도주를 이어가게 한다.
void ACMSacrificeAIController::StartInjuredCrawl()
{
    if (CannotAct())
    {
        return;
    }
    Phase = ECMSacrificeBehaviorPhase::Escape;
    ClearFocus(EAIFocusPriority::Gameplay);
    Sacrifice->GetCharacterMovement()->bOrientRotationToMovement = true;
    Sacrifice->GetCharacterMovement()->bUseControllerDesiredRotation = false;
    const bool bMoveStarted = MoveAwayFromThreat(1000.0f);
    if (bMoveStarted)
    {
        Sacrifice->ActivateSacrificeAction(UCMSacrificeInjuredCrawlAbility::StaticClass());
    }
    else if (bFinishGoalAfterThreatLost)
    {
        StartSafetyRecovery();
    }
    else
    {
        Sacrifice->CancelSacrificeActions();
        GetWorldTimerManager().SetTimer(MoveRetryTimer, this, &ThisClass::StartInjuredCrawl, 0.5f, false);
    }
}

// 이동 결과와 현재 행동 단계에 따라 도주 횟수 소모·다음 도주·안전 복구를 연결한다.
void ACMSacrificeAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);
    if (bIgnoreMoveCompletion || CannotAct())
    {
        return;
    }
    if (Phase == ECMSacrificeBehaviorPhase::Ambient)
    {
        if (Sacrifice->GetSacrificeActionState() == ECMSacrificeActionState::Wander)
        {
            Sacrifice->CancelSacrificeActions();
        }
        return;
    }
    if (bFinishGoalAfterThreatLost)
    {
        StartSafetyRecovery();

        return;
    }
    if (Phase == ECMSacrificeBehaviorPhase::BackCrawl)
    {
        RetryBackCrawlMovement();

        return;
    }
    if (Phase == ECMSacrificeBehaviorPhase::Escape)
    {
        if (Result.IsSuccess() && Sacrifice->GetSacrificeActionState() == ECMSacrificeActionState::Flee)
        {
            Sacrifice->ConsumeFleeCharge();
        }
        if (Sacrifice->GetSacrificeActionState() == ECMSacrificeActionState::InjuredCrawl)
        {
            StartInjuredCrawl();
        }
        else
        {
            StartEscapeLeg();
        }
    }
}

// 위협 기억을 지우고 부상 및 직전 자세에 따라 무력화·기상·경계 중 하나로 전환한다.
void ACMSacrificeAIController::StartSafetyRecovery()
{
    if (CannotAct())
    {
        return;
    }

    ThreatTracker.ClearMemory();

    const ECMSacrificeBehaviorPhase PreviousPhase = Phase;
    const ECMSacrificeActionState ActionBeforeRecovery = Sacrifice->GetSacrificeActionState();
    StopBehaviorTimers();
    StopMovementForTransition();
    ClearFocus(EAIFocusPriority::Gameplay);
    bFinishGoalAfterThreatLost = false;
    bResumeInjuredCrawlAfterHit = false;

    // 부상 기기를 이미 마친 개체는 재생 가능한 넘어짐을 반복하지 않고 바로 무력화한다.
    if (Sacrifice->HasLostArmOrLeg())
    {
        if (ActionBeforeRecovery == ECMSacrificeActionState::InjuredCrawl)
        {
            Phase = ECMSacrificeBehaviorPhase::Inactive;
            Sacrifice->EnterIncapacitated();

            return;
        }
        Phase = ECMSacrificeBehaviorPhase::HitReact;
        const float FallDuration = Sacrifice->BeginSafetyInjuryFall();
        if (FallDuration <= 0.0f)
        {
            Phase = ECMSacrificeBehaviorPhase::Inactive;
            Sacrifice->EnterIncapacitated();

            return;
        }
        GetWorldTimerManager().SetTimer(HitReactionTimer, this, &ThisClass::FinishSafetyInjuryFall, FallDuration, false);

        return;
    }
    if (PreviousPhase == ECMSacrificeBehaviorPhase::BackCrawl)
    {
        StartGettingUp();

        return;
    }
    StartVigilance();
}

// 안전해진 정상 개체를 기상 상태로 전환하고 애니메이션 길이에 맞춰 완료를 예약한다.
void ACMSacrificeAIController::StartGettingUp()
{
    if (CannotAct())
    {
        return;
    }
    Phase = ECMSacrificeBehaviorPhase::GettingUp;
    bResumeEscapeAfterGettingUp = false;
    Sacrifice->SetCurrentThreat(nullptr);
    const float GettingUpDuration = Sacrifice->BeginGettingUp(ECMSacrificeHitReactionDirection::Front);
    Sacrifice->ActivateSacrificeAction(UCMSacrificeGettingUpAbility::StaticClass());
    GetWorldTimerManager().SetTimer(GettingUpTimer, this, &ThisClass::FinishGettingUp, GettingUpDuration, false);
}

void ACMSacrificeAIController::StartHitGettingUp(const ECMSacrificeHitReactionDirection Direction)
{
    if (CannotAct() || Sacrifice->HasLostArmOrLeg())
    {
        return;
    }
    StopMovementForTransition();
    ClearFocus(EAIFocusPriority::Gameplay);
    Phase = ECMSacrificeBehaviorPhase::GettingUp;
    bResumeEscapeAfterGettingUp = true;
    const float GettingUpDuration = Sacrifice->BeginGettingUp(Direction);
    Sacrifice->ActivateSacrificeAction(UCMSacrificeGettingUpAbility::StaticClass());
    GetWorldTimerManager().SetTimer(GettingUpTimer, this, &ThisClass::FinishGettingUp, GettingUpDuration, false);
}

// 기상 상태를 정리하고 피격 중단 여부에 따라 도주 또는 평상 행동으로 복귀한다.
void ACMSacrificeAIController::FinishGettingUp()
{
    if (CannotAct() || Phase != ECMSacrificeBehaviorPhase::GettingUp)
    {
        return;
    }
    Sacrifice->FinishGettingUp();
    Sacrifice->RefillFleeCharges();
    Sacrifice->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Sacrifice->GetCharacterMovement()->bOrientRotationToMovement = true;
    Sacrifice->GetCharacterMovement()->bUseControllerDesiredRotation = false;
    if (bResumeEscapeAfterGettingUp)
    {
        bResumeEscapeAfterGettingUp = false;
        Phase = ECMSacrificeBehaviorPhase::Escape;
        StartEscapeLeg();

        return;
    }
    Phase = ECMSacrificeBehaviorPhase::Ambient;
    SelectAmbientAction();
}

void ACMSacrificeAIController::FinishSafetyInjuryFall()
{
    if (!Sacrifice || Phase != ECMSacrificeBehaviorPhase::HitReact)
    {
        return;
    }
    Sacrifice->FinishHitReaction();
    Phase = ECMSacrificeBehaviorPhase::Inactive;
    Sacrifice->EnterIncapacitated();
}

// 위협 소실 후 이동을 멈추고 제한시간 동안 주변을 경계하는 상태로 전환한다.
void ACMSacrificeAIController::StartVigilance()
{
    StopBehaviorTimers();
    Phase = ECMSacrificeBehaviorPhase::Vigilant;
    StopMovementForTransition();
    ClearFocus(EAIFocusPriority::Gameplay);
    bFinishGoalAfterThreatLost = false;
    Sacrifice->ActivateSacrificeAction(UCMSacrificeVigilantAbility::StaticClass());
    GetWorldTimerManager().SetTimer(VigilanceTimer, this, &ThisClass::FinishVigilance, 20.0f, false);
}

// 경계를 마치면 도주 횟수와 위협 상태를 초기화하고 평상 행동으로 복귀한다.
void ACMSacrificeAIController::FinishVigilance()
{
    if (CannotAct() || Phase != ECMSacrificeBehaviorPhase::Vigilant)
    {
        return;
    }
    Sacrifice->RefillFleeCharges();
    Sacrifice->SetCurrentThreat(nullptr);
    bResumeInjuredCrawlAfterHit = false;
    Phase = ECMSacrificeBehaviorPhase::Ambient;
    SelectAmbientAction();
}

// 기도·주변 살피기·근거리 배회 중 하나를 선택해 평상 행동을 실행한다.
void ACMSacrificeAIController::SelectAmbientAction()
{
    if (CannotAct() || Phase != ECMSacrificeBehaviorPhase::Ambient)
    {
        return;
    }
    StopMovementForTransition();
    const int32 Choice = FMath::RandRange(0, 2);
    if (Choice == 0)
    {
        Sacrifice->ActivateSacrificeAction(UCMSacrificePrayerAbility::StaticClass());
    }
    else if (Choice == 1)
    {
        Sacrifice->ActivateSacrificeAction(UCMSacrificeLookAroundAbility::StaticClass());
    }
    else
    {
        Sacrifice->ActivateSacrificeAction(UCMSacrificeWanderAbility::StaticClass());
        bool bMoveStarted = false;
        FNavLocation RandomPoint;
        if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
        {
            ANavigationData* NavigationData = Nav->GetNavDataForProps(Sacrifice->GetNavAgentPropertiesRef());
            if (NavigationData && Nav->GetRandomReachablePointInRadius(Sacrifice->GetActorLocation(), 300.0f, RandomPoint, NavigationData))
            {
                bMoveStarted = MoveToProjectedLocation(RandomPoint.Location, 30.0f);
            }
        }
        if (!bMoveStarted)
        {
            Sacrifice->CancelSacrificeActions();
        }
    }
    ScheduleAmbientAction();
}

void ACMSacrificeAIController::ScheduleAmbientAction()
{
    GetWorldTimerManager().SetTimer(AmbientActionTimer, this, &ThisClass::SelectAmbientAction, FMath::FRandRange(3.0f, 5.0f), false);
}

// 모든 기억 위협에서 멀어지면서 NavMesh에 투영 가능한 가장 연속적인 도주 경로를 찾는다.
bool ACMSacrificeAIController::MoveAwayFromThreat(const float DistanceCm)
{
    if (!Sacrifice)
    {
        return false;
    }

    AActor* CurrentThreat = Sacrifice->GetCurrentThreat();

    TArray<FVector> ThreatLocations;
    ThreatTracker.GatherRememberedLocations(ThreatLocations);

    if (ThreatLocations.IsEmpty())
    {
        if (!CurrentThreat)
        {
            return false;
        }

        ThreatLocations.Add(CurrentThreat->GetActorLocation());
    }

    const FVector Origin = Sacrifice->GetActorLocation();

    // CurrentThreat보다 기존 진행 방향을 우선한다.
    FVector PreferredDirection = LastFleeDirection;

    if (PreferredDirection.IsNearlyZero() && CurrentThreat)
    {
        PreferredDirection = Origin - CurrentThreat->GetActorLocation();
    }

    const FVector DesiredDirection = FCMSacrificeRules::CalculateFleeDirection(Origin, ThreatLocations, PreferredDirection, DistanceCm);

    // CalculateFleeDirection이 만든 방향 주변부터 검사한다.
    const float YawOffsets[] = {0.0f, 22.5f, -22.5f, 45.0f, -45.0f, 67.5f, -67.5f, 90.0f, -90.0f};

    const float DistanceScales[] = {1.0f, 0.75f, 0.5f, 0.3f};

    for (const float DistanceScale : DistanceScales)
    {
        const float CandidateDistance = FMath::Max(DistanceCm * DistanceScale, 150.0f);

        for (const float YawOffset : YawOffsets)
        {
            const FVector CandidateDirection = DesiredDirection.RotateAngleAxis(YawOffset, FVector::UpVector);

            if (!FCMSacrificeRules::IsDirectionAwayFromAllThreats(Origin, ThreatLocations, CandidateDirection, CandidateDistance))
            {
                continue;
            }

            const FVector Goal = Origin + CandidateDirection * CandidateDistance;

            if (MoveToProjectedLocation(Goal, 50.0f))
            {
                // 중요:
                // DesiredDirection이나 CurrentThreat 반대 방향이 아니라
                // 실제 선택된 방향을 기억한다.
                LastFleeDirection = CandidateDirection.GetSafeNormal2D();

                return true;
            }
        }
    }

    for (const float DistanceScale : DistanceScales)
    {
        const float CandidateDistance = FMath::Max(DistanceCm * DistanceScale, 150.0f);

        const FVector Goal = Origin + DesiredDirection * CandidateDistance;

        if (MoveToProjectedLocation(Goal, 50.0f))
        {
            LastFleeDirection = DesiredDirection.GetSafeNormal2D();

            return true;
        }
    }

    return false;
}

// 후보 위치를 NavMesh에 투영하고 동기 완료 콜백과 상태 전환의 재진입을 막아 이동한다.
bool ACMSacrificeAIController::MoveToProjectedLocation(const FVector& Goal, const float AcceptanceRadius)
{
    UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    ANavigationData* NavigationData = Nav && Sacrifice ? Nav->GetNavDataForProps(Sacrifice->GetNavAgentPropertiesRef()) : nullptr;
    FNavLocation Projected;
    if (!NavigationData || !Nav->ProjectPointToNavigation(Goal, Projected, FVector(200.0f, 200.0f, 300.0f), NavigationData))
    {
        return false;
    }
    if (!FCMSacrificeRules::HasMeaningfulProjectedMove(Sacrifice->GetActorLocation(), Projected.Location, AcceptanceRadius, MinimumMovementGoalDistanceCm))
    {
        return false;
    }

    if (Phase == ECMSacrificeBehaviorPhase::BackCrawl || Phase == ECMSacrificeBehaviorPhase::Escape)
    {
        TArray<FVector> ThreatLocations;
        ThreatTracker.GatherRememberedLocations(ThreatLocations);
        if (ThreatLocations.IsEmpty() && Sacrifice->GetCurrentThreat())
            ThreatLocations.Add(Sacrifice->GetCurrentThreat()->GetActorLocation());
        const FVector ProjectedMove = Projected.Location - Sacrifice->GetActorLocation();
        if (!ThreatLocations.IsEmpty() && !FCMSacrificeRules::IsDirectionAwayFromAllThreats(Sacrifice->GetActorLocation(), ThreatLocations, ProjectedMove, ProjectedMove.Size2D()))
            return false;
    }

    FNavLocation ProjectedStart;
    if (!Nav->ProjectPointToNavigation(Sacrifice->GetActorLocation(), ProjectedStart, FVector(200.0f, 200.0f, 300.0f), NavigationData))
        return false;
    FPathFindingQuery Query(this, *NavigationData, ProjectedStart.Location, Projected.Location);
    Query.SetAllowPartialPaths(false);
    Query.SetNavAgentProperties(Sacrifice->GetNavAgentPropertiesRef());
    const FPathFindingResult PathResult = Nav->FindPathSync(Sacrifice->GetNavAgentPropertiesRef(), Query);
    if (!PathResult.IsSuccessful() || !PathResult.Path.IsValid() || PathResult.Path->IsPartial())
        return false;

    const TArray<FNavPathPoint>& PathPoints = PathResult.Path->GetPathPoints();
    if (!CMSacrificePursuitNavigation::IsPathSupportedByAgent(*Nav, PathPoints, CMSacrificePursuitNavigation::RipperAgentName, this)
        || !CMSacrificePursuitNavigation::IsPathSupportedByAgent(*Nav, PathPoints, CMSacrificePursuitNavigation::CentipedeAgentName, this))
        return false;

    // 즉시 완료되는 Move 요청이 현재 상태 전환 도중 OnMoveCompleted를 재진입하지 않게 한다.
    TGuardValue<bool> IgnoreSynchronousCompletion(bIgnoreMoveCompletion, true);

    const bool bCanStrafe = Phase == ECMSacrificeBehaviorPhase::BackCrawl;
    return MoveToLocation(Projected.Location, AcceptanceRadius, true, true, true, bCanStrafe) == EPathFollowingRequestResult::RequestSuccessful;
}

// 승인된 절단 피격이 오면 행동을 중단하고 피격 방향에 맞는 반응 상태를 시작한다.
void ACMSacrificeAIController::HandleAcceptedHit(AActor* Attacker, AActor* SourcePart, const FVector ImpactDirection)
{
    AActor* Threat = Attacker ? Attacker : SourcePart;
    if (!Threat || CannotAct() || Sacrifice->IsHitReacting())
    {
        return;
    }

    bResumeInjuredCrawlAfterHit = FCMSacrificeRules::ShouldUseInjuredCrawlAfterHit(Phase == ECMSacrificeBehaviorPhase::BackCrawl, Sacrifice->HasLostArmOrLeg());

    StopBehaviorTimers();
    StopMovementForTransition();
    ClearFocus(EAIFocusPriority::Gameplay);
    Sacrifice->SetCurrentThreat(Threat);
    bThreatWasVisible = true;
    bFinishGoalAfterThreatLost = false;
    Phase = ECMSacrificeBehaviorPhase::HitReact;

    const float ReactionDuration = Sacrifice->BeginHitReaction(ImpactDirection);
    if (ReactionDuration <= 0.0f)
    {
        Phase = ECMSacrificeBehaviorPhase::Inactive;
        return;
    }
    GetWorldTimerManager().SetTimer(HitReactionTimer, this, &ThisClass::FinishHitReaction, ReactionDuration, false);
}

// 피격 반응 종료 뒤 예약된 무력화 또는 부상 기기·기상 상태로 분기한다.
void ACMSacrificeAIController::FinishHitReaction()
{
    if (!Sacrifice)
    {
        return;
    }
    const ECMSacrificeHitReactionDirection HitDirection = Sacrifice->GetHitReactionDirection();
    Sacrifice->FinishHitReaction();
    if (Sacrifice->EnterPendingIncapacitation())
    {
        Phase = ECMSacrificeBehaviorPhase::Inactive;
        return;
    }
    if (CannotAct())
    {
        Phase = ECMSacrificeBehaviorPhase::Inactive;
        return;
    }
    const bool bShouldResumeInjuredCrawl = FCMSacrificeRules::ShouldUseInjuredCrawlAfterHit(bResumeInjuredCrawlAfterHit, Sacrifice->HasLostArmOrLeg());
    bResumeInjuredCrawlAfterHit = false;
    if (bShouldResumeInjuredCrawl)
    {
        Phase = ECMSacrificeBehaviorPhase::Escape;
        StartInjuredCrawl();
    }
    else
    {
        StartHitGettingUp(HitDirection);
    }
}

// 사망한 Sacrifice의 상태 머신과 모든 예약 행동 및 이동을 중단한다.
void ACMSacrificeAIController::HandleSacrificeDied()
{
    Phase = ECMSacrificeBehaviorPhase::Inactive;
    StopBehaviorTimers();
    StopMovement();
}

void ACMSacrificeAIController::StopBehaviorTimers()
{
    GetWorldTimerManager().ClearTimer(AmbientActionTimer);
    GetWorldTimerManager().ClearTimer(BackFallTimer);
    GetWorldTimerManager().ClearTimer(VigilanceTimer);
    GetWorldTimerManager().ClearTimer(MoveRetryTimer);
    GetWorldTimerManager().ClearTimer(HitReactionTimer);
    GetWorldTimerManager().ClearTimer(GettingUpTimer);
}

// 상태 전환 중 이동 완료 콜백을 무시하며 현재 이동과 도주 방향을 초기화한다.
void ACMSacrificeAIController::StopMovementForTransition()
{
    LastFleeDirection = FVector::ZeroVector;
    StationaryMovementSeconds = 0.0f;
    bIgnoreMoveCompletion = true;
    StopMovement();
    bIgnoreMoveCompletion = false;
}

bool ACMSacrificeAIController::CannotAct() const
{
    return !Sacrifice || !Sacrifice->GetSacrificeStateComponent()->IsAlive() || Sacrifice->GetSacrificeActionState() == ECMSacrificeActionState::Incapacitated || Sacrifice->GetSacrificeActionState() == ECMSacrificeActionState::Dead;
}
