#include "Player/CMChimera.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Movement/CMLineBodyMovementCoordinator.h"
#include "Parts/Arm/CMSpringArmPart.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

float CMChimeraPhysics::ResolveLinearSpeedLimit(
    const float RequestedSpeed,
    const float MaximumSafeSpeed)
{
    return FMath::Clamp(
        RequestedSpeed,
        0.0f,
        FMath::Max(MaximumSafeSpeed, 0.0f));
}

FVector CMChimeraPhysics::ClampLinearVelocity(
    const FVector& Velocity,
    const float MaximumSafeSpeed)
{
    const float SafeLimit = FMath::Max(MaximumSafeSpeed, 0.0f);
    return SafeLimit > 0.0f
        ? Velocity.GetClampedToMaxSize(SafeLimit)
        : FVector::ZeroVector;
}

bool CMChimeraPhysics::IsBlockingPlanarContact(
    const FVector& MovementDirection,
    const FVector& ContactNormal,
    const float MinimumOppositionDot)
{
    const FVector PlanarDirection = FVector(
        MovementDirection.X,
        MovementDirection.Y,
        0.0f).GetSafeNormal();
    const FVector PlanarNormal = FVector(
        ContactNormal.X,
        ContactNormal.Y,
        0.0f).GetSafeNormal();
    return !PlanarDirection.IsNearlyZero()
        && !PlanarNormal.IsNearlyZero()
        && FVector::DotProduct(PlanarDirection, PlanarNormal)
            <= -FMath::Clamp(MinimumOppositionDot, 0.0f, 1.0f);
}

namespace
{
void BuildCurledRespawnPose(int32 SegmentCount, float SegmentSpacing, float BodyHalfLength, float BodyHalfWidth, float SignedBendDegrees, TArray<FTransform>& OutTransforms)
{
    OutTransforms.Reset(SegmentCount);
    FVector SegmentLocation = FVector::ZeroVector;
    FBox2D PoseBounds(ForceInit);

    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        const FQuat SegmentRotation = FRotator(0.0f, SignedBendDegrees * Index, 0.0f).Quaternion();
        if (Index > 0)
        {
            const FQuat PreviousRotation = OutTransforms[Index - 1].GetRotation();
            SegmentLocation -= (PreviousRotation.GetForwardVector() + SegmentRotation.GetForwardVector()) * SegmentSpacing * 0.5f;
        }

        OutTransforms.Emplace(SegmentRotation, SegmentLocation);
        const FVector Forward = SegmentRotation.GetForwardVector();
        const FVector Right = SegmentRotation.GetRightVector();
        const FVector2D BoundsExtent(FMath::Abs(Forward.X) * BodyHalfLength + FMath::Abs(Right.X) * BodyHalfWidth, FMath::Abs(Forward.Y) * BodyHalfLength + FMath::Abs(Right.Y) * BodyHalfWidth);
        PoseBounds += FVector2D(SegmentLocation) - BoundsExtent;
        PoseBounds += FVector2D(SegmentLocation) + BoundsExtent;
    }

    const FVector2D PoseCenter = PoseBounds.GetCenter();
    for (FTransform& Transform : OutTransforms)
    {
        Transform.AddToTranslation(FVector(-PoseCenter.X, -PoseCenter.Y, 0.0f));
    }
}
}

// 바람·컨베이어 가속도를 질량과 무관하게 적용해 정지 마찰을 넘김
void ACMChimera::ApplyEnvironmentalForce(const FVector& Acceleration)
{
    if (!HasAuthority()
        || Acceleration.IsNearlyZero()
        || StuckRecoveryCooldownRemaining > 0.0f)
    {
        return;
    }

    const int32 SegmentCount = FMath::Min(ActiveSegmentCount, BodySegments.Num());
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        ApplyEnvironmentalForceToSegment(Index, Acceleration);
    }
}

void ACMChimera::ApplyEnvironmentalForceToSegment(
    int32 SegmentIndex,
    const FVector& Acceleration)
{
    if (!HasAuthority()
        || Acceleration.IsNearlyZero()
        || SegmentIndex < 0
        || SegmentIndex >= ActiveSegmentCount
        || !BodySegments.IsValidIndex(SegmentIndex)
        || StuckRecoveryCooldownRemaining > 0.0f)
    {
        return;
    }

    UBoxComponent* SegmentBody = BodySegments[SegmentIndex];
    if (SegmentBody && SegmentBody->IsSimulatingPhysics())
    {
        SegmentBody->AddForce(Acceleration, NAME_None, true);
    }
}

float ACMChimera::GetAssemblyVelocityAlongDirection(
    const FVector& WorldDirection) const
{
    const FVector Direction = WorldDirection.GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        return 0.0f;
    }

    FVector MassWeightedVelocity = FVector::ZeroVector;
    float TotalMass = 0.0f;
    const int32 SegmentCount = FMath::Min(ActiveSegmentCount, BodySegments.Num());
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        UBoxComponent* SegmentBody = BodySegments[Index];
        if (!SegmentBody || !SegmentBody->IsSimulatingPhysics())
        {
            continue;
        }

        const float SegmentMass = FMath::Max(SegmentBody->GetMass(), 0.01f);
        MassWeightedVelocity += SegmentBody->GetPhysicsLinearVelocity()
            * SegmentMass;
        TotalMass += SegmentMass;
    }

    return TotalMass > UE_SMALL_NUMBER
        ? FVector::DotProduct(MassWeightedVelocity / TotalMass, Direction)
        : 0.0f;
}

void ACMChimera::HandleBodySegmentHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (!HasAuthority()
        || !bPlanarKnockbackActive
        || OtherActor == this)
    {
        return;
    }

    const FVector ContactNormal = !Hit.ImpactNormal.IsNearlyZero()
        ? Hit.ImpactNormal
        : Hit.Normal;
    if (!CMChimeraPhysics::IsBlockingPlanarContact(
            PlanarKnockbackDirection,
            ContactNormal))
    {
        return;
    }

    bPlanarKnockbackActive = false;
    SetBodyHitNotifications(false);
    FVector PlanarNormal(ContactNormal.X, ContactNormal.Y, 0.0f);
    PlanarNormal.Normalize();
    const int32 SegmentCount = FMath::Min(
        ActiveSegmentCount,
        BodySegments.Num());
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        UBoxComponent* SegmentBody = BodySegments[Index];
        if (!SegmentBody || !SegmentBody->IsSimulatingPhysics())
        {
            continue;
        }

        FVector Velocity = SegmentBody->GetPhysicsLinearVelocity();
        const float IntoSurfaceSpeed = FVector::DotProduct(
            Velocity,
            PlanarNormal);
        if (IntoSurfaceSpeed < 0.0f)
        {
            Velocity -= PlanarNormal * IntoSurfaceSpeed;
            SegmentBody->SetPhysicsLinearVelocity(Velocity);
        }
    }

    UE_LOG(LogChimeraLineBody, Verbose,
        TEXT("[Planar Knockback Stopped] Segment=%s Other=%s Normal=%s"),
        *GetNameSafe(HitComponent),
        *GetNameSafe(OtherActor),
        *ContactNormal.ToCompactString());
}

void ACMChimera::ResetStuckRecoveryHistory()
{
    SafeAssemblySnapshots.Reset();
    StuckRecoverySnapshotElapsed = 0.0f;
    StuckRecoveryUnsafeElapsed = 0.0f;
    StuckRecoveryCooldownRemaining = 0.0f;
}

void ACMChimera::SetBodyHitNotifications(const bool bEnabled)
{
    const int32 SegmentCount = FMath::Min(
        ActiveSegmentCount,
        BodySegments.Num());
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        if (UBoxComponent* SegmentBody = BodySegments[Index])
        {
            SegmentBody->SetNotifyRigidBodyCollision(bEnabled);
        }
    }
}

bool ACMChimera::IsAssemblyPlacementClear(
    const TArray<FTransform>& SegmentTransforms,
    const float ProbeInset) const
{
    const UWorld* World = GetWorld();
    const int32 SegmentCount = FMath::Min(
        ActiveSegmentCount,
        BodySegments.Num());
    if (!World || SegmentCount <= 0
        || SegmentTransforms.Num() != SegmentCount)
    {
        return false;
    }

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(CMStuckRecovery),
        false,
        this);
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors, true, true);
    QueryParams.AddIgnoredActors(AttachedActors);

    const float SafeInset = FMath::Max(ProbeInset, 0.0f);
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        const UBoxComponent* SegmentBody = BodySegments[Index];
        if (!IsValid(SegmentBody))
        {
            return false;
        }

        FVector CollisionExtent = SegmentBody->GetScaledBoxExtent()
            - FVector(SafeInset);
        CollisionExtent.X = FMath::Max(CollisionExtent.X, 1.0f);
        CollisionExtent.Y = FMath::Max(CollisionExtent.Y, 1.0f);
        CollisionExtent.Z = FMath::Max(CollisionExtent.Z, 1.0f);
        const FTransform& Transform = SegmentTransforms[Index];
        if (World->OverlapBlockingTestByProfile(
                Transform.GetLocation(),
                Transform.GetRotation(),
                SegmentBody->GetCollisionProfileName(),
                FCollisionShape::MakeBox(CollisionExtent),
                QueryParams))
        {
            return false;
        }
    }
    return true;
}

void ACMChimera::SaveSafeAssemblySnapshot()
{
    const int32 SegmentCount = FMath::Min(
        ActiveSegmentCount,
        BodySegments.Num());
    if (SegmentCount <= 0)
    {
        return;
    }

    FSafeAssemblySnapshot Snapshot;
    Snapshot.SegmentTransforms.Reserve(SegmentCount);
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        const UBoxComponent* SegmentBody = BodySegments[Index];
        if (!IsValid(SegmentBody))
        {
            return;
        }
        Snapshot.SegmentTransforms.Add(SegmentBody->GetComponentTransform());
    }

    SafeAssemblySnapshots.Add(MoveTemp(Snapshot));
    const int32 MaximumHistory = FMath::Clamp(
        StuckRecoveryHistorySize,
        1,
        32);
    if (SafeAssemblySnapshots.Num() > MaximumHistory)
    {
        SafeAssemblySnapshots.RemoveAt(
            0,
            SafeAssemblySnapshots.Num() - MaximumHistory,
            EAllowShrinking::No);
    }
}

bool ACMChimera::RestoreLatestSafeAssemblySnapshot()
{
    for (int32 SnapshotIndex = SafeAssemblySnapshots.Num() - 1;
        SnapshotIndex >= 0;
        --SnapshotIndex)
    {
        const FSafeAssemblySnapshot& Snapshot =
            SafeAssemblySnapshots[SnapshotIndex];
        if (!IsAssemblyPlacementClear(
                Snapshot.SegmentTransforms,
                StuckRecoveryProbeInset))
        {
            continue;
        }

        bPlanarKnockbackActive = false;
        SetBodyHitNotifications(false);
        ClearPressedControlParts();
        if (MovementCoordinator)
        {
            MovementCoordinator->CancelAllMovement();
        }
        if (ACMSpringArmPart* SpringArm = ActiveSpringArmPull.Get())
        {
            SpringArm->CancelBodyPullFromOverride();
        }
        ActiveSpringArmPull.Reset();

        for (int32 Index = 0;
            Index < Snapshot.SegmentTransforms.Num();
            ++Index)
        {
            UBoxComponent* SegmentBody = BodySegments[Index];
            SegmentBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
            SegmentBody->SetPhysicsAngularVelocityInRadians(
                FVector::ZeroVector);
            SegmentBody->SetWorldTransform(
                Snapshot.SegmentTransforms[Index],
                false,
                nullptr,
                ETeleportType::TeleportPhysics);
            SegmentBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
            SegmentBody->SetPhysicsAngularVelocityInRadians(
                FVector::ZeroVector);
            SegmentBody->WakeAllRigidBodies();
        }

        StuckRecoveryUnsafeElapsed = 0.0f;
        StuckRecoverySnapshotElapsed = 0.0f;
        StuckRecoveryCooldownRemaining = FMath::Max(
            StuckRecoveryCooldown,
            0.0f);
        UpdateReplicatedSegmentStates();
        ForceNetUpdate();
        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Stuck Recovery] Restored snapshot %d/%d with %d segments."),
            SnapshotIndex + 1,
            SafeAssemblySnapshots.Num(),
            Snapshot.SegmentTransforms.Num());
        return true;
    }
    return false;
}

void ACMChimera::UpdateStuckRecovery(const float DeltaTime)
{
    if (!HasAuthority() || !bEnableStuckRecovery)
    {
        return;
    }

    StuckRecoveryCooldownRemaining = FMath::Max(
        StuckRecoveryCooldownRemaining - DeltaTime,
        0.0f);

    const int32 SegmentCount = FMath::Min(
        ActiveSegmentCount,
        BodySegments.Num());
    TArray<FTransform> CurrentTransforms;
    CurrentTransforms.Reserve(SegmentCount);
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        const UBoxComponent* SegmentBody = BodySegments[Index];
        if (!IsValid(SegmentBody))
        {
            return;
        }
        CurrentTransforms.Add(SegmentBody->GetComponentTransform());
    }

    if (IsAssemblyPlacementClear(
            CurrentTransforms,
            StuckRecoveryProbeInset))
    {
        StuckRecoveryUnsafeElapsed = 0.0f;
        StuckRecoverySnapshotElapsed += DeltaTime;
        if (SafeAssemblySnapshots.IsEmpty()
            || StuckRecoverySnapshotElapsed
                >= FMath::Max(StuckRecoverySnapshotInterval, 0.02f))
        {
            SaveSafeAssemblySnapshot();
            StuckRecoverySnapshotElapsed = 0.0f;
        }
        return;
    }

    StuckRecoverySnapshotElapsed = 0.0f;
    if (StuckRecoveryCooldownRemaining > 0.0f)
    {
        return;
    }

    StuckRecoveryUnsafeElapsed += DeltaTime;
    if (StuckRecoveryUnsafeElapsed
        < FMath::Max(StuckRecoveryDetectionTime, 0.05f))
    {
        return;
    }

    if (!RestoreLatestSafeAssemblySnapshot())
    {
        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Stuck Recovery Skipped] No collision-free snapshot is available."));
        StuckRecoveryUnsafeElapsed = 0.0f;
        StuckRecoveryCooldownRemaining = FMath::Max(
            StuckRecoveryCooldown,
            0.0f);
    }
}

// 활성 몸통 마디의 현재 상대 배치를 유지하고 속도를 제거한 뒤 서버에서 일괄 이동
bool ACMChimera::TeleportAssembly(const FTransform& DestinationTransform)
{
    if (!HasAuthority() || !BodyMesh || ActiveSegmentCount <= 0)
    {
        return false;
    }

    const int32 SegmentCount = FMath::Min(ActiveSegmentCount, BodySegments.Num());
    const FTransform SourceTransform = BodyMesh->GetComponentTransform();
    TArray<FTransform> DestinationTransforms;
    DestinationTransforms.Reserve(SegmentCount);

    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        const UBoxComponent* SegmentBody = BodySegments[Index];
        if (!IsValid(SegmentBody))
        {
            return false;
        }
        const FTransform RelativeTransform =
            SegmentBody->GetComponentTransform().GetRelativeTransform(SourceTransform);
        DestinationTransforms.Add(RelativeTransform * DestinationTransform);
    }

    ClearPressedControlParts();
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        UBoxComponent* SegmentBody = BodySegments[Index];
        SegmentBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
        SegmentBody->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        SegmentBody->SetWorldTransform(
            DestinationTransforms[Index],
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        SegmentBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
        SegmentBody->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        SegmentBody->WakeAllRigidBodies();
    }

    UpdateReplicatedSegmentStates();
    ForceNetUpdate();
    ResetStuckRecoveryHistory();
    return true;
}

bool ACMChimera::TeleportAssemblyForCheckpointRespawn(const FTransform& CheckpointTransform)
{
    UWorld* World = GetWorld();
    const int32 SegmentCount = FMath::Min(ActiveSegmentCount, BodySegments.Num());
    if (!HasAuthority() || !World || !BodyMesh
        || SegmentCount <= 0)
    {
        return false;
    }

    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        if (!IsValid(BodySegments[Index]))
        {
            return false;
        }
    }

    const float IdealBendDegrees = 360.0f / SegmentCount;
    const float SafeBendLimitDegrees = FMath::Max(HorizontalBendLimitDegrees - CheckpointRespawnBendSafetyMargin, 0.0f);
    const float BendDegrees = FMath::Min(IdealBendDegrees, SafeBendLimitDegrees);
    const float CheckpointYaw = CheckpointTransform.Rotator().Yaw;
    const FQuat CheckpointRotation = FRotator(0.0f, CheckpointYaw, 0.0f).Quaternion();
    const FVector CheckpointLocation = CheckpointTransform.GetLocation();

    TArray<FVector2D> CandidateOffsets;
    CandidateOffsets.Add(FVector2D::ZeroVector);
    constexpr int32 DirectionsPerRing = 8;
    for (int32 RingIndex = 1; RingIndex <= CheckpointRespawnSearchRings; ++RingIndex)
    {
        const float Radius = CheckpointRespawnSearchStep * RingIndex;
        for (int32 DirectionIndex = 0; DirectionIndex < DirectionsPerRing; ++DirectionIndex)
        {
            const float AngleRadians = UE_TWO_PI * DirectionIndex / DirectionsPerRing;
            CandidateOffsets.Add(FVector2D(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians)) * Radius);
        }
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMCheckpointRespawn), false, this);
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors, true, true);
    QueryParams.AddIgnoredActors(AttachedActors);

    TArray<FTransform> LocalTransforms;
    TArray<FTransform> CandidateTransforms;
    CandidateTransforms.Reserve(SegmentCount);
    for (const FVector2D& CandidateOffset : CandidateOffsets)
    {
        const FVector WorldOffset = CheckpointRotation.RotateVector(FVector(CandidateOffset, 0.0f));
        const FVector TraceOrigin = CheckpointLocation + WorldOffset;
        const FVector TraceStart = TraceOrigin + FVector::UpVector * CheckpointRespawnGroundTraceHeight;
        const FVector TraceEnd = TraceOrigin - FVector::UpVector * CheckpointRespawnGroundTraceDepth;
        FHitResult GroundHit;
        if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, GroundTraceChannel, QueryParams)
            || GroundHit.ImpactNormal.Z < CheckpointRespawnMinimumGroundNormalZ)
        {
            continue;
        }

        for (const float CurlDirection : { 1.0f, -1.0f })
        {
            BuildCurledRespawnPose(SegmentCount, SegmentSpacing, BodyCollisionHalfLength, BodyCollisionHalfWidth, BendDegrees * CurlDirection, LocalTransforms);
            CandidateTransforms.Reset(SegmentCount);
            const float RespawnZ = GroundHit.ImpactPoint.Z + BodyCollisionHalfHeight + CheckpointRespawnGroundClearance;
            const FVector PoseOrigin(TraceOrigin.X, TraceOrigin.Y, RespawnZ);
            bool bBlocked = false;

            for (int32 Index = 0; Index < SegmentCount; ++Index)
            {
                const FQuat WorldRotation = CheckpointRotation * LocalTransforms[Index].GetRotation();
                const FVector WorldLocation = PoseOrigin + CheckpointRotation.RotateVector(LocalTransforms[Index].GetLocation());
                const FVector CollisionExtent = BodySegments[Index]->GetScaledBoxExtent() + FVector(CheckpointRespawnCollisionPadding);
                CandidateTransforms.Emplace(WorldRotation, WorldLocation);
                if (World->OverlapBlockingTestByProfile(WorldLocation, WorldRotation, BodySegments[Index]->GetCollisionProfileName(), FCollisionShape::MakeBox(CollisionExtent), QueryParams))
                {
                    bBlocked = true;
                    break;
                }
            }

            if (bBlocked)
            {
                continue;
            }

            ClearPressedControlParts();
            for (int32 Index = 0; Index < SegmentCount; ++Index)
            {
                UBoxComponent* SegmentBody = BodySegments[Index];
                SegmentBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
                SegmentBody->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
                SegmentBody->SetWorldTransform(CandidateTransforms[Index], false, nullptr, ETeleportType::TeleportPhysics);
                SegmentBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
                SegmentBody->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
                SegmentBody->WakeAllRigidBodies();
            }

            UpdateReplicatedSegmentStates();
            ForceNetUpdate();
            ResetStuckRecoveryHistory();
            UE_LOG(LogChimeraLineBody, Display, TEXT("[Checkpoint Respawn Pose] Segments=%d Bend=%.1f Direction=%s Offset=(%.1f, %.1f) GroundZ=%.1f"), SegmentCount, BendDegrees, CurlDirection > 0.0f ? TEXT("Clockwise") : TEXT("CounterClockwise"), CandidateOffset.X, CandidateOffset.Y, GroundHit.ImpactPoint.Z);
            return true;
        }
    }

    UE_LOG(LogChimeraLineBody, Error, TEXT("[Checkpoint Respawn Failed] No collision-free curled pose found. Segments=%d Checkpoint=%s SearchRadius=%.1f"), SegmentCount, *CheckpointLocation.ToCompactString(), CheckpointRespawnSearchStep * CheckpointRespawnSearchRings);
    return false;
}

void ACMChimera::ConfigureSegments()
{
    ApplyBlueprintSettings();

    for (int32 Index = 0; Index < BodySegments.Num(); ++Index)
    {
        UBoxComponent* SegmentBody = BodySegments[Index];
        UBoxComponent* SegmentHurtbox = SegmentHurtboxes.IsValidIndex(Index)
            ? SegmentHurtboxes[Index]
            : nullptr;
        const bool bIsActive = Index < ActiveSegmentCount;

        if (!SegmentBody)
        {
            continue;
        }

        SegmentBody->SetBoxExtent(FVector(
            BodyCollisionHalfLength,
            BodyCollisionHalfWidth,
            BodyCollisionHalfHeight
        ));
        SegmentBody->SetWorldScale3D(FVector::OneVector);
        SegmentBody->SetHiddenInGame(true);
        SegmentBody->SetCollisionEnabled(
            bIsActive
                ? ECollisionEnabled::QueryAndPhysics
                : ECollisionEnabled::NoCollision
        );
        SegmentBody->SetMobility(EComponentMobility::Movable);
        SegmentBody->SetSimulatePhysics(bIsActive);
        SegmentBody->SetUseCCD(bIsActive && bUseBodyCCD, NAME_None);
        SegmentBody->SetNotifyRigidBodyCollision(false);
        SegmentBody->OnComponentHit.AddUniqueDynamic(
            this,
            &ACMChimera::HandleBodySegmentHit);
        if (SegmentHurtbox)
        {
            SegmentHurtbox->SetCollisionEnabled(
                bIsActive
                    ? ECollisionEnabled::QueryOnly
                    : ECollisionEnabled::NoCollision
            );
        }
        if (bIsActive)
        {
            ConfigureBodyRotationLock(SegmentBody);
        }
    }

    // Constraints exist only for the server's Chaos simulation. Both bodies
    // must already be dynamic before the joint is registered.
    if (!HasAuthority())
    {
        return;
    }

    ResetStuckRecoveryHistory();

    for (UPhysicsConstraintComponent* ExistingConstraint
        : SegmentConstraints)
    {
        if (ExistingConstraint)
        {
            ExistingConstraint->DestroyComponent();
        }
    }
    SegmentConstraints.Reset();
    SegmentConstraints.Reserve(FMath::Max(ActiveSegmentCount - 1, 0));
    for (int32 Index = 0; Index < ActiveSegmentCount - 1; ++Index)
    {
        UBoxComponent* FrontBody = BodySegments[Index];
        UBoxComponent* RearBody = BodySegments[Index + 1];

        if (!FrontBody
            || !RearBody
            || !FrontBody->IsSimulatingPhysics()
            || !RearBody->IsSimulatingPhysics())
        {
            UE_LOG(LogChimeraLineBody, Error,
                TEXT("[Constraint Skipped] Segment pair %d-%d is not ready for physics. Front=%d Rear=%d"),
                Index,
                Index + 1,
                FrontBody && FrontBody->IsSimulatingPhysics(),
                RearBody && RearBody->IsSimulatingPhysics());
            continue;
        }

        const FVector RearLocation =
            FrontBody->GetComponentLocation()
            - FrontBody->GetForwardVector()
                * SegmentSpacing;
        RearBody->SetWorldLocationAndRotation(
            RearLocation,
            FrontBody->GetComponentQuat()
        );

        UPhysicsConstraintComponent* Constraint =
            NewObject<UPhysicsConstraintComponent>(
                this,
                MakeUniqueObjectName(
                    this,
                    UPhysicsConstraintComponent::StaticClass(),
                    *FString::Printf(
                        TEXT("SegmentConstraint_%d"),
                        Index
                    )
                )
            );
        if (!Constraint)
        {
            UE_LOG(LogChimeraLineBody, Error,
                TEXT("[Constraint Create Failed] Segment pair %d-%d"),
                Index,
                Index + 1);
            continue;
        }

        Constraint->SetupAttachment(BodyMesh);
        Constraint->ComponentName1.ComponentName = FrontBody->GetFName();
        Constraint->ComponentName2.ComponentName = RearBody->GetFName();
        Constraint->OverrideComponent1 = FrontBody;
        Constraint->OverrideComponent2 = RearBody;
        Constraint->SetWorldLocation(
            (FrontBody->GetComponentLocation()
                + RearBody->GetComponentLocation()) * 0.5f
        );
        Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
        Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
        Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
        // Constraint의 기준 전방축은 X입니다.
        // Swing1은 Z축 회전(Yaw)이므로 몸통의 좌우 굽힘 각도를 담당합니다.
        Constraint->SetAngularSwing1Limit(
            EAngularConstraintMotion::ACM_Limited,
            HorizontalBendLimitDegrees
        );

        // Swing2는 Y축 회전(Pitch)이므로 언덕을 오를 때 필요한 위아래 굽힘 각도를 담당합니다.
        Constraint->SetAngularSwing2Limit(
            EAngularConstraintMotion::ACM_Limited,
            TerrainPitchLimitDegrees
        );
        Constraint->SetAngularTwistLimit(
            EAngularConstraintMotion::ACM_Limited,
            TwistLimitDegrees
        );
        // A velocity-only Swing drive behaves like a viscous joint. It slows
        // relative Pitch/Yaw motion but does not restore the chain to a
        // straight pose, so hills and curled body shapes remain possible.
        Constraint->SetAngularDriveMode(
            EAngularDriveMode::TwistAndSwing
        );
        Constraint->SetOrientationDriveTwistAndSwing(false, false);
        Constraint->SetAngularVelocityDriveTwistAndSwing(
            false,
            SwingVelocityDamping > 0.0f
        );
        Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
        Constraint->SetAngularDriveParams(
            0.0f,
            SwingVelocityDamping,
            SwingDampingForceLimit
        );
        Constraint->SetAngularDriveAccelerationMode(true);
        Constraint->SetDisableCollision(
            bDisableCollisionBetweenSegments
        );
        AddInstanceComponent(Constraint);
        Constraint->RegisterComponent();
        SegmentConstraints.Add(Constraint);

        UE_LOG(LogChimeraLineBody, Log,
            TEXT("[Constraint Ready] Segment pair %d-%d Front=%s Rear=%s Pitch=%.1f Horizontal=%.1f Twist=%.1f SwingDamping=%.1f ForceLimit=%.1f"),
            Index,
            Index + 1,
            *GetNameSafe(FrontBody),
            *GetNameSafe(RearBody),
            TerrainPitchLimitDegrees,
            HorizontalBendLimitDegrees,
            TwistLimitDegrees,
            SwingVelocityDamping,
            SwingDampingForceLimit);
    }
}

void ACMChimera::ConfigureBodyRotationLock(
    UBoxComponent* SegmentBody
)
{
    if (!SegmentBody)
    {
        return;
    }

    FBodyInstance& BodyInstance = SegmentBody->BodyInstance;
    // Segments are chained along local X: X is Roll, Y is Pitch, Z is Yaw.
    // Lock only Roll so the body can follow slopes and bend horizontally.
    BodyInstance.bLockXRotation = bLockBodyRoll;
    BodyInstance.bLockYRotation = false;
    BodyInstance.bLockZRotation = false;
    BodyInstance.SetDOFLock(EDOFMode::SixDOF);
}

void ACMChimera::ConfigureNetworkPhysics()
{
    if (HasAuthority())
    {
        UpdateReplicatedSegmentStates();
        return;
    }

    for (UPhysicsConstraintComponent* Constraint : SegmentConstraints)
    {
        if (Constraint)
        {
            Constraint->SetActive(false);
            Constraint->SetComponentTickEnabled(false);
        }
    }

    // 클라이언트는 루트를 포함한 모든 마디를 서버 상태로만 표시한다.
    // 로컬 Chaos 시뮬레이션을 남기면 첫 마디만 서버와 다른 자세가 된다.
    for (int32 Index = 0; Index < BodySegments.Num(); ++Index)
    {
        UBoxComponent* SegmentBody = BodySegments[Index];
        if (SegmentBody)
        {
            SegmentBody->SetSimulatePhysics(false);
            SegmentBody->SetCollisionEnabled(
                Index < ActiveSegmentCount
                    ? ECollisionEnabled::QueryOnly
                    : ECollisionEnabled::NoCollision
            );
        }
    }
}

void ACMChimera::UpdateReplicatedSegmentStates()
{
    const int32 ReplicatedSegmentCount = FMath::Max(
        0,
        FMath::Min(ActiveSegmentCount, BodySegments.Num())
    );
    ReplicatedSegmentStates.SetNum(ReplicatedSegmentCount);

    for (int32 StateIndex = 0;
        StateIndex < ReplicatedSegmentCount;
        ++StateIndex)
    {
        const UBoxComponent* SegmentBody =
            BodySegments[StateIndex];
        if (!SegmentBody)
        {
            continue;
        }

        ReplicatedSegmentStates[StateIndex].Location =
            SegmentBody->GetComponentLocation();
        ReplicatedSegmentStates[StateIndex].Rotation =
            SegmentBody->GetComponentRotation();
    }
}

void ACMChimera::ApplyReplicatedSegmentStates(float DeltaTime)
{
    if (!bHasReceivedSegmentStates)
    {
        return;
    }

    constexpr float SmoothingSpeed = 15.0f;
    constexpr float TeleportDistance = 500.0f;

    for (int32 StateIndex = 0;
        StateIndex < ReplicatedSegmentStates.Num();
        ++StateIndex)
    {
        const int32 SegmentIndex = StateIndex;
        if (!BodySegments.IsValidIndex(SegmentIndex)
            || !BodySegments[SegmentIndex])
        {
            continue;
        }

        UBoxComponent* SegmentBody = BodySegments[SegmentIndex];
        const FCMReplicatedSegmentState& TargetState =
            ReplicatedSegmentStates[StateIndex];
        const FVector TargetLocation = TargetState.Location;
        const FVector CurrentLocation = SegmentBody->GetComponentLocation();

        const bool bShouldTeleport = FVector::DistSquared(
            CurrentLocation,
            TargetLocation
        ) > FMath::Square(TeleportDistance);

        const FVector NewLocation = bShouldTeleport
            ? TargetLocation
            : FMath::VInterpTo(
                CurrentLocation,
                TargetLocation,
                DeltaTime,
                SmoothingSpeed
            );
        const FRotator NewRotation = bShouldTeleport
            ? TargetState.Rotation
            : FMath::RInterpTo(
                SegmentBody->GetComponentRotation(),
                TargetState.Rotation,
                DeltaTime,
                SmoothingSpeed
            );

        SegmentBody->SetWorldLocationAndRotation(
            NewLocation,
            NewRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
    }
}

void ACMChimera::OnRep_SegmentStates()
{
    bHasReceivedSegmentStates = true;
}

void ACMChimera::OnRep_ActiveSegmentCount()
{
    ConfigureSegments();
    ConfigureNetworkPhysics();
    RefreshTentacleSegments();
    OnSegmentStatesChanged.Broadcast();

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[Client Segment Count] ActiveSegments=%d PartSlots=%d"),
        ActiveSegmentCount,
        ActiveSegmentCount * CMControl::PartSlotsPerSegment);
}
