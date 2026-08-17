#include "Ability/CMDebugPartLogAbility.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Player/CMChimera.h"
#include "Player/CMDebugPartActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraDebugPart, Log, All);

UCMDebugPartLogAbility::UCMDebugPartLogAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bReplicateInputDirectly = false;
}

void UCMDebugPartLogAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData
)
{
    const FGameplayAbilitySpec* AbilitySpec = ActorInfo
        && ActorInfo->AbilitySystemComponent.IsValid()
        ? ActorInfo->AbilitySystemComponent
            ->FindAbilitySpecFromHandle(Handle)
        : nullptr;
    ACMDebugPartActor* DebugPart = AbilitySpec
        ? Cast<ACMDebugPartActor>(AbilitySpec->SourceObject.Get())
        : nullptr;

    if (DebugPart)
    {
        const FCMPartSlotAddress& Address =
            DebugPart->GetAttachedSlotAddress();
        UE_LOG(LogChimeraDebugPart, Warning,
            TEXT("[Debug Part Input] Slot=(%d,%d) Type=%s Part=%s"),
            Address.SegmentIndex,
            Address.PartSlotIndex,
            *DebugPart->GetPartTypeName(),
            *GetNameSafe(DebugPart));

#if !UE_BUILD_SHIPPING
        ACMPlayerState* ContributingPlayerState =
            DebugPart->ConsumeContributingPlayerState();
        if (DebugPart->GetPartType_Implementation()
            == ECMPartSlotType::Leg)
        {
            if (ACMChimera* Chimera = ActorInfo
                ? Cast<ACMChimera>(ActorInfo->AvatarActor.Get())
                : nullptr)
            {
                Chimera->ActivateDebugLegPart(
                    Address,
                    ContributingPlayerState
                );
            }
        }
#endif
    }
    else
    {
        UE_LOG(LogChimeraDebugPart, Warning,
            TEXT("[Debug Part Input Failed] Ability SourceObject is not ACMDebugPartActor."));
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
