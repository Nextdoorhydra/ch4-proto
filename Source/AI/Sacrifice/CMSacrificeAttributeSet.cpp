#include "Sacrifice/CMSacrificeAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

void UCMSacrificeAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(UCMSacrificeAttributeSet, FleeCharges, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UCMSacrificeAttributeSet, MaxFleeCharges, COND_None, REPNOTIFY_Always);
}

void UCMSacrificeAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetMaxFleeChargesAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
    else if (Attribute == GetFleeChargesAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxFleeCharges());
    }
}

void UCMSacrificeAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetFleeChargesAttribute() || Data.EvaluatedData.Attribute == GetMaxFleeChargesAttribute())
    {
        SetMaxFleeCharges(FMath::Max(0.0f, GetMaxFleeCharges()));
        SetFleeCharges(FMath::Clamp(GetFleeCharges(), 0.0f, GetMaxFleeCharges()));
    }
}

void UCMSacrificeAttributeSet::OnRep_FleeCharges(const FGameplayAttributeData& OldValue) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UCMSacrificeAttributeSet, FleeCharges, OldValue);
}

void UCMSacrificeAttributeSet::OnRep_MaxFleeCharges(const FGameplayAttributeData& OldValue) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UCMSacrificeAttributeSet, MaxFleeCharges, OldValue);
}
