#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"

#include "CMSpringArmGameplayAbility.generated.h"

class ACMSpringArmPart;

UCLASS()
class CHIMERA_API UCMSpringArmGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UCMSpringArmGameplayAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr
	) const override;

	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr
	) const override;

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
	void HandleSpringArmFinished();

	TWeakObjectPtr<ACMSpringArmPart> ActiveSpringArmPart;

	bool bSpringArmStarted = false;
};