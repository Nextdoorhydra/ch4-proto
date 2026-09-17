#include "Parts/Combat/CMBattleComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraBattle, Log, All);

namespace
{
    constexpr int32 MaxRememberedAttackIds = 32;
}

UCMBattleComponent::UCMBattleComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCMBattleComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ParryWindowTimerHandle);
    }
    Super::EndPlay(EndPlayReason);
}

void UCMBattleComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCMBattleComponent, bParryWindowActive);
}

ECMPartHitResult UCMBattleComponent::ResolveHit(
    const FCMPartHitPayload& HitPayload
)
{
    AActor* Owner = GetOwner();
    ACMPartActorBase* PartActor = Cast<ACMPartActorBase>(Owner);
    if (!PartActor || !PartActor->HasAuthority() || !PartActor->IsAlive()
        || HitPayload.Damage <= 0.0f
        || WasAttackAlreadyResolved(HitPayload.AttackId))
    {
        return ECMPartHitResult::Invalid;
    }

    RememberResolvedAttack(HitPayload.AttackId);

    if (bParryWindowActive)
    {
        OnParrySucceeded.Broadcast(HitPayload);
        OnHitResolved.Broadcast(ECMPartHitResult::Parried, HitPayload);

        UE_LOG(LogChimeraBattle, Log,
            TEXT("[Part Parry] Part=%s Attacker=%s AttackId=%s"),
            *GetNameSafe(Owner),
            *GetNameSafe(HitPayload.Attacker),
            *HitPayload.AttackId.ToString());
        return ECMPartHitResult::Parried;
    }

    const float PreviousHealth = PartActor->GetHealth();
    const FVector HitLocation = HitPayload.ImpactPoint.IsNearlyZero()
        ? PartActor->GetActorLocation()
        : HitPayload.ImpactPoint;
    const FVector SurfaceNormal = HitPayload.ImpactNormal.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    const bool bDied = PartActor->ApplyPartDamageAtHit(
        HitPayload.Damage,
        HitLocation,
        SurfaceNormal,
        SurfaceNormal);
    const ECMPartHitResult Result = bDied
        ? ECMPartHitResult::Dead
        : ECMPartHitResult::Damaged;

    OnHitResolved.Broadcast(Result, HitPayload);

    UE_LOG(LogChimeraBattle, Log,
        TEXT("[Part Hit] Part=%s Damage=%.1f Health=%.1f->%.1f Result=%d"),
        *GetNameSafe(Owner),
        HitPayload.Damage,
        PreviousHealth,
        PartActor->GetHealth(),
        static_cast<int32>(Result));
    return Result;
}

bool UCMBattleComponent::BeginParryWindow(float Duration)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    const ACMPartActorBase* PartActor = Cast<ACMPartActorBase>(Owner);
    if (!PartActor || !PartActor->HasAuthority() || !World
        || !PartActor->IsOperational()
        || Duration <= 0.0f)
    {
        return false;
    }

    World->GetTimerManager().ClearTimer(ParryWindowTimerHandle);
    bParryWindowActive = true;
    OnParryWindowChanged.Broadcast(true);
    World->GetTimerManager().SetTimer(
        ParryWindowTimerHandle,
        this,
        &UCMBattleComponent::EndParryWindow,
        Duration,
        false
    );
    Owner->ForceNetUpdate();

    UE_LOG(LogChimeraBattle, Verbose,
        TEXT("[Parry Window Open] Part=%s Duration=%.3f"),
        *GetNameSafe(Owner), Duration);
    return true;
}

void UCMBattleComponent::EndParryWindow()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !bParryWindowActive)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ParryWindowTimerHandle);
    }
    bParryWindowActive = false;
    OnParryWindowChanged.Broadcast(false);
    Owner->ForceNetUpdate();
}

bool UCMBattleComponent::IsParryWindowActive() const
{
    return bParryWindowActive;
}

void UCMBattleComponent::OnRep_ParryWindowActive()
{
    OnParryWindowChanged.Broadcast(bParryWindowActive);
}

bool UCMBattleComponent::WasAttackAlreadyResolved(
    const FGuid& AttackId
) const
{
    return AttackId.IsValid() && RecentResolvedAttackIds.Contains(AttackId);
}

void UCMBattleComponent::RememberResolvedAttack(const FGuid& AttackId)
{
    if (!AttackId.IsValid())
    {
        return;
    }

    RecentResolvedAttackIds.Add(AttackId);
    if (RecentResolvedAttackIds.Num() > MaxRememberedAttackIds)
    {
        RecentResolvedAttackIds.RemoveAt(0);
    }
}
