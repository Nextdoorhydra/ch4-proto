#include "Animation/CMPartAnimInstance.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Arm/CMSpringArmPart.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Player/CMPartSlotComponent.h"

namespace
{
const FName LegThighBone(TEXT("thigh_l"));
const FName LegCalfBone(TEXT("calf_l"));
const FName LegFootBone(TEXT("foot_l"));
const FName ArmDefaultUpperBone(TEXT("upperarm_l"));
const FName ArmDefaultLowerBone(TEXT("lowerarm_l"));
const FName ArmDefaultHandBone(TEXT("hand_l"));

bool TryGetReferenceComponentTransform(
    const USkeletalMeshComponent& Mesh,
    const FName BoneName,
    FTransform& OutTransform
)
{
    const USkeletalMesh* SkeletalMesh = Mesh.GetSkeletalMeshAsset();
    if (!SkeletalMesh)
    {
        return false;
    }

    const FReferenceSkeleton& ReferenceSkeleton =
        SkeletalMesh->GetRefSkeleton();
    int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);
    const TArray<FTransform>& ReferencePose =
        ReferenceSkeleton.GetRefBonePose();
    if (!ReferencePose.IsValidIndex(BoneIndex))
    {
        return false;
    }

    OutTransform = ReferencePose[BoneIndex];
    BoneIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
    while (ReferencePose.IsValidIndex(BoneIndex))
    {
        OutTransform *= ReferencePose[BoneIndex];
        BoneIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
    }
    return true;
}



}

float CMPartAnimation::CalculateLooseMotionAlpha(const float Phase)
{
    const float ClampedPhase = FMath::Clamp(Phase, 0.0f, 1.0f);
    if (ClampedPhase <= 0.0f || ClampedPhase >= 1.0f)
    {
        return 0.0f;
    }
    return FMath::Sin(PI * ClampedPhase);
}

float CMPartAnimation::CalculatePhysicsBlendWeight(
    const bool bActive,
    const float ActiveWeight
)
{
    return bActive ? FMath::Clamp(ActiveWeight, 0.0f, 1.0f) : 0.0f;
}

float CMPartAnimation::CalculateDelayedMotionPhase(
    const float StepPhase,
    const float DelayFraction
)
{
    const float Phase = FMath::Clamp(StepPhase, 0.0f, 1.0f);
    const float Delay = FMath::Clamp(DelayFraction, 0.0f, 0.95f);
    return Phase <= Delay
        ? 0.0f
        : FMath::Clamp((Phase - Delay) / (1.0f - Delay), 0.0f, 1.0f);
}

float CMPartAnimation::CalculateJointMotionAlpha(const float MotionPhase)
{
    const float Phase = FMath::Clamp(MotionPhase, 0.0f, 1.0f);
    if (Phase <= 0.0f || Phase >= 1.0f)
    {
        return 0.0f;
    }
    return FMath::Sin(2.0f * PI * Phase);
}

bool CMPartAnimation::BuildSlopeFrame(
    FVector GroundNormal,
    FVector DesiredForward,
    FVector& OutForward,
    FVector& OutRight,
    FVector& OutUp
)
{
    OutUp = GroundNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    if (OutUp.IsNearlyZero() || OutUp.Z <= 0.0f)
    {
        OutForward = FVector::ForwardVector;
        OutRight = FVector::RightVector;
        OutUp = FVector::UpVector;
        return false;
    }

    OutForward = FVector::VectorPlaneProject(DesiredForward, OutUp)
        .GetSafeNormal();
    if (OutForward.IsNearlyZero())
    {
        OutForward = FVector::VectorPlaneProject(
            FVector::ForwardVector,
            OutUp).GetSafeNormal();
    }
    if (OutForward.IsNearlyZero())
    {
        OutForward = FVector::VectorPlaneProject(
            FVector::RightVector,
            OutUp).GetSafeNormal();
    }
    if (OutForward.IsNearlyZero())
    {
        OutForward = FVector::ForwardVector;
        OutRight = FVector::RightVector;
        OutUp = FVector::UpVector;
        return false;
    }

    OutRight = FVector::CrossProduct(OutUp, OutForward).GetSafeNormal();
    OutForward = FVector::CrossProduct(OutRight, OutUp).GetSafeNormal();
    return !OutRight.IsNearlyZero() && !OutForward.IsNearlyZero();
}

FVector CMPartAnimation::CalculateSlopeFootTarget(
    const FVector StartLocation,
    const FVector TargetLocation,
    const FVector StartNormal,
    const FVector TargetNormal,
    const float Phase,
    const float StepHeight,
    const float SoleContactOffset
)
{
    const float ClampedPhase = FMath::Clamp(Phase, 0.0f, 1.0f);
    const float SmoothPhase = ClampedPhase * ClampedPhase
        * (3.0f - 2.0f * ClampedPhase);
    const FVector BlendedNormal = CalculateSlopeFootNormal(
        StartNormal,
        TargetNormal,
        ClampedPhase);
    const FVector BaseLocation = FMath::Lerp(
        StartLocation,
        TargetLocation,
        SmoothPhase);
    const float LiftAlpha = FMath::Sin(PI * ClampedPhase);
    return BaseLocation
        + BlendedNormal * (FMath::Max(StepHeight, 0.0f) * LiftAlpha
            + SoleContactOffset);
}

FVector CMPartAnimation::CalculateSlopeFootNormal(
    const FVector StartNormal,
    const FVector TargetNormal,
    const float Phase
)
{
    const float ClampedPhase = FMath::Clamp(Phase, 0.0f, 1.0f);
    const float SmoothPhase = ClampedPhase * ClampedPhase
        * (3.0f - 2.0f * ClampedPhase);
    const FVector SafeStartNormal = StartNormal.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    const FVector SafeTargetNormal = TargetNormal.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    return FMath::Lerp(
        SafeStartNormal,
        SafeTargetNormal,
        SmoothPhase).GetSafeNormal(
            SMALL_NUMBER,
            FVector::UpVector);
}

FVector CMPartAnimation::ClampFootTargetToReach(
    const FVector HipLocation,
    const FVector TargetLocation,
    const float ChainLength,
    const float MaxReachRatio
)
{
    const float MaxReach = FMath::Max(ChainLength, 0.0f)
        * FMath::Clamp(MaxReachRatio, 0.0f, 1.0f);
    if (MaxReach <= UE_SMALL_NUMBER)
    {
        return TargetLocation;
    }

    const FVector ToTarget = TargetLocation - HipLocation;
    if (ToTarget.SizeSquared() <= FMath::Square(MaxReach))
    {
        return TargetLocation;
    }
    return HipLocation + ToTarget.GetSafeNormal() * MaxReach;
}

FQuat CMPartAnimation::MakeSlopeFootRotation(
    const FVector GroundNormal,
    const FVector DesiredForward,
    const FQuat Calibration
)
{
    FVector Forward;
    FVector Right;
    FVector Up;
    BuildSlopeFrame(
        GroundNormal,
        DesiredForward,
        Forward,
        Right,
        Up);
    return (FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat()
        * Calibration).GetNormalized();
}

FQuat CMPartAnimation::ClampRotationToReference(
    FQuat DesiredRotation,
    FQuat ReferenceRotation,
    const float MaxAngleDegrees
)
{
    DesiredRotation.Normalize();
    ReferenceRotation.Normalize();
    const float MaxAngleRadians = FMath::DegreesToRadians(
        FMath::Clamp(MaxAngleDegrees, 0.0f, 180.0f));
    const float AngularDistance = ReferenceRotation.AngularDistance(
        DesiredRotation);
    if (AngularDistance <= MaxAngleRadians
        || AngularDistance <= UE_SMALL_NUMBER)
    {
        return DesiredRotation;
    }
    return FQuat::Slerp(
        ReferenceRotation,
        DesiredRotation,
        MaxAngleRadians / AngularDistance).GetNormalized();
}

FVector CMPartAnimation::CalculateArmSwingTarget(
    const FVector StartLocation,
    const FVector StrikeLocation,
    const FVector RelaxedLocation,
    FVector ArcDirection,
    const float Phase,
    const float ArcHeight,
    const float StrikePhase
)
{
    const float ClampedPhase = FMath::Clamp(Phase, 0.0f, 1.0f);
    const float ClampedStrikePhase = FMath::Clamp(
        StrikePhase,
        0.1f,
        0.9f);
    const bool bApproachingStrike = ClampedPhase <= ClampedStrikePhase;
    const float SegmentPhase = bApproachingStrike
        ? ClampedPhase / ClampedStrikePhase
        : (ClampedPhase - ClampedStrikePhase)
            / (1.0f - ClampedStrikePhase);
    const float SmoothPhase = SegmentPhase * SegmentPhase
        * (3.0f - 2.0f * SegmentPhase);
    const FVector BaseLocation = bApproachingStrike
        ? FMath::Lerp(StartLocation, StrikeLocation, SmoothPhase)
        : FMath::Lerp(StrikeLocation, RelaxedLocation, SmoothPhase);
    ArcDirection = ArcDirection.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    const float ArcAlpha = FMath::Sin(PI * SegmentPhase);
    return BaseLocation
        + ArcDirection * (FMath::Max(ArcHeight, 0.0f) * ArcAlpha);
}

FVector CMPartAnimation::CalculateArmForwardBackSwingTarget(
    const FVector StartLocation,
    const FVector BackLocation,
    const FVector FrontLocation,
    const FVector RelaxedLocation,
    FVector ArcDirection,
    const float Phase,
    const float ArcHeight,
    const float WindupPhase,
    const float StrikePhase
)
{
    const float SafeWindupPhase = FMath::Clamp(WindupPhase, 0.05f, 0.45f);
    const float SafeStrikePhase = FMath::Clamp(
        StrikePhase,
        SafeWindupPhase + 0.1f,
        0.9f);
    const float ClampedPhase = FMath::Clamp(Phase, 0.0f, 1.0f);

    FVector SegmentStart;
    FVector SegmentEnd;
    float SegmentPhase = 0.0f;
    float SegmentArcScale = 1.0f;
    if (ClampedPhase <= SafeWindupPhase)
    {
        SegmentStart = StartLocation;
        SegmentEnd = BackLocation;
        SegmentPhase = ClampedPhase / SafeWindupPhase;
        SegmentArcScale = 0.35f;
    }
    else if (ClampedPhase <= SafeStrikePhase)
    {
        SegmentStart = BackLocation;
        SegmentEnd = FrontLocation;
        SegmentPhase = (ClampedPhase - SafeWindupPhase)
            / (SafeStrikePhase - SafeWindupPhase);
    }
    else
    {
        SegmentStart = FrontLocation;
        SegmentEnd = RelaxedLocation;
        SegmentPhase = (ClampedPhase - SafeStrikePhase)
            / (1.0f - SafeStrikePhase);
        SegmentArcScale = 0.35f;
    }

    const float SmoothPhase = SegmentPhase * SegmentPhase
        * (3.0f - 2.0f * SegmentPhase);
    ArcDirection = ArcDirection.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    return FMath::Lerp(SegmentStart, SegmentEnd, SmoothPhase)
        + ArcDirection
            * (FMath::Max(ArcHeight, 0.0f)
                * SegmentArcScale
                * FMath::Sin(PI * SegmentPhase));
}

FVector CMPartAnimation::CalculateArmPlanarFanTarget(
    const FVector ShoulderLocation,
    FVector OutwardDirection,
    FVector ForwardDirection,
    const float SweepPhase,
    const float Radius,
    const float PlaneHeight,
    const float HalfAngleDegrees
)
{
    OutwardDirection = FVector::VectorPlaneProject(
        OutwardDirection,
        FVector::UpVector).GetSafeNormal(
            SMALL_NUMBER,
            FVector::RightVector);
    ForwardDirection = FVector::VectorPlaneProject(
        ForwardDirection,
        FVector::UpVector);
    ForwardDirection = (ForwardDirection
        - OutwardDirection * FVector::DotProduct(
            ForwardDirection,
            OutwardDirection)).GetSafeNormal(
                SMALL_NUMBER,
                FVector::ForwardVector);

    const float SmoothPhase = FMath::SmoothStep(
        0.0f,
        1.0f,
        FMath::Clamp(SweepPhase, 0.0f, 1.0f));
    const float HalfAngleRadians = FMath::DegreesToRadians(
        FMath::Clamp(HalfAngleDegrees, 0.0f, 179.0f));
    const float Angle = FMath::Lerp(
        -HalfAngleRadians,
        HalfAngleRadians,
        SmoothPhase);
    const FVector RadialDirection =
        OutwardDirection * FMath::Cos(Angle)
        + ForwardDirection * FMath::Sin(Angle);
    return ShoulderLocation
        + RadialDirection * FMath::Max(Radius, 0.0f)
        + FVector::UpVector * PlaneHeight;
}

void UCMPartAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    PartActor = Cast<ACMPartActorBase>(GetOwningActor());
    ResetRuntimeState();
}

void UCMPartAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!IsValid(PartActor))
    {
        PartActor = Cast<ACMPartActorBase>(GetOwningActor());
    }
    ResetRuntimeState();
    if (!PartActor)
    {
        return;
    }

    bOperational = PartActor->IsOperational();

    if (const ACMArmPart* ArmPart = Cast<ACMArmPart>(PartActor))
    {
        UpdateArmRuntimeState(*ArmPart, DeltaSeconds);

        if (const ACMSpringArmPart* SpringArm =
            Cast<ACMSpringArmPart>(ArmPart))
        {
            ArmExtensionLength = SpringArm->GetCurrentExtensionLength();
            ArmExtensionRatio = SpringArm->GetExtensionRatio();
        }
        return;
    }

    if (const ACMLegPart* LegPart = Cast<ACMLegPart>(PartActor))
    {
        UpdateLegRuntimeState(*LegPart);
        return;
    }

    if (const ACMHeadPartActor* HeadPart =
        Cast<ACMHeadPartActor>(PartActor))
    {
        HeadLookRotation = HeadPart->GetProceduralLookRotation();
        HeadPhysicsBlendWeight = CMPartAnimation::CalculatePhysicsBlendWeight(
            !HeadLookRotation.IsNearlyZero(),
            HeadLookPhysicsWeight);
    }
}

void UCMPartAnimInstance::ResetRuntimeState()
{
    bOperational = false;

    bArmGroundAnchored = false;
    bArmHolding = false;
    bArmSwinging = false;
    ArmHandTargetLocation = FVector::ZeroVector;
    ArmHandTargetTransform = FTransform::Identity;
    ArmElbowTargetLocation = FVector::ZeroVector;
    ArmHandIKAlpha = 0.0f;
    ArmGroundNormal = FVector::UpVector;
    ArmSwingPhase = 0.0f;
    ArmSwingArcAlpha = 0.0f;
    ArmPhysicsBlendWeight = 0.0f;
    ArmExtensionLength = 0.0f;
    ArmExtensionRatio = 0.0f;

    LegStepDirection = ECMLegStepDirection::None;
    LegPlantState = ECMLegPlantState::Free;
    LegPlantTrigger = ECMLegPlantTrigger::None;
    bLegContactValid = false;
    LegFootTargetLocation = FVector::ZeroVector;
    LegFootTargetTransform = FTransform::Identity;
    LegGroundNormal = FVector::UpVector;
    LegFootTargetRotation = FRotator::ZeroRotator;
    LegBodyAnchorTransform = FTransform::Identity;
    LegHipTargetTransform = FTransform::Identity;
    LegKneeTargetLocation = FVector::ZeroVector;
    LegFootIKAlpha = 0.0f;
    LegSideSign = 0.0f;
    LegStepPhase = 0.0f;
    LegDirectionSign = 0.0f;
    LegLiftAlpha = 0.0f;
    LegThighMotionAlpha = 0.0f;
    LegCalfMotionAlpha = 0.0f;
    LegFootMotionAlpha = 0.0f;
    LegPhysicsBlendWeight = 0.0f;

    HeadLookRotation = FRotator::ZeroRotator;
    HeadPhysicsBlendWeight = 0.0f;
}

void UCMPartAnimInstance::ResetArmSimulation()
{
    bArmSimulationInitialized = false;
    bWasArmSwinging = false;
    ArmSimulatedHandWorld = FVector::ZeroVector;
    ArmSimulatedHandVelocity = FVector::ZeroVector;
    ArmSwingStartWorld = FVector::ZeroVector;
}

void UCMPartAnimInstance::UpdateArmRuntimeState(
    const ACMArmPart& ArmPart,
    const float DeltaSeconds
)
{
    const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
    if (!Mesh)
    {
        ResetArmSimulation();
        return;
    }

    UCMPartSlotComponent* PartSlot = ArmPart.GetAttachedPartSlot();
    FName UpperBone = ArmDefaultUpperBone;
    FName LowerBone = ArmDefaultLowerBone;
    FName HandBone = ArmDefaultHandBone;
    bool bUsesMirroredLeftChain = false;
    if (PartSlot
        && !PartSlot->ResolveArmReferenceBoneNames(
            *Mesh,
            UpperBone,
            LowerBone,
            HandBone,
            bUsesMirroredLeftChain))
    {
        ResetArmSimulation();
        return;
    }

    FTransform ReferenceShoulder = FTransform::Identity;
    FTransform ReferenceElbow = FTransform::Identity;
    FTransform ReferenceHand = FTransform::Identity;
    const bool bHasReferenceChain = TryGetReferenceComponentTransform(
            *Mesh,
            UpperBone,
            ReferenceShoulder)
        && TryGetReferenceComponentTransform(
            *Mesh,
            LowerBone,
            ReferenceElbow)
        && TryGetReferenceComponentTransform(
            *Mesh,
            HandBone,
            ReferenceHand);
    if (!bHasReferenceChain)
    {
        ResetArmSimulation();
        return;
    }

    const FTransform& MeshWorld = Mesh->GetComponentTransform();
    const FVector ShoulderWorld = MeshWorld.TransformPosition(
        ReferenceShoulder.GetLocation());
    const FVector ElbowWorld = MeshWorld.TransformPosition(
        ReferenceElbow.GetLocation());
    const FVector ReferenceHandWorld = MeshWorld.TransformPosition(
        ReferenceHand.GetLocation());
    const float ChainLength = FVector::Distance(
            ShoulderWorld,
            ElbowWorld)
        + FVector::Distance(ElbowWorld, ReferenceHandWorld);
    if (ChainLength <= UE_SMALL_NUMBER)
    {
        ResetArmSimulation();
        return;
    }

    USceneComponent* BodySegment = PartSlot
        ? PartSlot->GetAttachParent()
        : nullptr;
    const USceneComponent* ArmMountAnchor = PartSlot
        ? PartSlot->GetArmRigControlAnchor()
        : nullptr;
    FVector OutwardDirection = Mesh->GetRightVector();
    if (PartSlot && BodySegment)
    {
        const FVector MountLocation = ArmMountAnchor
            ? ArmMountAnchor->GetComponentLocation()
            : PartSlot->GetComponentLocation();
        OutwardDirection = FVector::VectorPlaneProject(
            MountLocation - BodySegment->GetComponentLocation(),
            FVector::UpVector).GetSafeNormal();
    }
    if (OutwardDirection.IsNearlyZero())
    {
        OutwardDirection = Mesh->GetRightVector().GetSafeNormal(
            SMALL_NUMBER,
            FVector::RightVector);
    }
    FVector ForwardDirection = BodySegment
        ? BodySegment->GetForwardVector()
        : Mesh->GetForwardVector();
    ForwardDirection = FVector::VectorPlaneProject(
        ForwardDirection,
        FVector::UpVector).GetSafeNormal(
            SMALL_NUMBER,
            FVector::ForwardVector);

    const FVector RelaxedDirection = (
        FVector::UpVector
        + OutwardDirection * ArmIdleOutwardRatio).GetSafeNormal();
    FVector PlanarBodyVelocity = FVector::ZeroVector;
    if (UPrimitiveComponent* BodyPrimitive =
            Cast<UPrimitiveComponent>(BodySegment))
    {
        PlanarBodyVelocity = BodyPrimitive->GetPhysicsLinearVelocityAtPoint(
            ShoulderWorld);
        PlanarBodyVelocity.Z = 0.0f;
    }
    const FVector MovementLagOffset =
        (-PlanarBodyVelocity * FMath::Max(ArmIdleMovementLagSeconds, 0.0f))
        .GetClampedToMaxSize(
            ChainLength * FMath::Clamp(
                ArmIdleMaximumLagRatio,
                0.0f,
                1.0f));
    const FVector RelaxedHandWorld =
        CMPartAnimation::ClampFootTargetToReach(
            ShoulderWorld,
            ShoulderWorld + RelaxedDirection
                * (ChainLength * ArmIdleReachRatio)
                + MovementLagOffset,
            ChainLength,
            0.94f);

    if (!bArmSimulationInitialized
        || FVector::DistSquared(
            ArmSimulatedHandWorld,
            ShoulderWorld) > FMath::Square(ChainLength * 2.0f))
    {
        bArmSimulationInitialized = true;
        ArmSimulatedHandWorld = RelaxedHandWorld;
        ArmSimulatedHandVelocity = FVector::ZeroVector;
        ArmSwingStartWorld = RelaxedHandWorld;
    }

    bArmGroundAnchored = ArmPart.IsGroundAnchored();
    bArmHolding = ArmPart.IsHolding();
    bArmSwinging = ArmPart.IsSwinging();
    ArmSwingPhase = ArmPart.GetSwingPhase();
    ArmSwingArcAlpha = CMPartAnimation::CalculateLooseMotionAlpha(
        ArmSwingPhase);

    FVector TargetHandWorld = RelaxedHandWorld;
    FVector PoseNormal = FVector::UpVector;
    if (bArmHolding)
    {
        PoseNormal = ArmPart.GetGroundAnchorNormal().GetSafeNormal(
            SMALL_NUMBER,
            FVector::UpVector);
        TargetHandWorld = CMPartAnimation::ClampFootTargetToReach(
            ShoulderWorld,
            ArmPart.GetGroundAnchorLocation(),
            ChainLength,
            0.96f);

        // A hold must visually agree with the gameplay constraint on the
        // first evaluated frame, so it deliberately bypasses idle smoothing.
        ArmSimulatedHandWorld = TargetHandWorld;
        ArmSimulatedHandVelocity = FVector::ZeroVector;
    }
    else if (bArmSwinging)
    {
        if (!bWasArmSwinging)
        {
            ArmSwingStartWorld = ArmSimulatedHandWorld;
            ArmSimulatedHandVelocity = FVector::ZeroVector;
        }
        constexpr float WindupPhase = 0.22f;
        const float StrikePhase = FMath::Clamp(
            ArmSwingStrikePhase,
            WindupPhase + 0.1f,
            0.9f);
        const FVector BackWorld =
            CMPartAnimation::CalculateArmPlanarFanTarget(
                ShoulderWorld,
                OutwardDirection,
                ForwardDirection,
                0.0f,
                ChainLength * 0.9f,
                ArmSwingArcHeight,
                ArmSwingHalfAngleDegrees);
        const FVector FrontWorld =
            CMPartAnimation::CalculateArmPlanarFanTarget(
                ShoulderWorld,
                OutwardDirection,
                ForwardDirection,
                1.0f,
                ChainLength * 0.9f,
                ArmSwingArcHeight,
                ArmSwingHalfAngleDegrees);
        if (ArmSwingPhase <= WindupPhase)
        {
            const float WindupAlpha = FMath::SmoothStep(
                0.0f,
                1.0f,
                ArmSwingPhase / WindupPhase);
            TargetHandWorld = FMath::Lerp(
                ArmSwingStartWorld,
                BackWorld,
                WindupAlpha);
        }
        else if (ArmSwingPhase <= StrikePhase)
        {
            TargetHandWorld =
                CMPartAnimation::CalculateArmPlanarFanTarget(
                    ShoulderWorld,
                    OutwardDirection,
                    ForwardDirection,
                    (ArmSwingPhase - WindupPhase)
                        / (StrikePhase - WindupPhase),
                    ChainLength * 0.9f,
                    ArmSwingArcHeight,
                    ArmSwingHalfAngleDegrees);
        }
        else
        {
            const float ReturnAlpha = FMath::SmoothStep(
                0.0f,
                1.0f,
                (ArmSwingPhase - StrikePhase)
                    / (1.0f - StrikePhase));
            TargetHandWorld = FMath::Lerp(
                FrontWorld,
                RelaxedHandWorld,
                ReturnAlpha);
        }
        TargetHandWorld = CMPartAnimation::ClampFootTargetToReach(
            ShoulderWorld,
            TargetHandWorld,
            ChainLength,
            0.96f);
        ArmSimulatedHandWorld = TargetHandWorld;
    }
    else
    {
        const float StepSeconds = FMath::Clamp(
            DeltaSeconds,
            0.0f,
            1.0f / 30.0f);
        if (StepSeconds > 0.0f)
        {
            const FVector SpringAcceleration =
                (RelaxedHandWorld - ArmSimulatedHandWorld)
                    * ArmIdleSpringStiffness
                - ArmSimulatedHandVelocity * ArmIdleSpringDamping;
            ArmSimulatedHandVelocity += SpringAcceleration * StepSeconds;
            ArmSimulatedHandVelocity = ArmSimulatedHandVelocity
                .GetClampedToMaxSize(ChainLength * 7.0f);
            ArmSimulatedHandWorld +=
                ArmSimulatedHandVelocity * StepSeconds;
        }
        TargetHandWorld = CMPartAnimation::ClampFootTargetToReach(
            ShoulderWorld,
            ArmSimulatedHandWorld,
            ChainLength,
            0.96f);
        ArmSimulatedHandWorld = TargetHandWorld;
    }

    bWasArmSwinging = bArmSwinging;
    ArmHandTargetLocation = WorldLocationToComponent(TargetHandWorld);
    ArmGroundNormal = WorldDirectionToComponent(PoseNormal);
    ArmHandTargetTransform = FTransform(
        ReferenceHand.GetRotation(),
        ArmHandTargetLocation,
        FVector::OneVector);

    const FVector PoleWorld = (ShoulderWorld + TargetHandWorld) * 0.5f
        + ForwardDirection * FMath::Max(
            ArmElbowPoleOffset,
            ChainLength * 0.35f)
        + OutwardDirection * (ChainLength * 0.08f);
    ArmElbowTargetLocation = WorldLocationToComponent(PoleWorld);
    ArmHandIKAlpha = 1.0f;
    ArmPhysicsBlendWeight = CMPartAnimation::CalculatePhysicsBlendWeight(
        true,
        bArmHolding
            ? ArmHoldPhysicsWeight
            : bArmSwinging
                ? ArmSwingPhysicsWeight
                : ArmIdlePhysicsWeight);
}

void UCMPartAnimInstance::UpdateLegRuntimeState(
    const ACMLegPart& LegPart
)
{
    LegStepDirection = LegPart.GetStepDirection();
    LegPlantState = LegPart.GetPlantState();
    LegPlantTrigger = LegPart.GetPlantTrigger();
    bLegContactValid = LegPart.HasValidGroundContact();
    LegStepPhase = LegPart.GetStepPhase();
    LegSideSign = LegPart.GetSideSign();
    LegDirectionSign = LegStepDirection == ECMLegStepDirection::Forward
        ? 1.0f
        : LegStepDirection == ECMLegStepDirection::Reverse
            ? -1.0f
            : 0.0f;

    const FVector StartLocation = LegPart.GetStepStartGroundLocation();
    const FVector TargetLocation = LegPart.GetStepGroundLocation();
    const FVector StartNormal = LegPart.GetStepStartGroundNormal();
    const FVector TargetNormal = LegPart.GetStepGroundNormal();
    const bool bTransitioning = LegPlantState == ECMLegPlantState::Swing
        || LegPlantState == ECMLegPlantState::Landing;
    const bool bRecovering = LegPlantState == ECMLegPlantState::Recover;
    const float TargetPhase = bTransitioning ? LegStepPhase : 1.0f;
    const UCMPartSlotComponent* PartSlot =
        LegPart.GetAttachedPartSlot();
    const USceneComponent* RigControlAnchor = PartSlot
        ? PartSlot->GetLegRigControlAnchor()
        : nullptr;
    const UWorld* AnimWorld = LegPart.GetWorld();
    const bool bEditorRigPreview = RigControlAnchor
        && AnimWorld
        && !AnimWorld->IsGameWorld();

    FVector KneeWorld = LegPart.GetActorLocation();
    FVector HipWorld = KneeWorld;
    FVector FootWorld = KneeWorld;
    float UpperLength = 0.0f;
    float LowerLength = 0.0f;
    const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
    if (Mesh)
    {
        FTransform ReferenceHip = FTransform::Identity;
        FTransform ReferenceKnee = FTransform::Identity;
        FTransform ReferenceFoot = FTransform::Identity;
        const bool bHasReferenceChain =
            TryGetReferenceComponentTransform(
                *Mesh,
                LegThighBone,
                ReferenceHip)
            && TryGetReferenceComponentTransform(
                *Mesh,
                LegCalfBone,
                ReferenceKnee)
            && TryGetReferenceComponentTransform(
                *Mesh,
                LegFootBone,
                ReferenceFoot);
        if (bHasReferenceChain)
        {
            const FTransform& MeshWorld = Mesh->GetComponentTransform();
            LegHipTargetTransform = ReferenceHip;
            HipWorld = MeshWorld.TransformPosition(
                ReferenceHip.GetLocation());
            KneeWorld = MeshWorld.TransformPosition(
                ReferenceKnee.GetLocation());
            FootWorld = MeshWorld.TransformPosition(
                ReferenceFoot.GetLocation());
            UpperLength = FVector::Distance(HipWorld, KneeWorld);
            LowerLength = FVector::Distance(KneeWorld, FootWorld);
        }
        else
        {
            HipWorld = Mesh->GetBoneLocation(
                LegThighBone,
                EBoneSpaces::WorldSpace);
            KneeWorld = Mesh->GetBoneLocation(
                LegCalfBone,
                EBoneSpaces::WorldSpace);
            FootWorld = Mesh->GetBoneLocation(
                LegFootBone,
                EBoneSpaces::WorldSpace);

            const int32 HipBoneIndex = Mesh->GetBoneIndex(LegThighBone);
            if (HipBoneIndex != INDEX_NONE)
            {
                const FTransform HipWorldTransform = Mesh->GetBoneTransform(
                    HipBoneIndex,
                    Mesh->GetComponentTransform());
                LegHipTargetTransform =
                    HipWorldTransform.GetRelativeTransform(
                        Mesh->GetComponentTransform());
            }
        }

        if (PartSlot)
        {
            if (const USceneComponent* BodyAnchor =
                PartSlot->GetAttachParent())
            {
                LegBodyAnchorTransform =
                    BodyAnchor->GetComponentTransform().GetRelativeTransform(
                        Mesh->GetComponentTransform());
            }
        }
    }
    if (UpperLength <= UE_SMALL_NUMBER || LowerLength <= UE_SMALL_NUMBER)
    {
        UpperLength = FVector::Distance(HipWorld, KneeWorld);
        LowerLength = FVector::Distance(KneeWorld, FootWorld);
    }
    const float ChainLength = UpperLength + LowerLength;
    const FVector EffectiveTargetLocation = bEditorRigPreview
        && !bLegContactValid
        ? FootWorld
        : TargetLocation;
    const FVector ClampedTargetLocation =
        CMPartAnimation::ClampFootTargetToReach(
            HipWorld,
            EffectiveTargetLocation,
            ChainLength,
            0.95f);

    FVector TargetLocationWorld = EffectiveTargetLocation;
    if (bTransitioning || LegPlantState == ECMLegPlantState::Planted)
    {
        TargetLocationWorld = CMPartAnimation::CalculateSlopeFootTarget(
            StartLocation,
            ClampedTargetLocation,
            StartNormal,
            TargetNormal,
            TargetPhase,
            bTransitioning ? LegStepHeight : 0.0f,
            LegSoleContactOffset);
    }
    else if (bRecovering)
    {
        TargetLocationWorld = CMPartAnimation::CalculateSlopeFootTarget(
            StartLocation,
            ClampedTargetLocation,
            StartNormal,
            TargetNormal,
            1.0f,
            0.0f,
            LegSoleContactOffset);
    }

    LegFootTargetLocation = WorldLocationToComponent(TargetLocationWorld);
    const FVector PoseGroundNormal =
        CMPartAnimation::CalculateSlopeFootNormal(
            StartNormal,
            TargetNormal,
            TargetPhase);
    LegGroundNormal = WorldDirectionToComponent(PoseGroundNormal);

    FVector DesiredForward = FVector::ForwardVector;
    if (PartSlot)
    {
        DesiredForward = PartSlot->GetForwardVector();
    }
    FQuat ReferenceFootRotation = FQuat::Identity;
    if (Mesh)
    {
        FTransform ReferenceFoot = FTransform::Identity;
        if (TryGetReferenceComponentTransform(
                *Mesh,
                LegFootBone,
                ReferenceFoot))
        {
            ReferenceFootRotation = ReferenceFoot.GetRotation();
        }
    }
    const FQuat FlatFrameWorld = CMPartAnimation::MakeSlopeFootRotation(
        FVector::UpVector,
        DesiredForward);
    const FQuat ReferenceFootWorld = Mesh
        ? Mesh->GetComponentTransform().TransformRotation(
            ReferenceFootRotation)
        : ReferenceFootRotation;
    const FQuat FootCalibration =
        FlatFrameWorld.Inverse() * ReferenceFootWorld;
    FQuat FootRotationWorld = CMPartAnimation::MakeSlopeFootRotation(
        PoseGroundNormal,
        DesiredForward,
        FootCalibration);
    FootRotationWorld = CMPartAnimation::ClampRotationToReference(
        FootRotationWorld,
        ReferenceFootWorld,
        LegMaxFootRotationDegrees);
    LegFootTargetRotation = Mesh
        ? Mesh->GetComponentTransform().InverseTransformRotation(
            FootRotationWorld).Rotator()
        : FootRotationWorld.Rotator();

    LegFootTargetTransform = FTransform(
        LegFootTargetRotation.Quaternion(),
        LegFootTargetLocation,
        FVector::OneVector);

    const float PoleOffset = FMath::Max(
        LegKneePoleOffset,
        (UpperLength + LowerLength) * 0.25f);
    FVector BodySideDirection =
        (Mesh ? Mesh->GetRightVector() : FVector::RightVector)
        * LegSideSign;
    if (PartSlot)
    {
        if (const USceneComponent* BodySegment =
                PartSlot->GetAttachParent())
        {
            BodySideDirection = FVector::VectorPlaneProject(
                PartSlot->GetComponentLocation()
                    - BodySegment->GetComponentLocation(),
                BodySegment->GetUpVector()).GetSafeNormal();
        }
    }
    if (BodySideDirection.IsNearlyZero())
    {
        BodySideDirection = (Mesh
            ? Mesh->GetRightVector()
            : FVector::RightVector) * LegSideSign;
    }
    const FVector PoleWorld = KneeWorld
        + BodySideDirection * PoleOffset;
    LegKneeTargetLocation = WorldLocationToComponent(PoleWorld);

    LegLiftAlpha = bTransitioning
        ? CMPartAnimation::CalculateLooseMotionAlpha(LegStepPhase)
        : 0.0f;
    LegThighMotionAlpha = LegDirectionSign
        * CMPartAnimation::CalculateJointMotionAlpha(LegStepPhase);
    LegCalfMotionAlpha = LegDirectionSign
        * CMPartAnimation::CalculateJointMotionAlpha(
            CMPartAnimation::CalculateDelayedMotionPhase(
                LegStepPhase,
                LegCalfPhaseDelay));
    LegFootMotionAlpha = LegDirectionSign
        * CMPartAnimation::CalculateJointMotionAlpha(
            CMPartAnimation::CalculateDelayedMotionPhase(
                LegStepPhase,
                LegFootPhaseDelay));
    LegFootIKAlpha = bEditorRigPreview
        ? 1.0f
        : bTransitioning || LegPlantState
        == ECMLegPlantState::Planted
        ? 1.0f
        : bRecovering
            ? 1.0f - LegStepPhase
            : 0.0f;
    LegPhysicsBlendWeight = CMPartAnimation::CalculatePhysicsBlendWeight(
        LegStepDirection != ECMLegStepDirection::None
            && LegPlantTrigger != ECMLegPlantTrigger::ReachRecovery,
        LegStepPhysicsWeight);
}

FVector UCMPartAnimInstance::WorldLocationToComponent(
    const FVector WorldLocation
) const
{
    const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
    return Mesh
        ? Mesh->GetComponentTransform().InverseTransformPosition(WorldLocation)
        : WorldLocation;
}

FVector UCMPartAnimInstance::WorldDirectionToComponent(
    const FVector WorldDirection
) const
{
    const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
    const FVector Result = Mesh
        ? Mesh->GetComponentTransform().InverseTransformVectorNoScale(
            WorldDirection)
        : WorldDirection;
    return Result.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
}
