#pragma once

#include "CoreMinimal.h"
#include "Parts/Arm/CMArmPart.h"
#include "Stage/Trigger/CMGrabPullTarget.h"

#include "CMSpringArmPart.generated.h"

class ACMSpringArmHookProjectile;
class UArrowComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FCMSpringArmHookResolvedSignature,
    AActor*,
    HitActor,
    bool,
    bPulledTarget
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FCMSpringArmFinishedSignature
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FCMSpringArmPullResultSignature, AActor*, HitActor, ECMGrabPullResult, Result);

/** Arm variant that fires a hook and resolves one server-authoritative pull. */
UCLASS(Blueprintable)
class CHIMERA_API ACMSpringArmPart : public ACMArmPart
{
    GENERATED_BODY()

public:
    ACMSpringArmPart();

    virtual bool BeginSwing() override;
    virtual void EndSwing() override;
    virtual float GetSwingDuration() const override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Spring Arm")
    float GetExtensionSpeed() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Spring Arm")
    float GetPullImpulse() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Spring Arm|Animation")
    float GetCurrentExtensionLength() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Spring Arm|Animation")
    float GetExtensionRatio() const;

    /** Current synchronized yaw inside [-SweepHalfAngle, +SweepHalfAngle]. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Spring Arm|Aim")
    float GetCurrentSweepAngle() const;

    /** Direction used by both the visual arm and the authoritative hook shot. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Spring Arm|Aim")
    FVector GetCurrentAimDirection() const;

    /** Called only by the authoritative hook projectile. */
    void ResolveHookHit(const FHitResult& Hit);
    
    /** Stops this pull because another SpringArm became the active pull owner. */
    void CancelBodyPullFromOverride();

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Spring Arm")
    FCMSpringArmHookResolvedSignature OnHookResolved;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Spring Arm")
    FCMSpringArmPullResultSignature OnPullTargetResolved;
    
    UPROPERTY(BlueprintAssignable, Category = "Chimera|Spring Arm")
    FCMSpringArmFinishedSignature OnSpringArmFinished;

protected:
    virtual void ApplyPartData(
        const FCMPartLegArmTableRow& PartRow
    ) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Spring Arm", meta = (ClampMin = "0.0"))
    float ExtensionSpeed = 1200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Spring Arm", meta = (ClampMin = "0.0"))
    float PullImpulse = 5000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Spring Arm|Aim",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float SweepHalfAngle = 25.0f;

    // Angular frequency in radians per second. 1.5 gives a ~4.19 s cycle.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Spring Arm|Aim", meta = (ClampMin = "0.0"))
    float SweepSpeed = 1.5f;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
    Category = "Chimera|Spring Arm|Aim")
    TObjectPtr<UArrowComponent> SweepDirectionArrow;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Spring Arm")
    TSubclassOf<ACMSpringArmHookProjectile> HookProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Spring Arm|Pull",
        meta = (ClampMin = "0.01"))
    float PullUpdateInterval = 0.05f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Spring Arm|Pull",
        meta = (ClampMin = "0.0"))
    float PullStopDistance = 100.0f;

private:
    bool LaunchHook();
    float GetAutomaticSweepPhase() const;
    void StartBodyPull(
        class ACMChimera* Chimera,
        const FCMPartSlotAddress& SlotAddress,
        const FVector& AnchorLocation
    );
    void UpdateBodyPull();
    void StopBodyPull();

    UFUNCTION()
    void HandleHookDestroyed(AActor* DestroyedActor);

    UPROPERTY(Replicated)
    TObjectPtr<ACMSpringArmHookProjectile> ActiveHook;

    TWeakObjectPtr<class ACMChimera> PullTargetChimera;
    FCMPartSlotAddress PullTargetSlot;
    FVector PullAnchorLocation = FVector::ZeroVector;
    FTimerHandle PullTimerHandle;
};
