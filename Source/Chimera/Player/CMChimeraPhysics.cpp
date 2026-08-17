#include "Player/CMChimera.h"

#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

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
        Constraint->SetAngularSwing1Limit(
            EAngularConstraintMotion::ACM_Limited,
            SwingLimitDegrees
        );
        Constraint->SetAngularSwing2Limit(
            EAngularConstraintMotion::ACM_Limited,
            SwingLimitDegrees
        );
        Constraint->SetAngularTwistLimit(
            EAngularConstraintMotion::ACM_Limited,
            TwistLimitDegrees
        );
        Constraint->SetDisableCollision(
            bDisableCollisionBetweenSegments
        );
        AddInstanceComponent(Constraint);
        Constraint->RegisterComponent();
        SegmentConstraints.Add(Constraint);

        UE_LOG(LogChimeraLineBody, Log,
            TEXT("[Constraint Ready] Segment pair %d-%d Front=%s Rear=%s"),
            Index,
            Index + 1,
            *GetNameSafe(FrontBody),
            *GetNameSafe(RearBody));
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
    BodyInstance.bLockXRotation = bLockBodyUpright;
    BodyInstance.bLockYRotation = bLockBodyUpright;
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
