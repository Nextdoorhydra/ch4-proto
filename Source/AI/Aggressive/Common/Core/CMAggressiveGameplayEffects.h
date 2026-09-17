#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "CMAggressiveGameplayEffects.generated.h"

UCLASS()
class AI_API UCMAggressiveInitializeGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCMAggressiveInitializeGameplayEffect();
    static const FName KnockbackDistanceDataName;
};
