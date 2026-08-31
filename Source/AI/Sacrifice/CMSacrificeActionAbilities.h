#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Sacrifice/CMSacrificeAITypes.h"

#include "CMSacrificeActionAbilities.generated.h"

UCLASS(Abstract)
class AI_API UCMSacrificeActionGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UCMSacrificeActionGameplayAbility();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    ECMSacrificeActionState ActionState = ECMSacrificeActionState::None;
    FGameplayTag ActionStateTag;
};

UCLASS()
class AI_API UCMSacrificePrayerAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificePrayerAbility();
};

UCLASS()
class AI_API UCMSacrificeLookAroundAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificeLookAroundAbility();
};

UCLASS()
class AI_API UCMSacrificeWanderAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificeWanderAbility();
};

UCLASS()
class AI_API UCMSacrificeBackFallAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificeBackFallAbility();
};

UCLASS()
class AI_API UCMSacrificeBackCrawlAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificeBackCrawlAbility();
};

UCLASS()
class AI_API UCMSacrificeFleeAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificeFleeAbility();
};

UCLASS()
class AI_API UCMSacrificeInjuredCrawlAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificeInjuredCrawlAbility();
};

UCLASS()
class AI_API UCMSacrificeGettingUpAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificeGettingUpAbility();
};

UCLASS()
class AI_API UCMSacrificeExhaustedWalkAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificeExhaustedWalkAbility();
};

UCLASS()
class AI_API UCMSacrificeVigilantAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificeVigilantAbility();
};

UCLASS()
class AI_API UCMSacrificeIncapacitatedAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificeIncapacitatedAbility();
};

UCLASS()
class AI_API UCMSacrificeDeathAbility : public UCMSacrificeActionGameplayAbility
{
    GENERATED_BODY()
public:
    UCMSacrificeDeathAbility();
};
