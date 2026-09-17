#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"

#include "CMArmGameplayAbility.generated.h"

class ACMArmPart;

/** Server-authoritative Arm swing whose full active time is a parry window. */
UCLASS(Blueprintable)
class CHIMERA_API UCMArmGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UCMArmGameplayAbility();

    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr
    ) const override;

    /** Checks the attached Arm's data-driven cost against shared Stamina. */
    virtual bool CheckCost(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayTagContainer* OptionalRelevantTags = nullptr
    ) const override;

    /** Applies the attached Arm's cost through the configured Cost GE. */
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
    void FinishSwing();

    TWeakObjectPtr<ACMArmPart> ActiveArmPart;
    bool bSwingStarted = false;
};
