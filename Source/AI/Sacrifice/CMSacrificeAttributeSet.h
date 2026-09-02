#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

#include "CMSacrificeAttributeSet.generated.h"

#define CM_SACRIFICE_ATTRIBUTE_ACCESSORS(ClassName, PropertyName)                                                                                                                                                                                                                                          \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)                                                                                                                                                                                                                                             \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                                                                                                                                                                                                                                                           \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                                                                                                                                                                                                                                                           \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** Only GAS-owned numeric resources used by the Sacrifice AI. */
UCLASS()
class AI_API UCMSacrificeAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FleeCharges, Category = "Chimera|Sacrifice|Attributes")
    FGameplayAttributeData FleeCharges;
    CM_SACRIFICE_ATTRIBUTE_ACCESSORS(UCMSacrificeAttributeSet, FleeCharges)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxFleeCharges, Category = "Chimera|Sacrifice|Attributes")
    FGameplayAttributeData MaxFleeCharges;
    CM_SACRIFICE_ATTRIBUTE_ACCESSORS(UCMSacrificeAttributeSet, MaxFleeCharges)

private:
    UFUNCTION()
    void OnRep_FleeCharges(const FGameplayAttributeData& OldValue) const;

    UFUNCTION()
    void OnRep_MaxFleeCharges(const FGameplayAttributeData& OldValue) const;
};

#undef CM_SACRIFICE_ATTRIBUTE_ACCESSORS
