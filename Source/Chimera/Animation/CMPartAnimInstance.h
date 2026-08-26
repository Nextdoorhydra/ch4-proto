#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Parts/Leg/CMLegPart.h"

#include "CMPartAnimInstance.generated.h"

class ACMPartActorBase;

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
}

/**
 * Shared runtime data source for procedural Part animation Blueprints.
 *
 * A derived Anim Blueprint can feed these component-space targets into IK and
 * use PhysicsBlendWeight with RigidBody or AnimDynamics nodes. The instance
 * deliberately owns no bone names so each Part skeleton remains free to map
 * its own shoulder, elbow, wrist, hip, knee, ankle, and neck chains.
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
    bool bArmSwinging = false;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Arm")
    FVector ArmHandTargetLocation = FVector::ZeroVector;

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
    FVector LegFootTargetLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Transient,
        Category = "Chimera|Part Animation|Leg")
    FVector LegGroundNormal = FVector::UpVector;

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
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HeadLookPhysicsWeight = 0.25f;

private:
    void ResetRuntimeState();
    FVector WorldLocationToComponent(FVector WorldLocation) const;
    FVector WorldDirectionToComponent(FVector WorldDirection) const;
};
