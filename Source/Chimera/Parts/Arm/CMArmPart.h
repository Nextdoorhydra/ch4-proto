#pragma once

#include "CoreMinimal.h"
#include "Parts/Combat/CMBattleComponent.h"
#include "Parts/Core/CMPartActorBase.h"

#include "CMArmPart.generated.h"

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

protected:
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

private:
    UFUNCTION()
    void OnRep_Swinging();

    UFUNCTION()
    void HandlePartDied();

    UPROPERTY(ReplicatedUsing = OnRep_Swinging,
        VisibleInstanceOnly, BlueprintReadOnly, Category = "Chimera|Arm",
        meta = (AllowPrivateAccess = "true"))
    bool bSwinging = false;

    FGuid CurrentSwingAttackId;
};
