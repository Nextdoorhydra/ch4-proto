#include "Ability/CMArmGameplayAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Ability/CMChimeraAttributeSet.h"
#include "Ability/CMStaminaGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Parts/Arm/CMArmPart.h"
#include "Player/CMChimera.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraArmAbility, Log, All);

UCMArmGameplayAbility::UCMArmGameplayAbility()
{
    // Each Arm is granted as a separate Spec. Keeping one instance per Spec
    // makes GAS reject repeated input from this Arm for the full swing while
    // other Arms on the shared ASC remain independent.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bReplicateInputDirectly = false;
    bRetriggerInstancedAbility = false;
    CostGameplayEffectClass = UCMStaminaCostGameplayEffect::StaticClass();
}

bool UCMArmGameplayAbility::CheckCost(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayTagContainer* OptionalRelevantTags
) const
{
    if (!ActorInfo)
    {
        return false;
    }

    const ACMArmPart* ArmPart = Cast<ACMArmPart>(
        GetSourceObject(Handle, ActorInfo)
    );
    const UAbilitySystemComponent* ASC =
        ActorInfo->AbilitySystemComponent.Get();
    const UCMChimeraAttributeSet* Attributes = ASC
        ? ASC->GetSet<UCMChimeraAttributeSet>()
        : nullptr;
    return ArmPart && Attributes
        && Attributes->GetStamina()
            >= FMath::Max(ArmPart->GetStaminaCost(), 0.0f);
}

void UCMArmGameplayAbility::ApplyCost(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo
) const
{
    const ACMArmPart* ArmPart = Cast<ACMArmPart>(
        GetSourceObject(Handle, ActorInfo)
    );
    const UGameplayEffect* CostEffect = GetCostGameplayEffect();
    if (!ArmPart || !CostEffect)
    {
        return;
    }

    FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(
        CostEffect->GetClass(),
        GetAbilityLevel()
    );
    if (!CostSpec.IsValid())
    {
        return;
    }

    CostSpec.Data->SetSetByCallerMagnitude(
        UCMStaminaCostGameplayEffect::StaminaCostDataName,
        -FMath::Max(ArmPart->GetStaminaCost(), 0.0f)
    );
    const UCMChimeraAttributeSet* Attributes = ActorInfo
        && ActorInfo->AbilitySystemComponent.IsValid()
        ? ActorInfo->AbilitySystemComponent->GetSet<UCMChimeraAttributeSet>()
        : nullptr;
    const float OldStamina = Attributes ? Attributes->GetStamina() : 0.0f;
    ApplyGameplayEffectSpecToOwner(
        Handle,
        ActorInfo,
        ActivationInfo,
        CostSpec
    );
    UE_LOG(LogChimeraArmAbility, Log,
        TEXT("[Arm GE Cost] Part=%s Cost=%.1f SharedStamina=%.1f -> %.1f"),
        *GetNameSafe(ArmPart),
        FMath::Max(ArmPart->GetStaminaCost(), 0.0f),
        OldStamina,
        Attributes ? Attributes->GetStamina() : 0.0f);
}

bool UCMArmGameplayAbility::CanActivateAbility(
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

    const ACMArmPart* ArmPart = Cast<ACMArmPart>(
        GetSourceObject(Handle, ActorInfo)
    );
    const ACMChimera* Chimera = ActorInfo
        ? Cast<ACMChimera>(ActorInfo->AvatarActor.Get())
        : nullptr;
    return ArmPart && Chimera && ArmPart->IsOperational()
        && !ArmPart->IsSwinging();
}

void UCMArmGameplayAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData
)
{
    // As in NetKarma, the BP child configures Class Defaults while C++ owns
    // validation, commit, tasks, and cleanup.
    Super::ActivateAbility(
        Handle,
        ActorInfo,
        ActivationInfo,
        TriggerEventData
    );

    ActiveArmPart = Cast<ACMArmPart>(GetSourceObject(Handle, ActorInfo));
    bSwingStarted = false;
    if (!ActiveArmPart.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ACMArmPart* ArmPart = ActiveArmPart.Get();
    ACMChimera* Chimera = ActorInfo
        ? Cast<ACMChimera>(ActorInfo->AvatarActor.Get())
        : nullptr;
    bSwingStarted = Chimera && Chimera->TryActivateArmPart(
        ArmPart,
        ArmPart->ConsumeContributingPlayerState()
    );
    if (!bSwingStarted)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UE_LOG(LogChimeraArmAbility, Log,
        TEXT("[Arm Ability] Part=%s SwingDuration=%.3f Started=true Driver=C++"),
        *GetNameSafe(ArmPart),
        ArmPart->GetSwingDuration());

    UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(
        this,
        ActiveArmPart->GetSwingDuration()
    );
    WaitTask->OnFinish.AddDynamic(this, &UCMArmGameplayAbility::FinishSwing);
    WaitTask->ReadyForActivation();
}

void UCMArmGameplayAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled
)
{
    ACMArmPart* ArmPart = ActiveArmPart.Get();
    if (ArmPart)
    {
        if (bSwingStarted)
        {
            ArmPart->EndSwing();
        }
        ArmPart->ConsumeContributingPlayerState();
    }
    ActiveArmPart.Reset();
    bSwingStarted = false;

    Super::EndAbility(
        Handle,
        ActorInfo,
        ActivationInfo,
        bReplicateEndAbility,
        bWasCancelled
    );
}

void UCMArmGameplayAbility::FinishSwing()
{
    EndAbility(
        CurrentSpecHandle,
        CurrentActorInfo,
        CurrentActivationInfo,
        true,
        false
    );
}
