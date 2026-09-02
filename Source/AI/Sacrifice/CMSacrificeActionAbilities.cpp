#include "Sacrifice/CMSacrificeActionAbilities.h"

#include "Common/Ability/CMAIGameplayTags.h"
#include "Sacrifice/CMSacrificeCharacter.h"

UCMSacrificeActionGameplayAbility::UCMSacrificeActionGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(CMAIGameplayTags::Ability_Sacrifice_Action);
    SetAssetTags(AssetTags);
}

// 행동 어빌리티가 활성화되면 캐릭터 상태와 대응 Gameplay Tag를 함께 설정한다.
void UCMSacrificeActionGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    ACMSacrificeCharacter* Character = Cast<ACMSacrificeCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
    if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

        return;
    }

    Character->SetSacrificeActionState(ActionState, ActionStateTag);
}

// 행동 어빌리티 종료 시 자신이 설정한 상태와 Gameplay Tag만 안전하게 해제한다.
void UCMSacrificeActionGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ACMSacrificeCharacter* Character = Cast<ACMSacrificeCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
    {
        Character->ClearSacrificeActionState(ActionState, ActionStateTag);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#define CM_DEFINE_SACRIFICE_ACTION_ABILITY(ClassName, State, Tag)                                                                                                                                                                                                                                          \
    ClassName::ClassName()                                                                                                                                                                                                                                                                                 \
    {                                                                                                                                                                                                                                                                                                      \
        ActionState = ECMSacrificeActionState::State;                                                                                                                                                                                                                                                      \
        ActionStateTag = CMAIGameplayTags::Tag;                                                                                                                                                                                                                                                            \
    }

CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificePrayerAbility, Prayer, State_Sacrifice_Prayer)
CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificeLookAroundAbility, LookAround, State_Sacrifice_LookAround)
CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificeWanderAbility, Wander, State_Sacrifice_Wander)
CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificeBackFallAbility, BackFall, State_Sacrifice_BackFall)
CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificeBackCrawlAbility, BackCrawl, State_Sacrifice_BackCrawl)
CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificeFleeAbility, Flee, State_Sacrifice_Flee)
CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificeInjuredCrawlAbility, InjuredCrawl, State_Sacrifice_InjuredCrawl)
CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificeGettingUpAbility, GettingUp, State_Sacrifice_GettingUp)
CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificeExhaustedWalkAbility, ExhaustedWalk, State_Sacrifice_Exhausted)
CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificeVigilantAbility, Vigilant, State_Sacrifice_Vigilant)
CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificeIncapacitatedAbility, Incapacitated, State_Sacrifice_Incapacitated)
CM_DEFINE_SACRIFICE_ACTION_ABILITY(UCMSacrificeDeathAbility, Dead, State_Sacrifice_Dead)

#undef CM_DEFINE_SACRIFICE_ACTION_ABILITY
