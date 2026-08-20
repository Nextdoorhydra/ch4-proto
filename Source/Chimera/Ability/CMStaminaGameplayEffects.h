#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "CMStaminaGameplayEffects.generated.h"

/**
 * Instant GAS effect used by attached Part Gameplay Abilities.
 * Each GA supplies its Part's negative cost through SetByCaller, so the same
 * effect base class supports data-driven Arm and Leg costs.
 */
UCLASS(Blueprintable)
class CHIMERA_API UCMStaminaCostGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCMStaminaCostGameplayEffect();

    static const FName StaminaCostDataName;
};

/**
 * Infinite periodic GAS effect that adds the target's StaminaRegen attribute
 * to Stamina once per second. It replaces per-frame manual regeneration.
 */
UCLASS()
class CHIMERA_API UCMStaminaRegenGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCMStaminaRegenGameplayEffect();
};
