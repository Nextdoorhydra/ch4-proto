#include "Player/CMChimera.h"

#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

// 전체 환경 Force를 질량 비율로 나눠 모든 마디에 같은 가속도 적용
void ACMChimera::ApplyEnvironmentalForce(const FVector& TotalForce)
{
    if (!HasAuthority() || TotalForce.IsNearlyZero())
    {
        return;
    }

    const int32 SegmentCount = FMath::Min(ActiveSegmentCount, BodySegments.Num());
    float TotalMass = 0.0f;
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        const UStaticMeshComponent* SegmentBody = BodySegments[Index];
        if (SegmentBody && SegmentBody->IsSimulatingPhysics())
        {
            TotalMass += FMath::Max(SegmentBody->GetMass(), UE_SMALL_NUMBER);
        }
    }

    if (TotalMass <= UE_SMALL_NUMBER)
    {
        return;
    }

    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        UStaticMeshComponent* SegmentBody = BodySegments[Index];
        if (!SegmentBody || !SegmentBody->IsSimulatingPhysics())
        {
            continue;
        }

        const float MassFraction =
            FMath::Max(SegmentBody->GetMass(), UE_SMALL_NUMBER) / TotalMass;
        SegmentBody->AddForce(TotalForce * MassFraction);
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
        const UStaticMeshComponent* SegmentBody = BodySegments[Index];
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
        UStaticMeshComponent* SegmentBody = BodySegments[Index];
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

void ACMChimera::ConfigureSegments()
{
    ApplyBlueprintSettings();

    for (int32 Index = 0; Index < BodySegments.Num(); ++Index)
    {
        UStaticMeshComponent* SegmentBody = BodySegments[Index];
        const bool bIsActive = Index < ActiveSegmentCount;

        if (!SegmentBody)
        {
            continue;
        }

        if (Index > 0 && !SegmentBody->GetStaticMesh())
        {
            SegmentBody->SetStaticMesh(BodyMesh->GetStaticMesh());
        }

        SegmentBody->SetWorldScale3D(FVector(SegmentScale));
        SegmentBody->SetHiddenInGame(!bIsActive);
        SegmentBody->SetCollisionEnabled(
            bIsActive
                ? ECollisionEnabled::QueryAndPhysics
                : ECollisionEnabled::NoCollision
        );
        SegmentBody->SetMobility(EComponentMobility::Movable);
        SegmentBody->SetSimulatePhysics(bIsActive);
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
        UStaticMeshComponent* FrontBody = BodySegments[Index];
        UStaticMeshComponent* RearBody = BodySegments[Index + 1];

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
                * SegmentSpacing * SegmentScale;
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
    UStaticMeshComponent* SegmentBody
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

    for (int32 Index = 1; Index < BodySegments.Num(); ++Index)
    {
        UStaticMeshComponent* SegmentBody = BodySegments[Index];
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
        FMath::Min(ActiveSegmentCount, BodySegments.Num()) - 1
    );
    ReplicatedSegmentStates.SetNum(ReplicatedSegmentCount);

    for (int32 StateIndex = 0;
        StateIndex < ReplicatedSegmentCount;
        ++StateIndex)
    {
        const UStaticMeshComponent* SegmentBody =
            BodySegments[StateIndex + 1];
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
        const int32 SegmentIndex = StateIndex + 1;
        if (!BodySegments.IsValidIndex(SegmentIndex)
            || !BodySegments[SegmentIndex])
        {
            continue;
        }

        UStaticMeshComponent* SegmentBody = BodySegments[SegmentIndex];
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

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[Client Segment Count] ActiveSegments=%d PartSlots=%d"),
        ActiveSegmentCount,
        ActiveSegmentCount * CMControl::PartSlotsPerSegment);
}
