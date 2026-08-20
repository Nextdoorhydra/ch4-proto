#include "Movement/CMLineBodyMovementCoordinator.h"

#include "Player/CMControlTypes.h"
#include "DrawDebugHelpers.h"
#include "GameMode/CMGameState.h"
#include "Player/CMChimera.h"
#include "Player/CMControlBody.h"
#include "Player/CMPlayerState.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraMovement, Log, All);

UCMLineBodyMovementCoordinator::UCMLineBodyMovementCoordinator()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UCMLineBodyMovementCoordinator::TryActivateLeg(
    ACMChimera& Chimera,
    int32 SegmentIndex,
    USceneComponent* FootPoint,
    ACMPlayerState* ContributingPlayerState,
    float MovementImpulseMultiplier
)
{
    if (!Chimera.HasAuthority()
        || SegmentIndex < 0
        || SegmentIndex >= Chimera.ActiveSegmentCount
        || !Chimera.BodySegments.IsValidIndex(SegmentIndex))
    {
        return false;
    }

    return ApplyLegImpulse(
        Chimera,
        Chimera.BodySegments[SegmentIndex],
        FootPoint,
        ContributingPlayerState,
        MovementImpulseMultiplier
    );
}

bool UCMLineBodyMovementCoordinator::TryActivateArm(
    ACMChimera& Chimera,
    int32 SegmentIndex,
    USceneComponent* ImpulsePoint,
    ACMPlayerState* ContributingPlayerState,
    float MovementImpulseMultiplier
)
{
    if (!Chimera.HasAuthority()
        || SegmentIndex < 0
        || SegmentIndex >= Chimera.ActiveSegmentCount
        || !Chimera.BodySegments.IsValidIndex(SegmentIndex))
    {
        return false;
    }

    return ApplyArmImpulse(
        Chimera,
        Chimera.BodySegments[SegmentIndex],
        ImpulsePoint,
        ContributingPlayerState,
        MovementImpulseMultiplier
    );
}

bool UCMLineBodyMovementCoordinator::ApplyAnchorPull(
    ACMChimera& Chimera,
    int32 SegmentIndex,
    const FVector& AnchorLocation,
    float PullImpulse,
    float StopDistance
)
{
    if (!Chimera.HasAuthority()
        || PullImpulse <= 0.0f
        || SegmentIndex < 0
        || SegmentIndex >= Chimera.ActiveSegmentCount
        || !Chimera.BodySegments.IsValidIndex(SegmentIndex))
    {
        return false;
    }

    float TotalMass = 0.0f;
    FVector WeightedBodyCenter = FVector::ZeroVector;
    TArray<UStaticMeshComponent*> SimulatedSegments;
    for (int32 Index = 0; Index < Chimera.ActiveSegmentCount; ++Index)
    {
        UStaticMeshComponent* BodySegment =
            Chimera.BodySegments.IsValidIndex(Index)
            ? Chimera.BodySegments[Index]
            : nullptr;
        if (!BodySegment || !BodySegment->IsSimulatingPhysics())
        {
            continue;
        }

        const float SegmentMass = FMath::Max(BodySegment->GetMass(), 0.01f);
        TotalMass += SegmentMass;
        WeightedBodyCenter += BodySegment->GetCenterOfMass() * SegmentMass;
        SimulatedSegments.Add(BodySegment);
    }
    if (SimulatedSegments.IsEmpty() || TotalMass <= UE_SMALL_NUMBER)
    {
        return false;
    }

    const FVector BodyCenter = WeightedBodyCenter / TotalMass;
    const FVector AnchorOffset = AnchorLocation - BodyCenter;
    if (AnchorOffset.Size() <= FMath::Max(StopDistance, 0.0f))
    {
        return false;
    }

    const FVector PullDirection = AnchorOffset.GetSafeNormal();
    if (PullDirection.IsNearlyZero())
    {
        return false;
    }

    // Distribute one total impulse by mass. Every segment receives the same
    // velocity change, so the constraint chain translates without an
    // artificial yaw torque from pulling only one segment.
    for (UStaticMeshComponent* BodySegment : SimulatedSegments)
    {
        const float MassFraction = BodySegment->GetMass() / TotalMass;
        BodySegment->AddImpulse(
            PullDirection * PullImpulse * MassFraction
        );
    }

    UE_LOG(LogChimeraMovement, Verbose,
        TEXT("[SpringArm Body Pull] SourceSegment=%d SegmentCount=%d Anchor=%s TotalImpulse=%.1f"),
        SegmentIndex,
        SimulatedSegments.Num(),
        *AnchorLocation.ToCompactString(),
        PullImpulse);
    return true;
}

bool UCMLineBodyMovementCoordinator::ApplyLegImpulse(
    ACMChimera& Chimera,
    UStaticMeshComponent* SegmentBody,
    USceneComponent* FootPoint,
    ACMPlayerState* ContributingPlayerState,
    float MovementImpulseMultiplier
)
{
    if (!SegmentBody || !FootPoint)
    {
        return false;
    }

    FHitResult GroundHit;
    if (!TraceGround(Chimera, FootPoint, GroundHit))
    {
        return false;
    }

    FVector ForwardDirection = SegmentBody->GetForwardVector();
    ForwardDirection.Z = 0.0f;
    ForwardDirection.Normalize();

    const FVector Impulse =
        ForwardDirection
        * Chimera.BaseMovementImpulse
        * GetPerControlImpulseMultiplier(Chimera)
        * FMath::Max(MovementImpulseMultiplier, 0.0f);

    const float TranslationFraction = FMath::Clamp(
        Chimera.IndividualPlanarTranslationFraction,
        0.0f,
        1.0f
    );
    const float RotationFraction = FMath::Clamp(
        Chimera.IndividualYawRotationFraction,
        0.0f,
        1.0f
    );
    const FVector RotationalImpulse = Impulse * RotationFraction;
    SegmentBody->AddImpulseAtLocation(
        RotationalImpulse,
        GroundHit.ImpactPoint
    );
    SegmentBody->AddImpulse(
        Impulse * (TranslationFraction - RotationFraction)
    );

    const FVector LeverArm =
        GroundHit.ImpactPoint - SegmentBody->GetCenterOfMass();
    const float IntendedYawAngularImpulse =
        FVector::CrossProduct(LeverArm, Impulse).Z;
    RegisterCooperativeInput(
        Chimera,
        ContributingPlayerState,
        Impulse,
        IntendedYawAngularImpulse
    );

    return true;
}

bool UCMLineBodyMovementCoordinator::ApplyArmImpulse(
    ACMChimera& Chimera,
    UStaticMeshComponent* SegmentBody,
    USceneComponent* ImpulsePoint,
    ACMPlayerState* ContributingPlayerState,
    float MovementImpulseMultiplier
)
{
    if (!SegmentBody || !ImpulsePoint
        || MovementImpulseMultiplier <= 0.0f)
    {
        return false;
    }

    FVector ForwardDirection = SegmentBody->GetForwardVector();
    ForwardDirection.Z = 0.0f;
    if (!ForwardDirection.Normalize())
    {
        return false;
    }

    const FVector Impulse = ForwardDirection
        * Chimera.BaseMovementImpulse
        * GetPerControlImpulseMultiplier(Chimera)
        * MovementImpulseMultiplier;
    const float TranslationFraction = FMath::Clamp(
        Chimera.IndividualPlanarTranslationFraction,
        0.0f,
        1.0f
    );
    const float RotationFraction = FMath::Clamp(
        Chimera.IndividualYawRotationFraction,
        0.0f,
        1.0f
    );
    const FVector ImpulseLocation = ImpulsePoint->GetComponentLocation();
    SegmentBody->AddImpulseAtLocation(
        Impulse * RotationFraction,
        ImpulseLocation
    );
    SegmentBody->AddImpulse(
        Impulse * (TranslationFraction - RotationFraction)
    );

    const FVector LeverArm =
        ImpulseLocation - SegmentBody->GetCenterOfMass();
    const float IntendedYawAngularImpulse =
        FVector::CrossProduct(LeverArm, Impulse).Z;
    RegisterCooperativeInput(
        Chimera,
        ContributingPlayerState,
        Impulse,
        IntendedYawAngularImpulse
    );

    UE_LOG(LogChimeraMovement, Log,
        TEXT("[Arm Impulse] Segment=%d Scale=%.2f Base=%.1f Final=%.1f"),
        Chimera.BodySegments.IndexOfByKey(SegmentBody),
        MovementImpulseMultiplier,
        Chimera.BaseMovementImpulse,
        Impulse.Size());
    return true;
}

void UCMLineBodyMovementCoordinator::RegisterCooperativeInput(
    ACMChimera& Chimera,
    ACMPlayerState* ContributingPlayerState,
    const FVector& PlanarImpulse,
    float YawAngularImpulse
)
{
    if (!Chimera.HasAuthority()
        || !IsValid(ContributingPlayerState)
        || PlanarImpulse.IsNearlyZero())
    {
        return;
    }

    if (PendingCooperationContributions.IsEmpty())
    {
        Chimera.GetWorldTimerManager().SetTimer(
            CooperationFlushTimerHandle,
            this,
            &UCMLineBodyMovementCoordinator::FlushCooperativeInput,
            FMath::Max(Chimera.CooperationInputWindow, 0.01f),
            false
        );
    }

    const int32 ContributorId = ContributingPlayerState->GetUniqueID();
    if (!PendingCooperationContributions.Contains(ContributorId))
    {
        FVector HorizontalImpulse = PlanarImpulse;
        HorizontalImpulse.Z = 0.0f;
        PendingCooperationContributions.Add(
            ContributorId,
            HorizontalImpulse
        );
        PendingCooperationYawImpulses.Add(
            ContributorId,
            YawAngularImpulse
        );
    }
}

void UCMLineBodyMovementCoordinator::FlushCooperativeInput()
{
    ACMChimera* Chimera = Cast<ACMChimera>(GetOwner());
    const int32 ContributorCount =
        PendingCooperationContributions.Num();
    const ACMGameState* GameState = Chimera && Chimera->GetWorld()
        ? Chimera->GetWorld()->GetGameState<ACMGameState>()
        : nullptr;
    const int32 ActivePlayerCount = FMath::Clamp(
        GameState ? GameState->GetLobbyPlayerCount() : 1,
        1,
        CMControl::MaxPlayers
    );

    if (!Chimera
        || !Chimera->HasAuthority()
        || !Chimera->BodyMesh
        || ContributorCount <= 0)
    {
        PendingCooperationContributions.Reset();
        PendingCooperationYawImpulses.Reset();
        if (Chimera)
        {
            Chimera->GetWorldTimerManager().ClearTimer(
                CooperationFlushTimerHandle
            );
        }
        return;
    }

    const float ParticipationRatio = FMath::Clamp(
        static_cast<float>(ContributorCount) / ActivePlayerCount,
        0.0f,
        1.0f
    );

    FVector DirectionSum = FVector::ZeroVector;
    for (const TPair<int32, FVector>& Contribution
        : PendingCooperationContributions)
    {
        DirectionSum += Contribution.Value.GetSafeNormal2D();
    }

    const float DirectionCoherence =
        DirectionSum.Size2D() / ContributorCount;
    if (DirectionCoherence >= Chimera->CooperationDirectionThreshold)
    {
        const FVector CoherentDirection =
            DirectionSum.GetSafeNormal2D();
        float AlignedImpulseMagnitude = 0.0f;
        for (const TPair<int32, FVector>& Contribution
            : PendingCooperationContributions)
        {
            AlignedImpulseMagnitude += FMath::Max(
                0.0f,
                FVector::DotProduct(
                    Contribution.Value,
                    CoherentDirection
                )
            );
        }

        const float TranslationFraction = FMath::Clamp(
            Chimera->IndividualPlanarTranslationFraction,
            0.0f,
            1.0f
        );
        const float RetainedIndividualImpulse =
            AlignedImpulseMagnitude * TranslationFraction;
        const float MaximumFinalImpulse = FMath::Max(
            Chimera->MaximumCooperativePlanarImpulse,
            0.0f
        );
        const float TargetFinalImpulse =
            MaximumFinalImpulse
            * ParticipationRatio
            * DirectionCoherence;
        const float BonusImpulseMagnitude = FMath::Max(
            TargetFinalImpulse - RetainedIndividualImpulse,
            0.0f
        );
        Chimera->BodyMesh->AddImpulse(
            CoherentDirection * BonusImpulseMagnitude
        );
    }

    float YawDirectionSum = 0.0f;
    for (const TPair<int32, float>& Contribution
        : PendingCooperationYawImpulses)
    {
        if (!FMath::IsNearlyZero(Contribution.Value))
        {
            YawDirectionSum += FMath::Sign(Contribution.Value);
        }
    }

    const float YawDirectionCoherence =
        FMath::Abs(YawDirectionSum) / ContributorCount;
    if (YawDirectionCoherence >= Chimera->CooperationDirectionThreshold)
    {
        const float YawDirection = FMath::Sign(YawDirectionSum);
        float AlignedYawAngularImpulse = 0.0f;
        for (const TPair<int32, float>& Contribution
            : PendingCooperationYawImpulses)
        {
            if (FMath::Sign(Contribution.Value) == YawDirection)
            {
                AlignedYawAngularImpulse += FMath::Abs(
                    Contribution.Value
                );
            }
        }

        const float RotationFraction = FMath::Clamp(
            Chimera->IndividualYawRotationFraction,
            0.0f,
            1.0f
        );
        const float RetainedIndividualYawImpulse =
            AlignedYawAngularImpulse * RotationFraction;
        const float MaximumFinalYawImpulse = FMath::Max(
            Chimera->MaximumCooperativeYawAngularImpulse,
            0.0f
        );
        const float TargetFinalYawImpulse =
            MaximumFinalYawImpulse
            * ParticipationRatio
            * YawDirectionCoherence;
        const float YawBonusImpulseMagnitude = FMath::Max(
            TargetFinalYawImpulse - RetainedIndividualYawImpulse,
            0.0f
        );
        Chimera->BodyMesh->AddAngularImpulseInRadians(FVector(
            0.0f,
            0.0f,
            YawDirection * YawBonusImpulseMagnitude
        ));
    }

    PendingCooperationContributions.Reset();
    PendingCooperationYawImpulses.Reset();
    Chimera->GetWorldTimerManager().ClearTimer(
        CooperationFlushTimerHandle
    );
}

void UCMLineBodyMovementCoordinator::UpdateServerMovement(
    ACMChimera& Chimera
) const
{
    if (!Chimera.HasAuthority() || !Chimera.BodyMesh)
    {
        return;
    }

    FVector Velocity = Chimera.BodyMesh->GetPhysicsLinearVelocity();
    const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);
    const float EffectiveMaxSpeed =
    Chimera.IsSpringArmPulling()
        ? Chimera.SpringArmMaxSpeed
        : Chimera.MaxSpeed * GetPlayerCountSpeedMultiplier(Chimera);

    if (HorizontalVelocity.Size() > EffectiveMaxSpeed)
    {
        const FVector LimitedHorizontalVelocity =
            HorizontalVelocity.GetSafeNormal() * EffectiveMaxSpeed;
        Velocity.X = LimitedHorizontalVelocity.X;
        Velocity.Y = LimitedHorizontalVelocity.Y;
        Chimera.BodyMesh->SetPhysicsLinearVelocity(Velocity);
    }
}

float UCMLineBodyMovementCoordinator::GetPlayerCountSpeedMultiplier(
    const ACMChimera& Chimera
) const
{
    const ACMGameState* GameState = Chimera.GetWorld()
        ? Chimera.GetWorld()->GetGameState<ACMGameState>()
        : nullptr;
    const int32 PlayerCount = FMath::Clamp(
        GameState ? GameState->GetLobbyPlayerCount() : 1,
        1,
        CMControl::MaxPlayers
    );

    float TargetSeconds = Chimera.OnePlayerTurnTargetSeconds;
    if (PlayerCount == 2)
    {
        TargetSeconds = Chimera.TwoPlayerTurnTargetSeconds;
    }
    else if (PlayerCount == 3)
    {
        TargetSeconds = Chimera.ThreePlayerTurnTargetSeconds;
    }
    else if (PlayerCount >= 4)
    {
        TargetSeconds = Chimera.FourPlayerTurnTargetSeconds;
    }

    return FMath::Max(Chimera.OnePlayerTurnTargetSeconds, 0.1f)
        / FMath::Max(TargetSeconds, 0.1f);
}

float UCMLineBodyMovementCoordinator::GetPerControlImpulseMultiplier(
    const ACMChimera& Chimera
) const
{
    int32 AssignedControlCount = 0;
    if (Chimera.GetWorld())
    {
        for (TActorIterator<ACMControlBody> It(Chimera.GetWorld());
            It;
            ++It)
        {
            const ACMControlBody* ControlBody = *It;
            const ACMPlayerState* PlayerState = ControlBody
                ? ControlBody->GetPlayerState<ACMPlayerState>()
                : nullptr;
            // 죽은 마디를 소유한 플레이어는 ControlBody 자체는 월드에 남아 있지만
            // 더 이상 Q/W/E/R을 누를 수 없습니다. 해당 4칸을 힘 분배 인원에
            // 포함하면 살아 있는 플레이어의 실제 입력 힘까지 불필요하게 줄어듭니다.
            if (PlayerState
                && !PlayerState->IsOnlyASpectator()
                && ControlBody->IsControlInputEnabled())
            {
                AssignedControlCount +=
                    ControlBody->GetAssignedControlCount();
            }
        }
    }

    constexpr float SinglePlayerControlCount = 4.0f;
    return GetPlayerCountSpeedMultiplier(Chimera)
        * SinglePlayerControlCount
        / FMath::Max(static_cast<float>(AssignedControlCount), 1.0f);
}

bool UCMLineBodyMovementCoordinator::TraceGround(
    const ACMChimera& Chimera,
    USceneComponent* FootPoint,
    FHitResult& OutHit
) const
{
    if (!FootPoint || !Chimera.GetWorld())
    {
        return false;
    }

    const FVector Start = FootPoint->GetComponentLocation();
    const FVector End =
        Start - FVector::UpVector * Chimera.GroundContactDistance;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(&Chimera);

    TArray<FHitResult> Hits;
    Chimera.GetWorld()->SweepMultiByChannel(
        Hits,
        Start,
        End,
        FQuat::Identity,
        Chimera.GroundTraceChannel,
        FCollisionShape::MakeSphere(Chimera.GroundCheckRadius),
        QueryParams
    );

    bool bHasGroundContact = false;
    for (const FHitResult& Hit : Hits)
    {
        if (Hit.ImpactNormal.Z >= Chimera.MinimumGroundNormalZ)
        {
            OutHit = Hit;
            bHasGroundContact = true;
            break;
        }
    }

    if (Chimera.bDrawGroundContactDebug)
    {
        DrawDebugSphere(
            Chimera.GetWorld(),
            End,
            Chimera.GroundCheckRadius,
            12,
            bHasGroundContact ? FColor::Green : FColor::Red,
            false,
            0.5f,
            0,
            1.5f
        );
    }

    return bHasGroundContact;
}

void UCMLineBodyMovementCoordinator::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            CooperationFlushTimerHandle
        );
    }
    PendingCooperationContributions.Reset();
    PendingCooperationYawImpulses.Reset();

    Super::EndPlay(EndPlayReason);
}
