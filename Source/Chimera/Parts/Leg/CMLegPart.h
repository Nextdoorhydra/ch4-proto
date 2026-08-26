#pragma once

#include "CoreMinimal.h"
#include "Parts/Core/CMPartActorBase.h"

#include "CMLegPart.generated.h"

UENUM(BlueprintType)
enum class ECMLegStepDirection : uint8
{
    None,
    Forward,
    Reverse
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FCMLegStepStateChangedSignature,
    ECMLegStepDirection,
    Direction,
    FVector,
    GroundLocation,
    FVector,
    GroundNormal,
    float,
    Duration
);

/** First production Part that grants the shared Chimera its leg action GA. */
UCLASS(Blueprintable)
class CHIMERA_API ACMLegPart : public ACMPartActorBase
{
    GENERATED_BODY()

public:
    ACMLegPart();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual void OnDetachedFromPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot
    ) override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg")
    float GetStaminaCost() const;

    /** Time during which this specific Leg's GA rejects repeated input. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Leg")
    float GetActionDuration() const;

    /** Stores the direction sampled when Q/W/E/R starts this activation. */
    void SetPendingReverseMovement(bool bReverseMovement);

    /** Consumed once by the granted Leg ability when its Step begins. */
    bool ConsumePendingReverseMovement();

    void BeginProceduralStep(
        bool bReverseMovement,
        FVector GroundLocation,
        FVector GroundNormal,
        float Duration
    );
    void EndProceduralStep();

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    ECMLegStepDirection GetStepDirection() const { return StepDirection; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    FVector GetStepGroundLocation() const { return StepGroundLocation; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    FVector GetStepGroundNormal() const { return StepGroundNormal; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    float GetStepPhase() const;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Leg|Animation")
    FCMLegStepStateChangedSignature OnStepStateChanged;

protected:
    virtual void ApplyPartData(
        const FCMPartLegArmTableRow& PartRow
    ) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Leg",
        meta = (ClampMin = "0.0"))
    float StaminaCost = 10.0f;

    // This is gameplay timing, not animation timing. A later animation may
    // read the same value, but the server remains responsible for the lock.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Leg",
        meta = (ClampMin = "0.01"))
    float ActionDuration = 0.25f;

private:
    UFUNCTION()
    void OnRep_StepState();

    bool bPendingReverseMovement = false;

    UPROPERTY(ReplicatedUsing = OnRep_StepState)
    ECMLegStepDirection StepDirection = ECMLegStepDirection::None;

    UPROPERTY(ReplicatedUsing = OnRep_StepState)
    FVector_NetQuantize10 StepGroundLocation = FVector::ZeroVector;

    UPROPERTY(ReplicatedUsing = OnRep_StepState)
    FVector_NetQuantizeNormal StepGroundNormal = FVector::UpVector;

    UPROPERTY(Replicated)
    float StepStartTime = 0.0f;

    UPROPERTY(Replicated)
    float StepDuration = 0.0f;
};
