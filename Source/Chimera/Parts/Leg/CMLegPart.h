#pragma once

#include "CoreMinimal.h"
#include "Parts/Core/CMPartActorBase.h"

#include "CMLegPart.generated.h"

class UCMBloodTransferComponent;

UENUM(BlueprintType)
enum class ECMLegStepDirection : uint8
{
    None,
    Forward,
    Reverse
};

UENUM(BlueprintType)
enum class ECMLegPlantState : uint8
{
    Free,
    Swing,
    Landing,
    Planted,
    Recover
};

UENUM(BlueprintType)
enum class ECMLegPlantTrigger : uint8
{
    None,
    Initialization,
    PlayerInput,
    ReachRecovery,
    Emergency
};

USTRUCT(BlueprintType)
struct FCMLegPlantSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ECMLegPlantState State = ECMLegPlantState::Free;

    UPROPERTY(BlueprintReadOnly)
    ECMLegPlantTrigger Trigger = ECMLegPlantTrigger::None;

    /** Non-None only while this snapshot owns gameplay movement force. */
    UPROPERTY(BlueprintReadOnly)
    ECMLegStepDirection StepDirection = ECMLegStepDirection::None;

    UPROPERTY(BlueprintReadOnly)
    bool bContactValid = false;

    UPROPERTY(BlueprintReadOnly)
    FVector_NetQuantize10 StartGroundLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector_NetQuantizeNormal StartGroundNormal = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly)
    FVector_NetQuantize10 GroundLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector_NetQuantizeNormal GroundNormal = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly)
    float ServerStartTime = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float Duration = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 Sequence = 0;
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
        float Duration,
        ECMLegPlantTrigger Trigger = ECMLegPlantTrigger::PlayerInput
    );

    /** Starts a visual-only reach recovery; it must never apply body force. */
    void BeginVisualReplant(
        FVector GroundLocation,
        FVector GroundNormal,
        float Duration
    );

    /** Stores the first authoritative contact without playing a swing. */
    void InitializePlantedContact(
        FVector GroundLocation,
        FVector GroundNormal
    );

    /** Completes a valid gameplay or visual transition into Planted. */
    void EndProceduralStep();

    /** Cancels a transition without applying force or retaining its target. */
    void CancelProceduralStep(
        ECMLegPlantTrigger Trigger = ECMLegPlantTrigger::Emergency
    );

    /** Advances short Recover transitions on the authority. */
    void AdvancePlantState();

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    ECMLegStepDirection GetStepDirection() const
    {
        return PlantSnapshot.StepDirection;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    ECMLegPlantState GetPlantState() const { return PlantSnapshot.State; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    ECMLegPlantTrigger GetPlantTrigger() const { return PlantSnapshot.Trigger; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    bool HasValidGroundContact() const { return PlantSnapshot.bContactValid; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    FVector GetStepStartGroundLocation() const
    {
        return PlantSnapshot.StartGroundLocation;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    FVector GetStepStartGroundNormal() const
    {
        return PlantSnapshot.StartGroundNormal;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    int32 GetPlantSequence() const { return PlantSnapshot.Sequence; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    float GetSideSign() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    FVector GetStepGroundLocation() const
    {
        return PlantSnapshot.GroundLocation;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    FVector GetStepGroundNormal() const
    {
        return PlantSnapshot.GroundNormal;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg|Animation")
    float GetStepPhase() const;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Leg|Animation")
    FCMLegStepStateChangedSignature OnStepStateChanged;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Gore")
    TObjectPtr<UCMBloodTransferComponent> BloodTransferComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism")
    TObjectPtr<class UCMMechanismWeightComponent> MechanismWeightComponent;

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
    /** Kept as a compatibility hook for older Blueprint subclasses. */
    UFUNCTION()
    void OnRep_StepState();

    UFUNCTION()
    void OnRep_PlantSnapshot();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayFootstep(FVector_NetQuantize10 GroundLocation);

    void BeginPlantTransition(
        ECMLegStepDirection NewStepDirection,
        ECMLegPlantTrigger Trigger,
        FVector GroundLocation,
        FVector GroundNormal,
        float Duration
    );

    void BroadcastPlantState();

    FVector GetCurrentPlantStartLocation() const;

    bool bPendingReverseMovement = false;

    // Legacy fields remain as transient compatibility mirrors for existing
    // native/Blueprint callers. Authority state is replicated atomically via
    // PlantSnapshot below.
    UPROPERTY(Transient)
    ECMLegStepDirection StepDirection = ECMLegStepDirection::None;

    UPROPERTY(Transient)
    FVector_NetQuantize10 StepGroundLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    FVector_NetQuantizeNormal StepGroundNormal = FVector::UpVector;

    /** Client-side guard against a delayed transition snapshot. */
    UPROPERTY(Transient)
    int32 LastAppliedPlantSequence = INDEX_NONE;

    UPROPERTY(ReplicatedUsing = OnRep_PlantSnapshot)
    FCMLegPlantSnapshot PlantSnapshot;
};
