#include "Aggressive/Common/Core/CMAggressiveAIAttributeSet.h"

#include "Net/UnrealNetwork.h"

void UCMAggressiveAIAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UCMAggressiveAIAttributeSet, KnockbackDistance, COND_None, REPNOTIFY_Always);
}

void UCMAggressiveAIAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    if (Attribute == GetKnockbackDistanceAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
}

void UCMAggressiveAIAttributeSet::OnRep_KnockbackDistance(const FGameplayAttributeData& OldValue) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UCMAggressiveAIAttributeSet, KnockbackDistance, OldValue);
}
