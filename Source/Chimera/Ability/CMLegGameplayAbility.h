#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"

#include "CMLegGameplayAbility.generated.h"

class ACMLegPart;

/** Server-authoritative leg action granted while ACMLegPart is attached. */
UCLASS(Blueprintable)
class CHIMERA_API UCMLegGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UCMLegGameplayAbility();

    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr
    ) const override;

    /** Checks the attached Leg's data-driven cost against shared Stamina. */
    virtual bool CheckCost(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayTagContainer* OptionalRelevantTags = nullptr
    ) const override;

    /** Applies the attached Leg's cost through the configured Cost GE. */
    virtual void ApplyCost(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo
    ) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled
    ) override;

private:
    UFUNCTION()
    void FinishAction();

    TWeakObjectPtr<ACMLegPart> ActiveLegPart;
};
