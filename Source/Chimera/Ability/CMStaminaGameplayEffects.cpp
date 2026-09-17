#include "Ability/CMStaminaGameplayEffects.h"

#include "Ability/CMChimeraAttributeSet.h"

const FName UCMStaminaCostGameplayEffect::StaminaCostDataName(
    TEXT("Data.StaminaCost")
);

UCMStaminaCostGameplayEffect::UCMStaminaCostGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FSetByCallerFloat SetByCallerCost;
    SetByCallerCost.DataName = StaminaCostDataName;

    FGameplayModifierInfo& StaminaModifier =
        Modifiers.AddDefaulted_GetRef();
    StaminaModifier.Attribute =
        UCMChimeraAttributeSet::GetStaminaAttribute();
    StaminaModifier.ModifierOp = EGameplayModOp::Additive;
    StaminaModifier.ModifierMagnitude =
        FGameplayEffectModifierMagnitude(SetByCallerCost);
}

UCMStaminaRegenGameplayEffect::UCMStaminaRegenGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    Period = FScalableFloat(1.0f);

    // Waiting one full period avoids granting a free extra regeneration tick
    // at spawn time, when Stamina has already been initialized to MaxStamina.
    bExecutePeriodicEffectOnApplication = false;

    FAttributeBasedFloat RegenMagnitude;
    RegenMagnitude.BackingAttribute =
        FGameplayEffectAttributeCaptureDefinition(
            UCMChimeraAttributeSet::GetStaminaRegenAttribute(),
            EGameplayEffectAttributeCaptureSource::Target,
            false
        );
    RegenMagnitude.AttributeCalculationType =
        EAttributeBasedFloatCalculationType::AttributeMagnitude;

    FGameplayModifierInfo& StaminaModifier =
        Modifiers.AddDefaulted_GetRef();
    StaminaModifier.Attribute =
        UCMChimeraAttributeSet::GetStaminaAttribute();
    StaminaModifier.ModifierOp = EGameplayModOp::Additive;
    StaminaModifier.ModifierMagnitude =
        FGameplayEffectModifierMagnitude(RegenMagnitude);
}
