#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayAbilitySpecHandle.h"

#include "CMControlTypes.h"
#include "CMPartSlotComponent.generated.h"

class AActor;
class UAbilitySystemComponent;
class UCMPartSlotComponent;

/** Parts may later use this value to validate which physical slots accept them. */
UENUM(BlueprintType)
enum class ECMPartSlotType : uint8
{
    Any,
    Head,
    Arm,
    Leg,
    Organ
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FCMPartSlotAttachmentChanged,
    UCMPartSlotComponent*,
    PartSlot,
    AActor*,
    AttachedPart
);

/**
 * One editor-placeable attachment point on a Chimera body segment.
 *
 * Its transform is authored directly in the BP_CMChimera viewport. Runtime
 * code reads the stable segment/local-slot address but does not overwrite the
 * authored transform. Marker components are children of this component, so
 * they automatically follow whenever the slot is moved.
 */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMPartSlotComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UCMPartSlotComponent();

    void InitializeSlotAddress(int32 InSegmentIndex, int32 InPartSlotIndex);

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Slot")
    FCMPartSlotAddress GetSlotAddress() const;

    /** Server-only attachment. PartActor must implement ICMPartInterface. */
    bool AttachPart(AActor* PartActor);

    /** Server-only detachment. Returns the Part that was detached. */
    AActor* DetachPart();

    /** Called by the authoritative Chimera when this slot's control key fires. */
    bool TryActivateGrantedAbility();

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Slot")
    AActor* GetAttachedPart() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Slot")
    bool HasAttachedPart() const;

    /** Optional editor-authored thigh attachment point for a mounted leg rig. */
    void SetLegRigControlAnchor(USceneComponent* InControlAnchor);

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Slot|Control Rig")
    USceneComponent* GetLegRigControlAnchor() const;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Part Slot")
    FCMPartSlotAttachmentChanged OnAttachedPartChanged;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Slot")
    int32 SegmentIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Slot")
    int32 PartSlotIndex = INDEX_NONE;

    // Kept as Any for the prototype. Part attachment can enforce this later.
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Slot")
    ECMPartSlotType AllowedPartType = ECMPartSlotType::Any;

protected:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

private:
    UFUNCTION()
    void OnRep_AttachedPart(AActor* PreviousPart);

    UFUNCTION()
    void HandleAttachedPartDestroyed(AActor* DestroyedPart);

    UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
    void RemoveGrantedAbility();

    UPROPERTY(ReplicatedUsing = OnRep_AttachedPart,
        VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Part Slot",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<AActor> AttachedPart;

    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> LegRigControlAnchor;

    FGameplayAbilitySpecHandle GrantedAbilityHandle;
};
