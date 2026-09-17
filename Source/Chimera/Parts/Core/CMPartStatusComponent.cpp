#include "Parts/Core/CMPartStatusComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UCMPartStatusComponent::UCMPartStatusComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCMPartStatusComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, ActiveStatusTags);
    DOREPLIFETIME(ThisClass, RuntimeMovementMultiplier);
    DOREPLIFETIME(ThisClass, bAbilityBlocked);
}

// 같은 발생원과 태그의 상태를 찾거나 새로 만들고 만료 타이머 설정
void UCMPartStatusComponent::ApplyStatus(
    FGameplayTag StatusTag,
    float Duration,
    float MovementMultiplier,
    bool bBlocksAbility,
    UObject* Source
)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !StatusTag.IsValid())
    {
        return;
    }

    FActivePartStatus* Status = ActiveStatuses.FindByPredicate(
        [StatusTag, Source](const FActivePartStatus& Candidate)
        {
            return Candidate.StatusTag.MatchesTagExact(StatusTag)
                && Candidate.Source.Get() == Source;
        });

    if (!Status)
    {
        FActivePartStatus& NewStatus = ActiveStatuses.AddDefaulted_GetRef();
        NewStatus.Handle = NextStatusHandle++;
        NewStatus.StatusTag = StatusTag;
        NewStatus.Source = Source;
        Status = &NewStatus;
    }

    Status->MovementMultiplier = FMath::Max(MovementMultiplier, 0.0f);
    Status->bBlocksAbility = bBlocksAbility;

    GetWorld()->GetTimerManager().ClearTimer(Status->ExpirationTimer);
    if (Duration > 0.0f)
    {
        FTimerDelegate ExpirationDelegate;
        ExpirationDelegate.BindUObject(
            this,
            &ThisClass::HandleStatusExpired,
            Status->Handle);
        GetWorld()->GetTimerManager().SetTimer(
            Status->ExpirationTimer,
            ExpirationDelegate,
            Duration,
            false);
    }

    RecalculateAggregates();
}

// 지정한 장판이나 장애물이 부여한 특정 상태만 제거
void UCMPartStatusComponent::RemoveStatus(
    FGameplayTag StatusTag,
    UObject* Source
)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    FTimerManager& TimerManager = GetWorld()->GetTimerManager();
    const int32 RemovedCount = ActiveStatuses.RemoveAll(
        [&TimerManager, StatusTag, Source](FActivePartStatus& Status)
        {
            const bool bMatches =
                Status.StatusTag.MatchesTagExact(StatusTag)
                && Status.Source.Get() == Source;
            if (bMatches)
            {
                TimerManager.ClearTimer(Status.ExpirationTimer);
            }
            return bMatches;
        });

    if (RemovedCount > 0)
    {
        RecalculateAggregates();
    }
}

// 파츠에 남은 모든 상태 타이머를 취소하고 기본 상태 복원
void UCMPartStatusComponent::ClearAllStatuses()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    FTimerManager& TimerManager = GetWorld()->GetTimerManager();
    for (FActivePartStatus& Status : ActiveStatuses)
    {
        TimerManager.ClearTimer(Status.ExpirationTimer);
    }
    ActiveStatuses.Reset();
    RecalculateAggregates();
}

bool UCMPartStatusComponent::HasStatus(FGameplayTag StatusTag) const
{
    return ActiveStatusTags.HasTagExact(StatusTag);
}

// 예약된 Handle의 상태가 아직 존재할 때만 제거
void UCMPartStatusComponent::HandleStatusExpired(int32 StatusHandle)
{
    const int32 RemovedCount = ActiveStatuses.RemoveAll(
        [StatusHandle](const FActivePartStatus& Status)
        {
            return Status.Handle == StatusHandle;
        });
    if (RemovedCount > 0)
    {
        RecalculateAggregates();
    }
}

// 여러 상태의 이동 배율은 곱하고 하나라도 행동 차단이면 파츠 GA 차단
void UCMPartStatusComponent::RecalculateAggregates()
{
    ActiveStatusTags.Reset();
    RuntimeMovementMultiplier = 1.0f;
    bAbilityBlocked = false;

    for (const FActivePartStatus& Status : ActiveStatuses)
    {
        ActiveStatusTags.AddTag(Status.StatusTag);
        RuntimeMovementMultiplier *= Status.MovementMultiplier;
        bAbilityBlocked |= Status.bBlocksAbility;
    }

    OnStatusChanged.Broadcast();
    if (AActor* Owner = GetOwner())
    {
        Owner->ForceNetUpdate();
    }
}

void UCMPartStatusComponent::OnRep_StatusState()
{
    OnStatusChanged.Broadcast();
}
