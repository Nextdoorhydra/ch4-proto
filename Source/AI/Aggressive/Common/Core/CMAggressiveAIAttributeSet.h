#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

#include "CMAggressiveAIAttributeSet.generated.h"

#define CM_AGGRESSIVE_ATTRIBUTE_ACCESSORS(ClassName, PropertyName)                                                                                                                                                                                                                                         \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)                                                                                                                                                                                                                                             \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                                                                                                                                                                                                                                                           \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                                                                                                                                                                                                                                                           \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** Aggressive AIs have no health; GAS owns only reaction tuning data. */
UCLASS()
class AI_API UCMAggressiveAIAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_KnockbackDistance, Category = "Aggressive AI|Attributes")
    FGameplayAttributeData KnockbackDistance;
    CM_AGGRESSIVE_ATTRIBUTE_ACCESSORS(UCMAggressiveAIAttributeSet, KnockbackDistance)

private:
    UFUNCTION()
    void OnRep_KnockbackDistance(const FGameplayAttributeData& OldValue) const;
};

#undef CM_AGGRESSIVE_ATTRIBUTE_ACCESSORS
