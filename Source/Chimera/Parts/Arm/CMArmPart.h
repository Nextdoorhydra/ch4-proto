#pragma once

#include "CoreMinimal.h"
#include "Parts/Combat/CMBattleComponent.h"
#include "Parts/Core/CMPartActorBase.h"

#include "CMArmPart.generated.h"

class UPrimitiveComponent;

UENUM(BlueprintType)
enum class ECMArmHoldType : uint8
{
    None,
    Ground,
    Interactable
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMArmSwingStateChangedSignature,
    bool,
    bSwinging
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FCMArmSwingHitSignature,
    ECMPartHitResult,
    Result,
    AActor*,
    TargetPart
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FCMArmAnchorStateChangedSignature,
    bool,
    bAnchored,
    FVector,
    AnchorLocation,
    FVector,
    AnchorNormal
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FCMArmSwingTargetDetectedSignature,
    AActor*,
    TargetActor,
    FVector,
    TargetLocation
);

/** Production Arm Part. One GA activation owns one complete swing. */
UCLASS(Blueprintable)
class CHIMERA_API ACMArmPart : public ACMPartActorBase
{
    GENERATED_BODY()

public:
    ACMArmPart();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual void OnDetachedFromPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot
    ) override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm")
    float GetStaminaCost() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm")
    float GetAnchorStaminaCostPerSecond() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm")
    virtual float GetSwingDuration() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm")
    float GetAttackRange() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm")
    float GetAttackRadius() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm")
    bool IsSwinging() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm")
    FGuid GetCurrentSwingAttackId() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm|Animation")
    bool IsGroundAnchored() const { return HoldType == ECMArmHoldType::Ground; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm|Animation")
    bool IsHolding() const { return HoldType != ECMArmHoldType::None; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm|Animation")
    ECMArmHoldType GetHoldType() const { return HoldType; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm|Animation")
    FVector GetGroundAnchorLocation() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm|Animation")
    FVector GetGroundAnchorNormal() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm|Hold")
    float GetHoldRange() const { return HoldRange; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm|Hold")
    float GetHoldRadius() const { return HoldRadius; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm|Animation")
    float GetSwingPhase() const;

    void BeginGroundAnchor(FVector Location, FVector Normal);
    void BeginInteractableHold(
        UPrimitiveComponent* TargetComponent,
        FVector Location,
        FVector Normal);
    void EndGroundAnchor();

    /** Called by the authoritative Arm GA at the start of one swing. */
    virtual bool BeginSwing();
    virtual void EndSwing();

    /**
     * Server animation trace/overlap bridge. Reusing CurrentSwingAttackId
     * prevents one swing from damaging the same target more than once.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Arm")
    ECMPartHitResult ResolveSwingHit(
        UCMBattleComponent* TargetBattleComponent,
        FVector ImpactPoint,
        FVector ImpactNormal
    );

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Arm")
    FCMArmSwingStateChangedSignature OnSwingStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Arm")
    FCMArmSwingHitSignature OnSwingHit;

    /** Server-only notification for actors found inside this Arm's swing sector. */
    UPROPERTY(BlueprintAssignable, Category = "Chimera|Arm")
    FCMArmSwingTargetDetectedSignature OnSwingTargetDetected;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Arm|Animation")
    FCMArmAnchorStateChangedSignature OnGroundAnchorStateChanged;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism")
    TObjectPtr<class UCMMechanismWeightComponent> MechanismWeightComponent;

    virtual void BeginPlay() override;
    virtual void ApplyPartData(
        const FCMPartLegArmTableRow& PartRow
    ) override;

    /** Provisional tuning values; concrete Arm Blueprints may override them. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Arm",
        meta = (ClampMin = "0.0"))
    float StaminaCost = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Arm",
        meta = (ClampMin = "0.0"))
    float AnchorStaminaCostPerSecond = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Arm",
        meta = (ClampMin = "0.01"))
    float SwingDuration = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Arm",
        meta = (ClampMin = "0.0"))
    float AttackRange = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Arm",
        meta = (ClampMin = "0.0"))
    float AttackRadius = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Arm|Hold",
        meta = (ClampMin = "0.0"))
    float HoldRange = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Arm|Hold",
        meta = (ClampMin = "0.0"))
    float HoldRadius = 40.0f;

    /** Extra forgiveness for characters that expose dismemberable body parts. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Arm",
        meta = (ClampMin = "0.0", ForceUnits = "cm"))
    float DismemberableTargetHitTolerance = 35.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Arm|Debug")
    bool bDrawSwingDebug = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Arm|Debug", meta = (ClampMin = "0.0"))
    float SwingDebugDuration = 1.0f;

private:
    void DetectSwingTargets();

    static bool IsInsideSwingSector(
        const FVector& Origin,
        const FVector& ForwardDirection,
        const FVector& TargetLocation,
        float Range,
        float Radius
    );

    UFUNCTION()
    void OnRep_Swinging();

    UFUNCTION()
    void OnRep_GroundAnchor();

    UFUNCTION()
    void HandlePartDied();

    UPROPERTY(ReplicatedUsing = OnRep_Swinging,
        VisibleInstanceOnly, BlueprintReadOnly, Category = "Chimera|Arm",
        meta = (AllowPrivateAccess = "true"))
    bool bSwinging = false;

    UPROPERTY(ReplicatedUsing = OnRep_GroundAnchor)
    ECMArmHoldType HoldType = ECMArmHoldType::None;

    UPROPERTY(ReplicatedUsing = OnRep_GroundAnchor)
    TObjectPtr<UPrimitiveComponent> HeldComponent;

    UPROPERTY(ReplicatedUsing = OnRep_GroundAnchor)
    FVector_NetQuantize10 GroundAnchorLocation = FVector::ZeroVector;

    UPROPERTY(ReplicatedUsing = OnRep_GroundAnchor)
    FVector_NetQuantizeNormal GroundAnchorNormal = FVector::UpVector;

    UPROPERTY(Replicated)
    float SwingStartTime = 0.0f;

    FGuid CurrentSwingAttackId;
    FTimerHandle SwingDetectionTimerHandle;
};
