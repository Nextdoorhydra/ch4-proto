#include "Parts/Leg/CMLegPart.h"

#include "Ability/CMLegGameplayAbility.h"
#include "Data/Part/CMPartLegArmTableRow.h"

ACMLegPart::ACMLegPart()
{
    PartType = ECMPartSlotType::Leg;
    GrantedAbilityClass = UCMLegGameplayAbility::StaticClass();
    PartRowName = TEXT("DefaultLeg");
}

float ACMLegPart::GetStaminaCost() const
{
    return StaminaCost;
}

float ACMLegPart::GetActionDuration() const
{
    return ActionDuration;
}

void ACMLegPart::SetPendingReverseMovement(bool bReverseMovement)
{
    bPendingReverseMovement = bReverseMovement;
}

bool ACMLegPart::ConsumePendingReverseMovement()
{
    const bool bWasReverse = bPendingReverseMovement;
    bPendingReverseMovement = false;
    return bWasReverse;
}

void ACMLegPart::ApplyPartData(const FCMPartLegArmTableRow& PartRow)
{
    StaminaCost = FMath::Max(PartRow.StaminaCost, 0.0f);
    ActionDuration = FMath::Max(PartRow.ActionDuration, 0.01f);
}
