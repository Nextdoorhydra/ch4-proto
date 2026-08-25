#include "Stage/Obstacle/Component/CMHazardComponent.h"

#include "Engine/World.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Core/CMPartStatusComponent.h"
#include "Player/CMPartSlotComponent.h"
#include "TimerManager.h"

UCMHazardComponent::UCMHazardComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// Definition PDA의 값 타입 데이터는 별도 에셋 로드 없이 즉시 복사
void UCMHazardComponent::ConfigurePartEffect(
    const FCMPartObstacleEffectConfig& NewConfig)
{
    PartEffect = NewConfig;
    PartEffect.PeriodSeconds = FMath::Max(PartEffect.PeriodSeconds, 0.01f);
    PartEffect.DamagePerApplication = FMath::Max(
        PartEffect.DamagePerApplication, 0.0f);
    PartEffect.MovementMultiplier = FMath::Max(
        PartEffect.MovementMultiplier, 0.0f);
    UpdatePeriodicTimer();
}

// 비활성 중에도 오버랩 목록은 유지하고 실제 효과와 타이머만 정지
void UCMHazardComponent::SetHazardEnabled(bool bEnabled)
{
    if (bHazardEnabled == bEnabled)
    {
        return;
    }

    bHazardEnabled = bEnabled;
    const bool bPersistent = PartEffect.ApplicationPolicy
        == ECMObstacleEffectApplicationPolicy::WhileOverlapping;
    for (auto It = TrackedParts.CreateIterator(); It; ++It)
    {
        ACMPartActorBase* PartActor = It.Key().Get();
        if (!IsValid(PartActor) || It.Value().OverlapCount <= 0)
        {
            It.RemoveCurrent();
            continue;
        }
        if (!bPersistent || !PartEffect.StatusTag.IsValid())
        {
            continue;
        }

        if (UCMPartStatusComponent* Status =
                PartActor->GetPartStatusComponent())
        {
            if (bHazardEnabled && PartEffect.bEnabled)
            {
                Status->ApplyStatus(
                    PartEffect.StatusTag, 0.0f,
                    PartEffect.MovementMultiplier,
                    PartEffect.bBlocksAbility, this);
            }
            else
            {
                Status->RemoveStatus(PartEffect.StatusTag, this);
            }
        }
    }
    UpdatePeriodicTimer();
}

// 첫 콜리전 진입만 효과로 처리해 한 파츠의 복수 콜리전 중복 방지
void UCMHazardComponent::NotifyTargetEntered(AActor* TargetActor)
{
    AActor* Owner = GetOwner();
    ACMPartActorBase* PartActor = ResolveSupportedPart(TargetActor);
    if (!Owner || !Owner->HasAuthority() || !PartActor)
    {
        return;
    }

    FTrackedPart& Tracked = TrackedParts.FindOrAdd(PartActor);
    if (++Tracked.OverlapCount > 1)
    {
        return;
    }

    if (bHazardEnabled && PartEffect.bEnabled
        && PartEffect.ApplicationPolicy
            != ECMObstacleEffectApplicationPolicy::PeriodicWhileOverlapping)
    {
        ApplyConfiguredEffect(*PartActor);
    }
    UpdatePeriodicTimer();
    OnTargetEntered.Broadcast(PartActor);
}

// 마지막 콜리전 이탈에서 지속 상태를 제거하고 추적 종료
void UCMHazardComponent::NotifyTargetExited(AActor* TargetActor)
{
    AActor* Owner = GetOwner();
    ACMPartActorBase* PartActor = ResolveSupportedPart(TargetActor);
    if (!Owner || !Owner->HasAuthority() || !PartActor)
    {
        return;
    }

    FTrackedPart* Tracked = TrackedParts.Find(PartActor);
    if (!Tracked || --Tracked->OverlapCount > 0)
    {
        return;
    }

    if (PartEffect.ApplicationPolicy
            == ECMObstacleEffectApplicationPolicy::WhileOverlapping
        && PartEffect.StatusTag.IsValid())
    {
        if (UCMPartStatusComponent* Status =
                PartActor->GetPartStatusComponent())
        {
            Status->RemoveStatus(PartEffect.StatusTag, this);
        }
    }

    TrackedParts.Remove(PartActor);
    UpdatePeriodicTimer();
    OnTargetExited.Broadcast(PartActor);
}

// 현재 정책은 바닥에 직접 닿는 팔과 다리만 허용
ACMPartActorBase* UCMHazardComponent::ResolveSupportedPart(
    AActor* TargetActor) const
{
    ACMPartActorBase* PartActor = Cast<ACMPartActorBase>(TargetActor);
    if (!PartActor)
    {
        return nullptr;
    }
    const ECMPartSlotType PartType = PartActor->GetPartType_Implementation();
    return PartType == ECMPartSlotType::Arm
        || PartType == ECMPartSlotType::Leg
        ? PartActor : nullptr;
}

// 기존 파츠 내구도 API와 PartStatus를 조합해 한 번의 효과 적용
void UCMHazardComponent::ApplyConfiguredEffect(
    ACMPartActorBase& PartActor)
{
    if (PartEffect.ApplicationPolicy
        == ECMObstacleEffectApplicationPolicy::KillOnEnter)
    {
        PartActor.ApplyPartDamage(PartActor.GetHealth());
    }
    else if (PartEffect.DamagePerApplication > 0.0f)
    {
        PartActor.ApplyPartDamage(PartEffect.DamagePerApplication);
    }

    if (!PartActor.IsAlive() || !PartEffect.StatusTag.IsValid())
    {
        return;
    }
    if (UCMPartStatusComponent* Status = PartActor.GetPartStatusComponent())
    {
        const float Duration = PartEffect.ApplicationPolicy
                == ECMObstacleEffectApplicationPolicy::WhileOverlapping
            ? 0.0f : PartEffect.StatusDuration;
        Status->ApplyStatus(
            PartEffect.StatusTag, Duration,
            PartEffect.MovementMultiplier,
            PartEffect.bBlocksAbility, this);
    }
}

// 주기 정책과 활성 추적 대상 유무에 따라 단일 반복 타이머 갱신
void UCMHazardComponent::UpdatePeriodicTimer()
{
    FTimerManager& TimerManager = GetWorld()->GetTimerManager();
    const bool bNeedsTimer = bHazardEnabled && PartEffect.bEnabled
        && PartEffect.ApplicationPolicy
            == ECMObstacleEffectApplicationPolicy::PeriodicWhileOverlapping
        && !TrackedParts.IsEmpty();
    if (!bNeedsTimer)
    {
        TimerManager.ClearTimer(PeriodicTimerHandle);
        return;
    }
    if (!TimerManager.IsTimerActive(PeriodicTimerHandle))
    {
        TimerManager.SetTimer(
            PeriodicTimerHandle, this,
            &ThisClass::HandlePeriodicApplication,
            PartEffect.PeriodSeconds, true);
    }
}

// 한 번의 장판 펄스에서 현재 접촉 중인 모든 유효 파츠에 적용
void UCMHazardComponent::HandlePeriodicApplication()
{
    for (auto It = TrackedParts.CreateIterator(); It; ++It)
    {
        ACMPartActorBase* PartActor = It.Key().Get();
        if (!IsValid(PartActor) || It.Value().OverlapCount <= 0)
        {
            It.RemoveCurrent();
            continue;
        }
        ApplyConfiguredEffect(*PartActor);
    }
    UpdatePeriodicTimer();
}
