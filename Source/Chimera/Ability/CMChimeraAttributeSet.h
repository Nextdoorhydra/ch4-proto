#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "CMChimeraAttributeSet.generated.h"

#define CM_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class CHIMERA_API UCMChimeraAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    /*
     * 이 AttributeSet은 특정 플레이어의 능력치가 아니라 한 마리의
     * Shared Chimera가 공용으로 사용하는 상태만 보관한다.
     * 마디별 체력은 각 마디가 독립적으로 죽을 수 있으므로 ACMChimera의
     * SegmentHealthStates에서 별도로 관리한다.
     */
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    // GAS를 통해 값이 바뀔 때 스태미나가 유효 범위를 벗어나지 않게 한다.
    virtual void PreAttributeChange(
        const FGameplayAttribute& Attribute,
        float& NewValue
    ) override;

    // Instant and periodic GameplayEffects finish here. Clamp their final
    // results as a second safety net after the modifier has been evaluated.
    virtual void PostGameplayEffectExecute(
        const FGameplayEffectModCallbackData& Data
    ) override;

    // 모든 조작자가 함께 소비하는 공용 행동 자원이다.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina,
        Category = "Chimera|Attributes")
    FGameplayAttributeData Stamina;
    CM_ATTRIBUTE_ACCESSORS(UCMChimeraAttributeSet, Stamina)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina,
        Category = "Chimera|Attributes")
    FGameplayAttributeData MaxStamina;
    CM_ATTRIBUTE_ACCESSORS(UCMChimeraAttributeSet, MaxStamina)

    // 서버가 초당 회복시킬 스태미나 양이다.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaRegen,
        Category = "Chimera|Attributes")
    FGameplayAttributeData StaminaRegen;
    CM_ATTRIBUTE_ACCESSORS(UCMChimeraAttributeSet, StaminaRegen)

private:
    UFUNCTION()
    void OnRep_Stamina(const FGameplayAttributeData& OldValue) const;

    UFUNCTION()
    void OnRep_MaxStamina(const FGameplayAttributeData& OldValue) const;

    UFUNCTION()
    void OnRep_StaminaRegen(const FGameplayAttributeData& OldValue) const;
};

#undef CM_ATTRIBUTE_ACCESSORS
