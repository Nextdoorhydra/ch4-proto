#include "Aggressive/Common/Core/CMAggressivePawnBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Aggressive/Common/Core/CMAggressiveAIAttributeSet.h"
#include "Aggressive/Common/Core/CMAggressiveGameplayEffects.h"
#include "Aggressive/Common/Core/CMAggressiveKnockbackAbility.h"
#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"
#include "Aggressive/Common/Movement/CMAggressiveKnockbackComponent.h"
#include "Common/Ability/CMAIGameplayTags.h"
#include "Common/Ability/CMAIStateGameplayEffect.h"

// 공격적 AI가 공유하는 어빌리티 시스템과 넉백 처리를 구성한다.
ACMAggressivePawnBase::ACMAggressivePawnBase()
{
    bReplicates = true;
    SetReplicateMovement(true);

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);
    AggressiveAttributeSet = CreateDefaultSubobject<UCMAggressiveAIAttributeSet>(TEXT("AggressiveAttributeSet"));
    KnockbackComponent = CreateDefaultSubobject<UCMAggressiveKnockbackComponent>(TEXT("KnockbackComponent"));
}

// 서버에서 공통 속성을 초기화하고 피격 넉백 어빌리티를 부여한다.
void ACMAggressivePawnBase::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    if (!HasAuthority())
        return;

    FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UCMAggressiveInitializeGameplayEffect::StaticClass(), 1.0f, AbilitySystemComponent->MakeEffectContext());
    if (Spec.IsValid())
    {
        Spec.Data->SetSetByCallerMagnitude(UCMAggressiveInitializeGameplayEffect::KnockbackDistanceDataName, ConfiguredKnockbackDistanceCm);
        AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    }

    AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UCMAggressiveKnockbackAbility::StaticClass()));
}

UAbilitySystemComponent* ACMAggressivePawnBase::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

// 중복 공격을 걸러낸 뒤 피격 방향을 저장하고 넉백 Gameplay Event를 전달한다.
bool ACMAggressivePawnBase::ReceiveCombatHit_Implementation(const FCMCombatHitRequest& Request)
{
    if (!HasAuthority() || !AggressiveAttributeSet || AggressiveAttributeSet->GetKnockbackDistance() <= 0.0f || WasAttackAlreadyResolved(Request.AttackId))
    {
        return false;
    }

    RememberResolvedAttack(Request.AttackId);
    PendingKnockbackDirection = Request.ImpactDirection.GetSafeNormal2D(SMALL_NUMBER, GetActorForwardVector());
    FGameplayEventData Payload;
    Payload.EventTag = CMAIGameplayTags::Event_Aggressive_HitReceived;
    Payload.Instigator = Request.Attacker;
    Payload.OptionalObject = Request.SourcePart;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, CMAIGameplayTags::Event_Aggressive_HitReceived, Payload);

    return true;
}

// 현재 이동을 중단하고 설정된 거리와 피격 방향으로 넉백을 시작한다.
bool ACMAggressivePawnBase::StartConfiguredKnockback()
{
    ICMAggressiveMovementAgent* MovementAgent = Cast<ICMAggressiveMovementAgent>(this);
    if (!MovementAgent || !AggressiveAttributeSet)
        return false;

    MovementAgent->StopAggressiveMovementForReaction();

    return KnockbackComponent->StartKnockback(PendingKnockbackDirection, AggressiveAttributeSet->GetKnockbackDistance());
}

// 넉백 상태 태그를 Gameplay Effect 수명과 함께 적용하거나 해제한다.
void ACMAggressivePawnBase::SetKnockbackState(const bool bEnabled)
{
    if (!AbilitySystemComponent)
        return;
    if (!bEnabled)
    {
        if (KnockbackStateHandle.IsValid())
        {
            AbilitySystemComponent->RemoveActiveGameplayEffect(KnockbackStateHandle);
            KnockbackStateHandle.Invalidate();
        }
        return;
    }

    if (KnockbackStateHandle.IsValid())
        return;

    FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UCMAIStateGameplayEffect::StaticClass(), 1.0f, AbilitySystemComponent->MakeEffectContext());

    if (Spec.IsValid())
    {
        Spec.Data->DynamicGrantedTags.AddTag(CMAIGameplayTags::State_Aggressive_Knockback);
        KnockbackStateHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    }
}

bool ACMAggressivePawnBase::WasAttackAlreadyResolved(const FGuid& AttackId) const
{
    return AttackId.IsValid() && RecentAttackIds.Contains(AttackId);
}

// 네트워크 중복 피격을 막되 기록이 무한히 늘지 않도록 최근 공격만 보관한다.
void ACMAggressivePawnBase::RememberResolvedAttack(const FGuid& AttackId)
{
    if (!AttackId.IsValid())
        return;
    RecentAttackIds.Add(AttackId);
    if (RecentAttackIds.Num() > 32)
        RecentAttackIds.RemoveAt(0, RecentAttackIds.Num() - 32);
}
