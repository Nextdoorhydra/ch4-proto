#include "Ability/CMSpringArmGameplayAbility.h"

#include "Ability/CMChimeraAttributeSet.h"
#include "Ability/CMStaminaGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "Parts/Arm/CMSpringArmPart.h"
#include "Player/CMChimera.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraSpringArmAbility, Log, All);

UCMSpringArmGameplayAbility::UCMSpringArmGameplayAbility()
{
	InstancingPolicy =
		EGameplayAbilityInstancingPolicy::InstancedPerActor;

	NetExecutionPolicy =
		EGameplayAbilityNetExecutionPolicy::ServerOnly;

	bReplicateInputDirectly = false;
	bRetriggerInstancedAbility = false;

	CostGameplayEffectClass =
		UCMStaminaCostGameplayEffect::StaticClass();
}

void UCMSpringArmGameplayAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData
)
{
    Super::ActivateAbility(
        Handle,
        ActorInfo,
        ActivationInfo,
        TriggerEventData
    );

    ActiveSpringArmPart =
        Cast<ACMSpringArmPart>(
            GetSourceObject(Handle, ActorInfo)
        );

    bSpringArmStarted = false;

    if (!ActiveSpringArmPart.IsValid())
    {
        EndAbility(
            Handle,
            ActorInfo,
            ActivationInfo,
            true,
            true
        );
        return;
    }

    ACMSpringArmPart* SpringArm =
        ActiveSpringArmPart.Get();

    ACMChimera* Chimera =
        ActorInfo
        ? Cast<ACMChimera>(ActorInfo->AvatarActor.Get())
        : nullptr;

    if (!Chimera)
    {
        EndAbility(
            Handle,
            ActorInfo,
            ActivationInfo,
            true,
            true
        );
        return;
    }
	
    // ★ 먼저 완료 이벤트를 연결
    SpringArm->OnSpringArmFinished.AddDynamic(
        this,
        &UCMSpringArmGameplayAbility::HandleSpringArmFinished
    );

    bSpringArmStarted =
        Chimera->TryActivateArmPart(
            SpringArm,
            SpringArm->ConsumeContributingPlayerState()
        );

    if (!bSpringArmStarted)
    {
        SpringArm->OnSpringArmFinished.RemoveDynamic(
            this,
            &UCMSpringArmGameplayAbility::HandleSpringArmFinished
        );

        EndAbility(
            Handle,
            ActorInfo,
            ActivationInfo,
            true,
            true
        );
        return;
    }

    if (!CommitAbility(
        Handle,
        ActorInfo,
        ActivationInfo))
    {
        EndAbility(
            Handle,
            ActorInfo,
            ActivationInfo,
            true,
            true
        );
        return;
    }

    UE_LOG(
        LogChimeraSpringArmAbility,
        Log,
        TEXT("[SpringArm Ability] Started Part=%s"),
        *GetNameSafe(SpringArm)
    );
}

bool UCMSpringArmGameplayAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags
) const
{
	if (!ActorInfo)
	{
		return false;
	}

	const ACMSpringArmPart* SpringArm =
		Cast<ACMSpringArmPart>(
			GetSourceObject(Handle, ActorInfo)
		);

	const UAbilitySystemComponent* ASC =
		ActorInfo->AbilitySystemComponent.Get();

	const UCMChimeraAttributeSet* Attributes =
		ASC
		? ASC->GetSet<UCMChimeraAttributeSet>()
		: nullptr;

	return SpringArm
		&& Attributes
		&& Attributes->GetStamina()
			>= FMath::Max(SpringArm->GetStaminaCost(), 0.0f);
}

void UCMSpringArmGameplayAbility::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo
) const
{
	const ACMSpringArmPart* SpringArm =
		Cast<ACMSpringArmPart>(
			GetSourceObject(Handle, ActorInfo)
		);

	const UGameplayEffect* CostEffect =
		GetCostGameplayEffect();

	if (!SpringArm || !CostEffect)
	{
		return;
	}

	FGameplayEffectSpecHandle CostSpec =
		MakeOutgoingGameplayEffectSpec(
			CostEffect->GetClass(),
			GetAbilityLevel()
		);

	if (!CostSpec.IsValid())
	{
		return;
	}

	CostSpec.Data->SetSetByCallerMagnitude(
		UCMStaminaCostGameplayEffect::StaminaCostDataName,
		-FMath::Max(SpringArm->GetStaminaCost(), 0.0f)
	);

	(void)ApplyGameplayEffectSpecToOwner(
		Handle,
		ActorInfo,
		ActivationInfo,
		CostSpec
	);
}

bool UCMSpringArmGameplayAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags
) const
{
	if (!Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags))
	{
		return false;
	}

	const ACMSpringArmPart* SpringArm =
		Cast<ACMSpringArmPart>(
			GetSourceObject(Handle, ActorInfo)
		);

	const ACMChimera* Chimera =
		ActorInfo
		? Cast<ACMChimera>(ActorInfo->AvatarActor.Get())
		: nullptr;

	return SpringArm
		&& Chimera
		&& SpringArm->IsOperational()
		&& !SpringArm->IsSwinging();
}

void UCMSpringArmGameplayAbility::HandleSpringArmFinished()
{
	UE_LOG(LogChimeraSpringArmAbility, Log,
	TEXT("[SpringArm Ability] Finished Part=%s"),
	*GetNameSafe(ActiveSpringArmPart.Get()));
	
	EndAbility(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		true,
		false
	);
}

void UCMSpringArmGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	ACMSpringArmPart* SpringArm =
		ActiveSpringArmPart.Get();

	if (SpringArm)
	{
		SpringArm->OnSpringArmFinished.RemoveDynamic(
			this,
			&UCMSpringArmGameplayAbility::HandleSpringArmFinished
		);

		if (bSpringArmStarted)
		{
			SpringArm->EndSwing();
		}

		SpringArm->ConsumeContributingPlayerState();
	}

	ActiveSpringArmPart.Reset();
	bSpringArmStarted = false;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled
	);
}