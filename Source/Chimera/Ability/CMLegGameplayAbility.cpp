#include "Ability/CMLegGameplayAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Ability/CMChimeraAttributeSet.h"
#include "Ability/CMStaminaGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Parts/Leg/CMLegPart.h"
#include "Player/CMChimera.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraLegAbility, Log, All);

UCMLegGameplayAbility::UCMLegGameplayAbility()
{
    // Each attached Part receives its own Ability Spec. InstancedPerActor
    // therefore blocks only repeated activation of this exact Leg Spec while
    // allowing the other Legs on the shared ASC to run at the same time.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bReplicateInputDirectly = false;
    bRetriggerInstancedAbility = false;

    // BP children may replace this class in Class Defaults, just like the
    // NetKarma BGA assets. The magnitude itself remains Part data and is
    // supplied with SetByCaller in ApplyCost().
    CostGameplayEffectClass = UCMStaminaCostGameplayEffect::StaticClass();
}

bool UCMLegGameplayAbility::CheckCost(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayTagContainer* OptionalRelevantTags
) const
{
    if (!ActorInfo)
    {
        return false;
    }

    const ACMLegPart* LegPart = Cast<ACMLegPart>(
        GetSourceObject(Handle, ActorInfo)
    );
    const UAbilitySystemComponent* ASC =
        ActorInfo->AbilitySystemComponent.Get();
    const UCMChimeraAttributeSet* Attributes = ASC
        ? ASC->GetSet<UCMChimeraAttributeSet>()
        : nullptr;
    return LegPart && Attributes
        && Attributes->GetStamina()
            >= FMath::Max(LegPart->GetStaminaCost(), 0.0f);
}

void UCMLegGameplayAbility::ApplyCost(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo
) const
{
    const ACMLegPart* LegPart = Cast<ACMLegPart>(
        GetSourceObject(Handle, ActorInfo)
    );
    const UGameplayEffect* CostEffect = GetCostGameplayEffect();
    if (!LegPart || !CostEffect)
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
        -FMath::Max(LegPart->GetStaminaCost(), 0.0f)
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
    UE_LOG(LogChimeraLegAbility, Log,
        TEXT("[Leg GE Cost] Part=%s Cost=%.1f SharedStamina=%.1f -> %.1f"),
        *GetNameSafe(LegPart),
        FMath::Max(LegPart->GetStaminaCost(), 0.0f),
        OldStamina,
        Attributes ? Attributes->GetStamina() : 0.0f);
}

bool UCMLegGameplayAbility::CanActivateAbility(
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

    const ACMLegPart* LegPart = Cast<ACMLegPart>(
        GetSourceObject(Handle, ActorInfo)
    );
    const ACMChimera* Chimera = ActorInfo
        ? Cast<ACMChimera>(ActorInfo->AvatarActor.Get())
        : nullptr;
    return LegPart && Chimera && LegPart->IsOperational();
}

void UCMLegGameplayAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData
)
{
    // NetKarma-style BGA children are configuration-only. Do not implement
    // Event Activate Ability in the BP child or it would run from this Super.
    Super::ActivateAbility(
        Handle,
        ActorInfo,
        ActivationInfo,
        TriggerEventData
    );

    ActiveLegPart = Cast<ACMLegPart>(GetSourceObject(Handle, ActorInfo));
    if (!ActiveLegPart.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ACMLegPart* LegPart = ActiveLegPart.Get();
    ACMChimera* Chimera = ActorInfo
        ? Cast<ACMChimera>(ActorInfo->AvatarActor.Get())
        : nullptr;
    const bool bMovementApplied = Chimera && Chimera->TryActivateLegPart(
        LegPart->GetAttachedSlotAddress(),
        LegPart->ConsumeContributingPlayerState()
    );
    if (!bMovementApplied)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Commit after the ground-dependent action succeeds so rejected airborne
    // input does not consume shared Stamina. Commit still owns GE cost logic.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UE_LOG(LogChimeraLegAbility, Log,
        TEXT("[Leg Ability] Chimera=%s Part=%s Slot=(%d,%d) MovementApplied=true ActionDuration=%.3f Driver=C++"),
        *GetNameSafe(Chimera),
        *GetNameSafe(LegPart),
        LegPart->GetAttachedSlotAddress().SegmentIndex,
        LegPart->GetAttachedSlotAddress().PartSlotIndex,
        LegPart->GetActionDuration());

    UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(
        this,
        ActiveLegPart->GetActionDuration()
    );
    WaitTask->OnFinish.AddDynamic(
        this,
        &UCMLegGameplayAbility::FinishAction
    );
    WaitTask->ReadyForActivation();
}

void UCMLegGameplayAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled
)
{
    if (ACMLegPart* LegPart = ActiveLegPart.Get())
    {
        LegPart->ConsumeContributingPlayerState();
    }
    ActiveLegPart.Reset();

    Super::EndAbility(
        Handle,
        ActorInfo,
        ActivationInfo,
        bReplicateEndAbility,
        bWasCancelled
    );
}

void UCMLegGameplayAbility::FinishAction()
{
    EndAbility(
        CurrentSpecHandle,
        CurrentActorInfo,
        CurrentActivationInfo,
        true,
        false
    );
}
