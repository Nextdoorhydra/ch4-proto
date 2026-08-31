#include "Sacrifice/CMSacrificeGameplayEffects.h"

#include "Sacrifice/CMSacrificeAttributeSet.h"

const FName UCMSacrificeInitializeGameplayEffect::MaxFleeChargesDataName(TEXT("Data.MaxFleeCharges"));

UCMSacrificeInitializeGameplayEffect::UCMSacrificeInitializeGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FSetByCallerFloat SetByCaller;
    SetByCaller.DataName = MaxFleeChargesDataName;

    FGameplayModifierInfo& MaxModifier = Modifiers.AddDefaulted_GetRef();
    MaxModifier.Attribute = UCMSacrificeAttributeSet::GetMaxFleeChargesAttribute();
    MaxModifier.ModifierOp = EGameplayModOp::Override;
    MaxModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

    FGameplayModifierInfo& CurrentModifier = Modifiers.AddDefaulted_GetRef();
    CurrentModifier.Attribute = UCMSacrificeAttributeSet::GetFleeChargesAttribute();
    CurrentModifier.ModifierOp = EGameplayModOp::Override;
    CurrentModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}

UCMSacrificeConsumeFleeGameplayEffect::UCMSacrificeConsumeFleeGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = UCMSacrificeAttributeSet::GetFleeChargesAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(-1.0f);
}

UCMSacrificeRefillFleeGameplayEffect::UCMSacrificeRefillFleeGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FAttributeBasedFloat Magnitude;
    Magnitude.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(UCMSacrificeAttributeSet::GetMaxFleeChargesAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);

    Magnitude.AttributeCalculationType = EAttributeBasedFloatCalculationType::AttributeMagnitude;

    FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = UCMSacrificeAttributeSet::GetFleeChargesAttribute();
    Modifier.ModifierOp = EGameplayModOp::Override;
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
}
