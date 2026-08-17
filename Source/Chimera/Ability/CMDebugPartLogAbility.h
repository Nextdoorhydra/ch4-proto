#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"

#include "CMDebugPartLogAbility.generated.h"

/** Server-only diagnostic ability granted by ACMDebugPartActor. */
UCLASS()
class CHIMERA_API UCMDebugPartLogAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UCMDebugPartLogAbility();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;
};
