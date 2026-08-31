#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Delegates/Delegate.h"

#include "CMAggressiveKnockbackAbility.generated.h"

UCLASS()
class AI_API UCMAggressiveKnockbackAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UCMAggressiveKnockbackAbility();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    void HandleKnockbackFinished();
    FDelegateHandle FinishedDelegateHandle;
};
