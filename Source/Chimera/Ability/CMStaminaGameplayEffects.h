#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "CMStaminaGameplayEffects.generated.h"

/**
 * Instant GAS effect used when a LineBody leg action succeeds.
 * The actual negative value is supplied by ACMChimera through SetByCaller,
 * so one effect class can support different part costs later.
 */
UCLASS()
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
