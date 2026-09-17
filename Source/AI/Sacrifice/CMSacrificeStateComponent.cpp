#include "Sacrifice/CMSacrificeStateComponent.h"

#include "Sacrifice/CMSacrificeCharacter.h"
#include "Sacrifice/CMSacrificeRules.h"
#include "Engine/World.h"
#include "Gore/CMDismembermentComponent.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Parts/Leg/CMLegPart.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMSacrifice, Log, All);

namespace
{
    constexpr int32 MaxRememberedAttackIds = 32;

    TSubclassOf<ACMPartActorBase> GetDefaultCollectiblePartClass(const ECMBodyPart BodyPart)
    {
        if (BodyPart == ECMBodyPart::Head)
        {
            return ACMHeadPartActor::StaticClass();
        }
        if (BodyPart == ECMBodyPart::ArmLeft || BodyPart == ECMBodyPart::ArmRight)
        {
            return ACMArmPart::StaticClass();
        }
        if (BodyPart == ECMBodyPart::LegLeft || BodyPart == ECMBodyPart::LegRight)
        {
            return ACMLegPart::StaticClass();
        }
        return nullptr;
    }
}

UCMSacrificeStateComponent::UCMSacrificeStateComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCMSacrificeStateComponent::BeginPlay()
{
    Super::BeginPlay();
    DismembermentComponent = GetOwner() ? GetOwner()->FindComponentByClass<UCMDismembermentComponent>() : nullptr;
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ValidateAttackPartRules();
    }
}

void UCMSacrificeStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BleedTimerHandle);
    }
    Super::EndPlay(EndPlayReason);
}

void UCMSacrificeStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCMSacrificeStateComponent, MissingPartMask);
    DOREPLIFETIME(UCMSacrificeStateComponent, bDead);
    DOREPLIFETIME(UCMSacrificeStateComponent, bBleeding);
    DOREPLIFETIME(UCMSacrificeStateComponent, BleedEndTime);
    DOREPLIFETIME(UCMSacrificeStateComponent, DroppedRewardMask);
}

// 중복 피격을 배제하고 부착 신체 하나를 선택해 절단·보상·출혈·사망 상태를 갱신한다.
int32 UCMSacrificeStateComponent::ResolveDismembermentHit(const FCMDismembermentHitRequest& Request)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || bDead
        || !DismembermentComponent || WasAttackAlreadyResolved(Request.AttackId))
    {
        return 0;
    }
    RememberResolvedAttack(Request.AttackId);

    const TArray<ECMBodyPart> AvailableParts = GetAttachedBodyParts();
    FRandomStream RandomStream(FMath::Rand());
    const ECMBodyPart SelectedPart = FCMSacrificeRules::SelectRandomPart(AvailableParts, RandomStream);
    if (SelectedPart == ECMBodyPart::None)
    {
        return 0;
    }

    const int32 PreviousMissingCount = GetMissingPartCount();
    const ACMSacrificeCharacter* Character = Cast<ACMSacrificeCharacter>(Owner);
    const float SeverImpulse = Character ? Character->GetDismembermentImpulse() : 0.0f;
    const FVector Impulse = Request.ImpactDirection.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector) * SeverImpulse;
    TSubclassOf<ACMPartActorBase> RewardClass = HasRewardDropped(SelectedPart) ? nullptr : ResolveCollectiblePartClass(SelectedPart);
    const bool bSevered = RewardClass ? DismembermentComponent->SeverBodyPartWithReward(SelectedPart, Request.ImpactPoint, Impulse, RewardClass) : DismembermentComponent->SeverBodyPart(SelectedPart, Request.ImpactPoint, Impulse);
    if (!bSevered)
    {
        return 0;
    }

    const uint8 SelectedPartBit = FCMSacrificeRules::GetBodyPartBit(SelectedPart);
    MissingPartMask |= SelectedPartBit;
    if (RewardClass)
    {
        DroppedRewardMask |= SelectedPartBit;
    }
    OnBodyPartSevered.Broadcast(SelectedPart);
    OnMissingPartsChanged.Broadcast(GetMissingPartCount());
    Owner->ForceNetUpdate();

    // 머리 절단은 출혈 타이머를 거치지 않고 즉시 사망으로 확정한다.
    if (SelectedPart == ECMBodyPart::Head)
    {
        Die();

        return 1;
    }

    StartOrUpdateBleeding(PreviousMissingCount, 1);

    return 1;
}

// Centipede 치명 공격은 장착 규칙과 무관하게 남은 모든 신체를 절단하고 즉시 사망 처리한다.
int32 UCMSacrificeStateComponent::ResolveFatalDismembermentHit(const FCMDismembermentHitRequest& Request)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || bDead
        || WasAttackAlreadyResolved(Request.AttackId))
    {
        return 0;
    }
    RememberResolvedAttack(Request.AttackId);

    const ACMSacrificeCharacter* Character = Cast<ACMSacrificeCharacter>(Owner);
    const float SeverImpulse = Character ? Character->GetDismembermentImpulse() : 0.0f;
    int32 SeveredPartCount = 0;
    for (const ECMBodyPart BodyPart : GetAttachedBodyParts())
    {
        if (!DismembermentComponent)
        {
            break;
        }
        const FVector ScatterDirection = (Request.ImpactDirection + FMath::VRand() * 0.5f).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
        const FVector Impulse = ScatterDirection * SeverImpulse;
        TSubclassOf<ACMPartActorBase> RewardClass = HasRewardDropped(BodyPart) ? nullptr : ResolveCollectiblePartClass(BodyPart);
        const bool bSevered = RewardClass ? DismembermentComponent->SeverBodyPartWithReward(BodyPart, Request.ImpactPoint, Impulse, RewardClass) : DismembermentComponent->SeverBodyPart(BodyPart, Request.ImpactPoint, Impulse);
        if (!bSevered)
        {
            continue;
        }

        const uint8 Bit = FCMSacrificeRules::GetBodyPartBit(BodyPart);
        MissingPartMask |= Bit;
        if (RewardClass)
        {
            DroppedRewardMask |= Bit;
        }
        ++SeveredPartCount;
        OnBodyPartSevered.Broadcast(BodyPart);
    }

    if (SeveredPartCount > 0)
    {
        OnMissingPartsChanged.Broadcast(GetMissingPartCount());
    }
    Die();
    Owner->ForceNetUpdate();
    return FMath::Max(SeveredPartCount, 1);
}

bool UCMSacrificeStateComponent::HasBodyPart(const ECMBodyPart BodyPart) const
{
    const uint8 Bit = FCMSacrificeRules::GetBodyPartBit(BodyPart);
    return Bit != 0 && (MissingPartMask & Bit) == 0 && FCMSacrificeRules::GetSeverableBodyParts().Contains(BodyPart);
}

int32 UCMSacrificeStateComponent::GetMissingPartCount() const
{
    int32 Count = 0;
    for (const ECMBodyPart BodyPart : FCMSacrificeRules::GetSeverableBodyParts())
    {
        Count += HasBodyPart(BodyPart) ? 0 : 1;
    }

    return Count;
}

TArray<ECMBodyPart> UCMSacrificeStateComponent::GetAttachedBodyParts() const
{
    TArray<ECMBodyPart> Result;
    for (const ECMBodyPart BodyPart : FCMSacrificeRules::GetSeverableBodyParts())
    {
        if (HasBodyPart(BodyPart))
        {
            Result.Add(BodyPart);
        }
    }

    return Result;
}

float UCMSacrificeStateComponent::GetBleedTimeRemaining() const
{
    const UWorld* World = GetWorld();
    return bBleeding && World ? FMath::Max(BleedEndTime - World->GetTimeSeconds(), 0.0f) : 0.0f;
}

bool UCMSacrificeStateComponent::HasRewardForBodyPart(const ECMBodyPart BodyPart) const
{
    return ResolveCollectiblePartClass(BodyPart) != nullptr;
}

bool UCMSacrificeStateComponent::HasRewardDropped(const ECMBodyPart BodyPart) const
{
    const uint8 Bit = FCMSacrificeRules::GetBodyPartBit(BodyPart);
    return Bit != 0 && (DroppedRewardMask & Bit) != 0;
}

int32 UCMSacrificeStateComponent::GetRemainingRewardCount() const
{
    const ACMSacrificeCharacter* Character = Cast<ACMSacrificeCharacter>(GetOwner());
    if (!Character)
    {
        return 0;
    }

    int32 Count = 0;
    for (const ECMBodyPart BodyPart : FCMSacrificeRules::GetSeverableBodyParts())
    {
        if (HasBodyPart(BodyPart) && !HasRewardDropped(BodyPart) && ResolveCollectiblePartClass(BodyPart))
        {
            ++Count;
        }
    }
    return Count;
}

void UCMSacrificeStateComponent::OnRep_MissingPartMask(const uint8 PreviousMask)
{
    for (const ECMBodyPart BodyPart : FCMSacrificeRules::GetSeverableBodyParts())
    {
        const uint8 Bit = FCMSacrificeRules::GetBodyPartBit(BodyPart);
        if ((PreviousMask & Bit) == 0 && (MissingPartMask & Bit) != 0)
        {
            OnBodyPartSevered.Broadcast(BodyPart);
        }
    }
    OnMissingPartsChanged.Broadcast(GetMissingPartCount());
}

void UCMSacrificeStateComponent::OnRep_Bleeding()
{
    OnBleedingStateChanged.Broadcast(bBleeding);
}

void UCMSacrificeStateComponent::OnRep_Dead()
{
    if (bDead)
    {
        OnSacrificeDied.Broadcast();
    }
}

// 최초 절단은 출혈 시간을 설정하고 추가 절단은 남은 시간을 단축해 사망 타이머를 갱신한다.
void UCMSacrificeStateComponent::StartOrUpdateBleeding(const int32 PreviousMissingCount, const int32 NewlyMissingCount)
{
    UWorld* World = GetWorld();
    if (!World || bDead || NewlyMissingCount <= 0)
    {
        return;
    }

    const float RemainingSeconds = PreviousMissingCount == 0 ? FCMSacrificeRules::GetInitialBleedDuration(GetMissingPartCount()) : GetBleedTimeRemaining() - FCMSacrificeRules::GetAdditionalBleedReduction(NewlyMissingCount);
    if (RemainingSeconds <= 0.0f)
    {
        Die();
        return;
    }

    const bool bWasBleeding = bBleeding;
    bBleeding = true;
    BleedEndTime = World->GetTimeSeconds() + RemainingSeconds;
    World->GetTimerManager().SetTimer(BleedTimerHandle, this, &ThisClass::HandleBleedExpired, RemainingSeconds, false);
    if (!bWasBleeding)
    {
        OnBleedingStateChanged.Broadcast(true);
    }
    GetOwner()->ForceNetUpdate();
}

void UCMSacrificeStateComponent::HandleBleedExpired()
{
    Die();
}

// 출혈 상태를 확정하고 시체 래그돌 및 사망 이벤트를 서버에서 한 번 실행한다.
void UCMSacrificeStateComponent::Die()
{
    if (bDead || !GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BleedTimerHandle);
    }
    const bool bWasBleeding = bBleeding;
    bBleeding = false;
    BleedEndTime = 0.0f;
    bDead = true;

    if (bWasBleeding)
    {
        OnBleedingStateChanged.Broadcast(false);
    }
    if (DismembermentComponent)
    {
        DismembermentComponent->EnterCorpseRagdoll();
    }
    OnSacrificeDied.Broadcast();
    GetOwner()->ForceNetUpdate();
}

// 시작 시 공격 절단 규칙의 부위와 중복 설정을 검사한다.
void UCMSacrificeStateComponent::ValidateAttackPartRules() const
{
    const ACMSacrificeCharacter* Character = Cast<ACMSacrificeCharacter>(GetOwner());
    if (!Character)
        return;

    TSet<ECMBodyPart> SeenParts;
    for (const FCMSacrificeAttackPartRule& Rule : Character->GetAttackPartRules())
    {
        const bool bValidBodyPart = FCMSacrificeRules::GetBodyPartBit(Rule.BodyPart) != 0 && FCMSacrificeRules::GetSeverableBodyParts().Contains(Rule.BodyPart);
        if (!bValidBodyPart)
        {
            UE_LOG(LogCMSacrifice, Warning, TEXT("[Sacrifice Dismemberment] '%s' has an invalid attack rule for body part %d."), *GetNameSafe(Character), static_cast<int32>(Rule.BodyPart));
            continue;
        }
        if (SeenParts.Contains(Rule.BodyPart))
        {
            UE_LOG(LogCMSacrifice, Warning, TEXT("[Sacrifice Dismemberment] '%s' has duplicate attack rules for body part %d; only the first is used."), *GetNameSafe(Character), static_cast<int32>(Rule.BodyPart));
            continue;
        }
        SeenParts.Add(Rule.BodyPart);
    }
}

const FCMSacrificeAttackPartRule* UCMSacrificeStateComponent::FindAttackPartRule(const ECMBodyPart BodyPart) const
{
    const ACMSacrificeCharacter* Character = Cast<ACMSacrificeCharacter>(GetOwner());
    return Character ? Character->GetAttackPartRules().FindByPredicate(
        [BodyPart](const FCMSacrificeAttackPartRule& Rule)
        {
            return Rule.BodyPart == BodyPart;
        }) : nullptr;
}

TSubclassOf<ACMPartActorBase> UCMSacrificeStateComponent::ResolveCollectiblePartClass(const ECMBodyPart BodyPart) const
{
    const FCMSacrificeAttackPartRule* Rule = FindAttackPartRule(BodyPart);
    if (!Rule || !Rule->bPlayerCanAcquire)
    {
        return nullptr;
    }
    return Rule->PartClassOverride ? Rule->PartClassOverride : GetDefaultCollectiblePartClass(BodyPart);
}

bool UCMSacrificeStateComponent::WasAttackAlreadyResolved(const FGuid& AttackId) const
{
    return AttackId.IsValid() && RecentAttackIds.Contains(AttackId);
}

// 네트워크 중복 절단을 막되 기록이 무한히 늘지 않도록 최근 공격만 보관한다.
void UCMSacrificeStateComponent::RememberResolvedAttack(const FGuid& AttackId)
{
    if (!AttackId.IsValid())
    {
        return;
    }
    RecentAttackIds.Add(AttackId);
    if (RecentAttackIds.Num() > MaxRememberedAttackIds)
    {
        RecentAttackIds.RemoveAt(0);
    }
}
