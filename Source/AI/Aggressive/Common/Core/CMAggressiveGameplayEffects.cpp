#include "Aggressive/Common/Core/CMAggressiveGameplayEffects.h"

#include "Aggressive/Common/Core/CMAggressiveAIAttributeSet.h"

const FName UCMAggressiveInitializeGameplayEffect::KnockbackDistanceDataName(TEXT("Data.KnockbackDistance"));

UCMAggressiveInitializeGameplayEffect::UCMAggressiveInitializeGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FSetByCallerFloat SetByCaller;
    SetByCaller.DataName = KnockbackDistanceDataName;

    FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = UCMAggressiveAIAttributeSet::GetKnockbackDistanceAttribute();
    Modifier.ModifierOp = EGameplayModOp::Override;
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}
