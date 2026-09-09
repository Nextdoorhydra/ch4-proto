#include "Player/CMChimera.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

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
    if (!HasAuthority() || Acceleration.IsNearlyZero())
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
        || !BodySegments.IsValidIndex(SegmentIndex))
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
        SegmentBody->SetHiddenInGame(!bIsActive);
        SegmentBody->SetCollisionEnabled(
            bIsActive
                ? ECollisionEnabled::QueryAndPhysics
                : ECollisionEnabled::NoCollision
        );
        SegmentBody->SetMobility(EComponentMobility::Movable);
        SegmentBody->SetSimulatePhysics(bIsActive);
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
