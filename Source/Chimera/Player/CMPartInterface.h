#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Player/CMPartSlotComponent.h"
#include "CMPartInterface.generated.h"

class UGameplayAbility;

/** Implement this on a replicated Part Actor or Part Blueprint. */
UINTERFACE(BlueprintType)
class CHIMERA_API UCMPartInterface : public UInterface
{
    GENERATED_BODY()
};

class CHIMERA_API ICMPartInterface
{
    GENERATED_BODY()

public:
    /** The physical part kind used only when a slot is not set to Any. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Part")
    ECMPartSlotType GetPartType() const;
    virtual ECMPartSlotType GetPartType_Implementation() const
    {
        return ECMPartSlotType::Any;
    }

    /** The ability granted to the shared Chimera ASC while this Part is attached. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Part")
    TSubclassOf<UGameplayAbility> GetGrantedAbilityClass() const;
    virtual TSubclassOf<UGameplayAbility>
        GetGrantedAbilityClass_Implementation() const
    {
        return nullptr;
    }

    /** Server-side notification after the Part is attached and its GA is granted. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Part")
    void OnAttachedToPartSlot(UCMPartSlotComponent* PartSlot);
    virtual void OnAttachedToPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot)
    {
    }

    /** Server-side notification before the Part is detached. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Part")
    void OnDetachedFromPartSlot(UCMPartSlotComponent* PartSlot);
    virtual void OnDetachedFromPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot)
    {
    }
};
