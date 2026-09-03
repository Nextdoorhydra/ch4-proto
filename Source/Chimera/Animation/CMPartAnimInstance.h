#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Parts/Leg/CMLegPart.h"

#include "CMPartAnimInstance.generated.h"

class ACMPartActorBase;
class ACMArmPart;

namespace CMPartAnimation
{
    CHIMERA_API float CalculateLooseMotionAlpha(float Phase);
    CHIMERA_API float CalculatePhysicsBlendWeight(
        bool bActive,
        float ActiveWeight
    );
    CHIMERA_API float CalculateDelayedMotionPhase(
        float StepPhase,
        float DelayFraction
    );
    CHIMERA_API float CalculateJointMotionAlpha(float MotionPhase);

    /** Builds a complete slope frame without dropping the leg's heading. */
    CHIMERA_API bool BuildSlopeFrame(
        FVector GroundNormal,
        FVector DesiredForward,
        FVector& OutForward,
        FVector& OutRight,
        FVector& OutUp
    );

    /** Calculates a world-space contact target for swing, landing, or plant. */
    CHIMERA_API FVector CalculateSlopeFootTarget(
        FVector StartLocation,
        FVector TargetLocation,
        FVector StartNormal,
        FVector TargetNormal,
        float Phase,
        float StepHeight,
        float SoleContactOffset
    );

    /** Returns the interpolated walkable normal used for a swing pose. */
    CHIMERA_API FVector CalculateSlopeFootNormal(
        FVector StartNormal,
        FVector TargetNormal,
        float Phase
    );

    /** Clamps an effector to a measured chain reach without stretching. */
    CHIMERA_API FVector ClampFootTargetToReach(
        FVector HipLocation,
        FVector TargetLocation,
        float ChainLength,
        float MaxReachRatio = 0.95f
    );

    CHIMERA_API FQuat MakeSlopeFootRotation(
        FVector GroundNormal,
        FVector DesiredForward,
        FQuat Calibration = FQuat::Identity
    );

    /** Limits an IK effector rotation to a cone around its reference pose. */
    CHIMERA_API FQuat ClampRotationToReference(
        FQuat DesiredRotation,
        FQuat ReferenceRotation,
        float MaxAngleDegrees
    );

    /** Builds a complete swing that returns to the supplied relaxed target. */
    CHIMERA_API FVector CalculateArmSwingTarget(
        FVector StartLocation,
        FVector StrikeLocation,
        FVector RelaxedLocation,
        FVector ArcDirection,
        float Phase,
        float ArcHeight,
        float StrikePhase = 0.62f
    );

    /** Moves a loose Arm through captured -> back -> front -> relaxed. */
    CHIMERA_API FVector CalculateArmForwardBackSwingTarget(
        FVector StartLocation,
        FVector BackLocation,
        FVector FrontLocation,
        FVector RelaxedLocation,
        FVector ArcDirection,
        float Phase,
        float ArcHeight,
        float WindupPhase = 0.22f,
        float StrikePhase = 0.68f
    );

    /** Sweeps a hand around the shoulder on the horizontal top-view plane. */
    CHIMERA_API FVector CalculateArmPlanarFanTarget(
        FVector ShoulderLocation,
        FVector OutwardDirection,
        FVector ForwardDirection,
        float SweepPhase,
        float Radius,
        float PlaneHeight,
        float HalfAngleDegrees
    );
}

/**
 * Shared runtime data source for procedural Part animation Blueprints.
 *
 * A derived Anim Blueprint can feed these component-space targets into IK and
 * use PhysicsBlendWeight with RigidBody or AnimDynamics nodes. Arm runtime
 * state resolves the conventional upperarm/lowerarm/hand _l or _r chain from
 * the mounted slot, while the Anim Blueprint remains free to map the exposed
 * component-space targets to its Control Rig controls.
 */
UCLASS(Transient, Blueprintable)
class CHIMERA_API UCMPartAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation")
    TObjectPtr<ACMPartActorBase> PartActor;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation")
    bool bOperational = false;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    bool bArmGroundAnchored = false;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    bool bArmHolding = false;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    bool bArmSwinging = false;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    FVector ArmHandTargetLocation = FVector::ZeroVector;

    /** Component-space effector transform consumed by the arm Control Rig. */
    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    FTransform ArmHandTargetTransform = FTransform::Identity;

    /** Component-space elbow pole target consumed by the arm Control Rig. */
    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    FVector ArmElbowTargetLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    float ArmHandIKAlpha = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    FVector ArmGroundNormal = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    float ArmSwingPhase = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    float ArmSwingArcAlpha = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    float ArmPhysicsBlendWeight = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    float ArmExtensionLength = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    float ArmExtensionRatio = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    ECMLegStepDirection LegStepDirection = ECMLegStepDirection::None;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    ECMLegPlantState LegPlantState = ECMLegPlantState::Free;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    ECMLegPlantTrigger LegPlantTrigger = ECMLegPlantTrigger::None;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    bool bLegContactValid = false;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    FVector LegFootTargetLocation = FVector::ZeroVector;

    /** Component-space transform consumed by the leg Control Rig foot IK control. */
    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    FTransform LegFootTargetTransform = FTransform::Identity;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    FVector LegGroundNormal = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    FRotator LegFootTargetRotation = FRotator::ZeroRotator;

    /** Body-segment attachment transform in skeletal-mesh component space. */
    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    FTransform LegBodyAnchorTransform = FTransform::Identity;

    /** Body-driven reference-pose thigh transform consumed by the Control Rig hip control. */
    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    FTransform LegHipTargetTransform = FTransform::Identity;

    /** Component-space knee pole target consumed by the leg Control Rig. */
    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    FVector LegKneeTargetLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    float LegFootIKAlpha = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    float LegSideSign = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    float LegStepPhase = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    float LegDirectionSign = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    float LegLiftAlpha = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    float LegThighMotionAlpha = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    float LegCalfMotionAlpha = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    float LegFootMotionAlpha = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    float LegPhysicsBlendWeight = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Head")
    FRotator HeadLookRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Head")
    float HeadPhysicsBlendWeight = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ArmHoldPhysicsWeight = 0.35f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ArmSwingPhysicsWeight = 0.85f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Arm",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ArmIdlePhysicsWeight = 0.65f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Arm",
        meta = (ClampMin = "0.0"))
    float ArmIdleReachRatio = 0.82f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Arm",
        meta = (ClampMin = "0.0"))
    float ArmIdleOutwardRatio = 0.28f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Arm",
        meta = (ClampMin = "0.0"))
    float ArmIdleSpringStiffness = 52.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Arm",
        meta = (ClampMin = "0.0"))
    float ArmIdleSpringDamping = 7.5f;

    /** Seconds of planar body velocity used to trail the loose hand. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Arm",
        meta = (ClampMin = "0.0"))
    float ArmIdleMovementLagSeconds = 0.22f;

    /** Maximum movement-opposite hand offset as a fraction of chain length. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Arm",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ArmIdleMaximumLagRatio = 0.45f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Arm",
        meta = (ClampMin = "0.0"))
    float ArmSwingArcHeight = 32.0f;

    /** Half-angle of the horizontal fan visible from the test top camera. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Arm",
        meta = (ClampMin = "5.0", ClampMax = "170.0"))
    float ArmSwingHalfAngleDegrees = 70.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Arm",
        meta = (ClampMin = "0.1", ClampMax = "0.9"))
    float ArmSwingStrikePhase = 0.62f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Arm",
        meta = (ClampMin = "0.0"))
    float ArmElbowPoleOffset = 45.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LegStepPhysicsWeight = 0.55f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning",
        meta = (ClampMin = "0.0", ClampMax = "0.95"))
    float LegCalfPhaseDelay = 0.12f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning",
        meta = (ClampMin = "0.0", ClampMax = "0.95"))
    float LegFootPhaseDelay = 0.24f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning",
        meta = (ClampMin = "0.0"))
    float LegStepHeight = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning",
        meta = (ClampMin = "-50.0", ClampMax = "50.0"))
    float LegSoleContactOffset = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning",
        meta = (ClampMin = "0.0"))
    float LegKneePoleOffset = 120.0f;

    /** Maximum total foot rotation away from the reference-pose orientation. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning|Leg",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float LegMaxFootRotationDegrees = 40.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Animation|Tuning",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HeadLookPhysicsWeight = 0.25f;

private:
    void ResetRuntimeState();
    void ResetArmSimulation();
    void UpdateArmRuntimeState(const ACMArmPart& ArmPart, float DeltaSeconds);
    void UpdateLegRuntimeState(const ACMLegPart& LegPart);
    FVector WorldLocationToComponent(FVector WorldLocation) const;
    FVector WorldDirectionToComponent(FVector WorldDirection) const;

    bool bArmSimulationInitialized = false;
    bool bWasArmSwinging = false;
    FVector ArmSimulatedHandWorld = FVector::ZeroVector;
    FVector ArmSimulatedHandVelocity = FVector::ZeroVector;
    FVector ArmSwingStartWorld = FVector::ZeroVector;
};
