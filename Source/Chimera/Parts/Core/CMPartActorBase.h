#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/CMPartInterface.h"

#include "CMPartActorBase.generated.h"

class UGameplayAbility;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Common base for replicated Parts that can be attached to a Chimera slot.
 *
 * Concrete Head, Arm, Leg, and Organ actors only need to configure PartType
 * and GrantedAbilityClass, then add their own behavior in derived classes.
 */
UCLASS(Abstract, Blueprintable)
class CHIMERA_API ACMPartActorBase
    : public AActor
    , public ICMPartInterface
{
    GENERATED_BODY()

public:
    ACMPartActorBase();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual ECMPartSlotType GetPartType_Implementation() const override;

    virtual TSubclassOf<UGameplayAbility>
        GetGrantedAbilityClass_Implementation() const override;

    virtual void OnAttachedToPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot
    ) override;

    virtual void OnDetachedFromPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot
    ) override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    UCMPartSlotComponent* GetAttachedPartSlot() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    FCMPartSlotAddress GetAttachedSlotAddress() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> PartMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Part")
    ECMPartSlotType PartType = ECMPartSlotType::Any;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Part")
    TSubclassOf<UGameplayAbility> GrantedAbilityClass;

private:
    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Part",
        meta = (AllowPrivateAccess = "true"))
    FCMPartSlotAddress AttachedSlotAddress;

    TWeakObjectPtr<UCMPartSlotComponent> AttachedPartSlot;
};
