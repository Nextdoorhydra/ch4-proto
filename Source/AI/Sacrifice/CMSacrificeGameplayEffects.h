#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "CMSacrificeGameplayEffects.generated.h"

UCLASS()
class AI_API UCMSacrificeInitializeGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCMSacrificeInitializeGameplayEffect();
    static const FName MaxFleeChargesDataName;
};

UCLASS()
class AI_API UCMSacrificeConsumeFleeGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCMSacrificeConsumeFleeGameplayEffect();
};

UCLASS()
class AI_API UCMSacrificeRefillFleeGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCMSacrificeRefillFleeGameplayEffect();
};
