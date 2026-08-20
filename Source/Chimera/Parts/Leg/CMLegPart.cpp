#include "Parts/Leg/CMLegPart.h"

#include "Ability/CMLegGameplayAbility.h"
#include "Data/Part/CMPartLegArmTableRow.h"

ACMLegPart::ACMLegPart()
{
    PartType = ECMPartSlotType::Leg;
    GrantedAbilityClass = UCMLegGameplayAbility::StaticClass();
    Strength = 1.0f;
    MovementImpulseMultiplier = 1.0f;
}

float ACMLegPart::GetStaminaCost() const
{
    return StaminaCost;
}

float ACMLegPart::GetActionDuration() const
{
    return ActionDuration;
}

void ACMLegPart::ApplyPartData(const FCMPartLegArmTableRow& PartRow)
{
    StaminaCost = FMath::Max(PartRow.StaminaCost, 0.0f);
    ActionDuration = FMath::Max(PartRow.ActionDuration, 0.01f);
}
