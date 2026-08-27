#include "Animation/CMPartAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Arm/CMSpringArmPart.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Head/CMHeadPartActor.h"

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

        bArmGroundAnchored = ArmPart->IsHolding();
        bArmSwinging = ArmPart->IsSwinging();
        ArmHandTargetLocation = WorldLocationToComponent(
            ArmPart->GetGroundAnchorLocation());
        ArmGroundNormal = WorldDirectionToComponent(
            ArmPart->GetGroundAnchorNormal());
        ArmSwingPhase = ArmPart->GetSwingPhase();
        ArmSwingArcAlpha = CMPartAnimation::CalculateLooseMotionAlpha(
            ArmSwingPhase);
        ArmPhysicsBlendWeight = CMPartAnimation::CalculatePhysicsBlendWeight(
            bArmSwinging || bArmGroundAnchored,
            bArmSwinging ? ArmSwingPhysicsWeight : ArmHoldPhysicsWeight);

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
        LegStepDirection = LegPart->GetStepDirection();
        LegFootTargetLocation = WorldLocationToComponent(
            LegPart->GetStepGroundLocation());
        LegGroundNormal = WorldDirectionToComponent(
            LegPart->GetStepGroundNormal());
        LegStepPhase = LegPart->GetStepPhase();
        LegDirectionSign = LegStepDirection == ECMLegStepDirection::Forward
            ? 1.0f
            : LegStepDirection == ECMLegStepDirection::Reverse
                ? -1.0f
                : 0.0f;
        LegLiftAlpha = CMPartAnimation::CalculateLooseMotionAlpha(
            LegStepPhase);
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
        LegPhysicsBlendWeight = CMPartAnimation::CalculatePhysicsBlendWeight(
            LegStepDirection != ECMLegStepDirection::None,
            LegStepPhysicsWeight);
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
    bArmSwinging = false;
    ArmHandTargetLocation = FVector::ZeroVector;
    ArmGroundNormal = FVector::UpVector;
    ArmSwingPhase = 0.0f;
    ArmSwingArcAlpha = 0.0f;
    ArmPhysicsBlendWeight = 0.0f;
    ArmExtensionLength = 0.0f;
    ArmExtensionRatio = 0.0f;

    LegStepDirection = ECMLegStepDirection::None;
    LegFootTargetLocation = FVector::ZeroVector;
    LegGroundNormal = FVector::UpVector;
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
