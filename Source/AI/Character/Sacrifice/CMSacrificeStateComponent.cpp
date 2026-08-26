#include "Character/Sacrifice/CMSacrificeStateComponent.h"

#include "Character/Sacrifice/CMSacrificeCharacter.h"
#include "Character/Sacrifice/CMSacrificeRules.h"
#include "Engine/World.h"
#include "Gore/CMDismembermentComponent.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMSacrifice, Log, All);

namespace
{
    constexpr int32 MaxRememberedAttackIds = 32;
}

UCMSacrificeStateComponent::UCMSacrificeStateComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCMSacrificeStateComponent::BeginPlay()
{
    Super::BeginPlay();
    DismembermentComponent = GetOwner()
        ? GetOwner()->FindComponentByClass<UCMDismembermentComponent>()
        : nullptr;
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ValidateRewards();
    }
}

void UCMSacrificeStateComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BleedTimerHandle);
    }
    Super::EndPlay(EndPlayReason);
}

void UCMSacrificeStateComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCMSacrificeStateComponent, MissingPartMask);
    DOREPLIFETIME(UCMSacrificeStateComponent, bDead);
    DOREPLIFETIME(UCMSacrificeStateComponent, bBleeding);
    DOREPLIFETIME(UCMSacrificeStateComponent, BleedEndTime);
    DOREPLIFETIME(UCMSacrificeStateComponent, DroppedRewardMask);
}

int32 UCMSacrificeStateComponent::ResolveDismembermentHit(
    const FCMDismembermentHitRequest& Request
)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || bDead
        || !DismembermentComponent
        || WasAttackAlreadyResolved(Request.AttackId))
    {
        return 0;
    }
    RememberResolvedAttack(Request.AttackId);

    const TArray<ECMBodyPart> AvailableParts = GetAttachedBodyParts();
    FRandomStream RandomStream(FMath::Rand());
    const ECMBodyPart SelectedPart = FCMSacrificeRules::SelectRandomPart(
        AvailableParts,
        RandomStream);
    if (SelectedPart == ECMBodyPart::None)
    {
        return 0;
    }

    const int32 PreviousMissingCount = GetMissingPartCount();
    const FVector Impulse = Request.ImpactDirection.GetSafeNormal(
        SMALL_NUMBER,
        FVector::ForwardVector) * DismembermentImpulse;
    const FCMSacrificeRewardPart* Reward = FindReward(SelectedPart);
    TSubclassOf<ACMPartActorBase> RewardClass =
        Reward && !HasRewardDropped(SelectedPart)
            ? Reward->PartClass
            : nullptr;
    const bool bSevered = RewardClass
        ? DismembermentComponent->SeverBodyPartWithReward(
            SelectedPart,
            Request.ImpactPoint,
            Impulse,
            RewardClass)
        : DismembermentComponent->SeverBodyPart(
            SelectedPart,
            Request.ImpactPoint,
            Impulse);
    if (!bSevered)
    {
        return 0;
    }

    const uint8 SelectedPartBit =
        FCMSacrificeRules::GetBodyPartBit(SelectedPart);
    MissingPartMask |= SelectedPartBit;
    if (RewardClass)
    {
        DroppedRewardMask |= SelectedPartBit;
    }
    OnBodyPartSevered.Broadcast(SelectedPart);
    OnMissingPartsChanged.Broadcast(GetMissingPartCount());
    Owner->ForceNetUpdate();

    if (SelectedPart == ECMBodyPart::Head)
    {
        Die();
        return 1;
    }

    StartOrUpdateBleeding(PreviousMissingCount, 1);
    return 1;
}

bool UCMSacrificeStateComponent::HasBodyPart(
    const ECMBodyPart BodyPart
) const
{
    const uint8 Bit = FCMSacrificeRules::GetBodyPartBit(BodyPart);
    return Bit != 0 && (MissingPartMask & Bit) == 0
        && FCMSacrificeRules::GetSeverableBodyParts().Contains(BodyPart);
}

int32 UCMSacrificeStateComponent::GetMissingPartCount() const
{
    int32 Count = 0;
    for (const ECMBodyPart BodyPart
        : FCMSacrificeRules::GetSeverableBodyParts())
    {
        Count += HasBodyPart(BodyPart) ? 0 : 1;
    }
    return Count;
}

TArray<ECMBodyPart> UCMSacrificeStateComponent::GetAttachedBodyParts() const
{
    TArray<ECMBodyPart> Result;
    for (const ECMBodyPart BodyPart
        : FCMSacrificeRules::GetSeverableBodyParts())
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
    return bBleeding && World
        ? FMath::Max(BleedEndTime - World->GetTimeSeconds(), 0.0f)
        : 0.0f;
}

bool UCMSacrificeStateComponent::HasRewardForBodyPart(
    const ECMBodyPart BodyPart
) const
{
    return FindReward(BodyPart) != nullptr;
}

bool UCMSacrificeStateComponent::HasRewardDropped(
    const ECMBodyPart BodyPart
) const
{
    const uint8 Bit = FCMSacrificeRules::GetBodyPartBit(BodyPart);
    return Bit != 0 && (DroppedRewardMask & Bit) != 0;
}

int32 UCMSacrificeStateComponent::GetRemainingRewardCount() const
{
    const ACMSacrificeCharacter* Character =
        Cast<ACMSacrificeCharacter>(GetOwner());
    if (!Character)
    {
        return 0;
    }

    TSet<ECMBodyPart> UniqueParts;
    for (const FCMSacrificeRewardPart& Reward : Character->GetRewardParts())
    {
        if (Reward.PartClass
            && FCMSacrificeRules::GetSeverableBodyParts().Contains(
                Reward.BodyPart)
            && !HasRewardDropped(Reward.BodyPart))
        {
            UniqueParts.Add(Reward.BodyPart);
        }
    }
    return UniqueParts.Num();
}

void UCMSacrificeStateComponent::OnRep_MissingPartMask(
    const uint8 PreviousMask
)
{
    for (const ECMBodyPart BodyPart
        : FCMSacrificeRules::GetSeverableBodyParts())
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

void UCMSacrificeStateComponent::StartOrUpdateBleeding(
    const int32 PreviousMissingCount,
    const int32 NewlyMissingCount
)
{
    UWorld* World = GetWorld();
    if (!World || bDead || NewlyMissingCount <= 0)
    {
        return;
    }

    const float RemainingSeconds = PreviousMissingCount == 0
        ? FCMSacrificeRules::GetInitialBleedDuration(
            GetMissingPartCount())
        : GetBleedTimeRemaining()
            - FCMSacrificeRules::GetAdditionalBleedReduction(
                NewlyMissingCount);
    if (RemainingSeconds <= 0.0f)
    {
        Die();
        return;
    }

    const bool bWasBleeding = bBleeding;
    bBleeding = true;
    BleedEndTime = World->GetTimeSeconds() + RemainingSeconds;
    World->GetTimerManager().SetTimer(
        BleedTimerHandle,
        this,
        &ThisClass::HandleBleedExpired,
        RemainingSeconds,
        false);
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
    DropRemainingRewards();
    if (DismembermentComponent)
    {
        DismembermentComponent->EnterCorpseRagdoll();
    }
    OnSacrificeDied.Broadcast();
    GetOwner()->ForceNetUpdate();
}

void UCMSacrificeStateComponent::DropRemainingRewards()
{
    ACMSacrificeCharacter* Character =
        Cast<ACMSacrificeCharacter>(GetOwner());
    if (!Character || !DismembermentComponent)
    {
        return;
    }

    bool bMissingPartsChanged = false;
    TSet<ECMBodyPart> ProcessedParts;
    for (const FCMSacrificeRewardPart& Reward : Character->GetRewardParts())
    {
        if (ProcessedParts.Contains(Reward.BodyPart)
            || HasRewardDropped(Reward.BodyPart)
            || !HasBodyPart(Reward.BodyPart))
        {
            continue;
        }
        ProcessedParts.Add(Reward.BodyPart);

        TSubclassOf<ACMPartActorBase> RewardClass = Reward.PartClass;
        if (!RewardClass)
        {
            continue;
        }

        const FVector Impulse = FMath::VRand().GetSafeNormal(
            SMALL_NUMBER,
            FVector::UpVector) * DismembermentImpulse;
        if (!DismembermentComponent->SeverBodyPartWithReward(
                Reward.BodyPart,
                FVector::ZeroVector,
                Impulse,
                RewardClass))
        {
            continue;
        }

        const uint8 Bit = FCMSacrificeRules::GetBodyPartBit(Reward.BodyPart);
        MissingPartMask |= Bit;
        DroppedRewardMask |= Bit;
        bMissingPartsChanged = true;
        OnBodyPartSevered.Broadcast(Reward.BodyPart);
    }

    if (bMissingPartsChanged)
    {
        OnMissingPartsChanged.Broadcast(GetMissingPartCount());
        Character->ForceNetUpdate();
    }
}

void UCMSacrificeStateComponent::ValidateRewards() const
{
    const ACMSacrificeCharacter* Character =
        Cast<ACMSacrificeCharacter>(GetOwner());
    if (!Character)
    {
        return;
    }

    TSet<ECMBodyPart> SeenParts;
    for (const FCMSacrificeRewardPart& Reward : Character->GetRewardParts())
    {
        const bool bValidBodyPart =
            FCMSacrificeRules::GetBodyPartBit(Reward.BodyPart) != 0
            && FCMSacrificeRules::GetSeverableBodyParts().Contains(
                Reward.BodyPart);
        if (!bValidBodyPart || !Reward.PartClass)
        {
            UE_LOG(LogCMSacrifice, Warning,
                TEXT("[Sacrifice Reward] '%s' has an invalid reward entry for body part %d."),
                *GetNameSafe(Character),
                static_cast<int32>(Reward.BodyPart));
            continue;
        }
        if (SeenParts.Contains(Reward.BodyPart))
        {
            UE_LOG(LogCMSacrifice, Warning,
                TEXT("[Sacrifice Reward] '%s' has duplicate reward entries for body part %d; only the first is used."),
                *GetNameSafe(Character),
                static_cast<int32>(Reward.BodyPart));
            continue;
        }
        SeenParts.Add(Reward.BodyPart);
    }
}

const FCMSacrificeRewardPart* UCMSacrificeStateComponent::FindReward(
    const ECMBodyPart BodyPart
) const
{
    const ACMSacrificeCharacter* Character =
        Cast<ACMSacrificeCharacter>(GetOwner());
    return Character
        ? Character->GetRewardParts().FindByPredicate(
            [BodyPart](const FCMSacrificeRewardPart& Reward)
            {
                return Reward.BodyPart == BodyPart
                    && Reward.PartClass;
            })
        : nullptr;
}

bool UCMSacrificeStateComponent::WasAttackAlreadyResolved(
    const FGuid& AttackId
) const
{
    return AttackId.IsValid() && RecentAttackIds.Contains(AttackId);
}

void UCMSacrificeStateComponent::RememberResolvedAttack(
    const FGuid& AttackId
)
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
