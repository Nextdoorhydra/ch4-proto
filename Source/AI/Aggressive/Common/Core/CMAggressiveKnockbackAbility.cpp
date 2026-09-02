#include "Aggressive/Common/Core/CMAggressiveKnockbackAbility.h"

#include "Aggressive/Common/Core/CMAggressivePawnBase.h"
#include "Aggressive/Common/Movement/CMAggressiveKnockbackComponent.h"
#include "Common/Ability/CMAIGameplayTags.h"

UCMAggressiveKnockbackAbility::UCMAggressiveKnockbackAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRetriggerInstancedAbility = true;

    FAbilityTriggerData Trigger;
    Trigger.TriggerTag = CMAIGameplayTags::Event_Aggressive_HitReceived;
    Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    AbilityTriggers.Add(Trigger);
}

// 피격 이벤트를 넉백 실행으로 연결하고 완료 콜백까지 어빌리티를 유지한다.
void UCMAggressiveKnockbackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    ACMAggressivePawnBase* Pawn = Cast<ACMAggressivePawnBase>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);

    if (!Pawn || !CommitAbility(Handle, ActorInfo, ActivationInfo) || !Pawn->StartConfiguredKnockback())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

        return;
    }

    Pawn->SetKnockbackState(true);
    FinishedDelegateHandle = Pawn->GetKnockbackComponent()->OnKnockbackFinished.AddUObject(this, &ThisClass::HandleKnockbackFinished);
}

// 넉백 완료 구독과 상태 태그를 정리하고 취소 시 물리 이동도 중단한다.
void UCMAggressiveKnockbackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ACMAggressivePawnBase* Pawn = Cast<ACMAggressivePawnBase>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
    {
        if (Pawn->GetKnockbackComponent() && FinishedDelegateHandle.IsValid())
        {
            Pawn->GetKnockbackComponent()->OnKnockbackFinished.Remove(FinishedDelegateHandle);
        }
        if (bWasCancelled && Pawn->GetKnockbackComponent())
        {
            Pawn->GetKnockbackComponent()->StopKnockback();
        }
        Pawn->SetKnockbackState(false);
    }

    FinishedDelegateHandle.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCMAggressiveKnockbackAbility::HandleKnockbackFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
