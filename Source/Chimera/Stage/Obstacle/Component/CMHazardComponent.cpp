#include "Stage/Obstacle/Component/CMHazardComponent.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Core/CMPartStatusComponent.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Parts/Head/CMVisionComponent.h"
#include "Player/CMChimera.h"
#include "Player/CMControlBody.h"
#include "Player/CMPartSlotComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"

UCMHazardComponent::UCMHazardComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 장애물 액터에서 해석된 런타임 설정을 복사
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

void UCMHazardComponent::ConfigureHeadVisionEffect(
    const FCMHeadVisionObstacleEffectConfig& NewConfig)
{
    HeadVisionEffect = NewConfig;
    HeadVisionEffect.PeriodSeconds = FMath::Max(
        HeadVisionEffect.PeriodSeconds, 0.01f);
    HeadVisionEffect.StatusDuration = FMath::Max(
        HeadVisionEffect.StatusDuration, 0.01f);
    HeadVisionEffect.VisionAngleMultiplier = FMath::Clamp(
        HeadVisionEffect.VisionAngleMultiplier, 0.0f, 1.0f);
    HeadVisionEffect.VisionDistanceMultiplier = FMath::Clamp(
        HeadVisionEffect.VisionDistanceMultiplier, 0.0f, 1.0f);
    UpdateHeadVisionPeriodicTimer();
}

void UCMHazardComponent::ConfigureControlEffect(
    const FCMControlObstacleEffectConfig& NewConfig)
{
    ControlEffect = NewConfig;
    ControlEffect.Duration = FMath::Max(ControlEffect.Duration, 0.01f);
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
        const ECMPartSlotType PartType =
            PartActor->GetPartType_Implementation();
        const bool bSupportsPartStatus = PartType == ECMPartSlotType::Arm
            || PartType == ECMPartSlotType::Leg;
        if (!bPersistent || !bSupportsPartStatus
            || !PartEffect.StatusTag.IsValid())
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
    if (HeadVisionEffect.ApplicationPolicy
        == ECMHeadVisionEffectApplicationPolicy::WhileOverlapping)
    {
        for (const TPair<TWeakObjectPtr<ACMPartActorBase>, FTrackedPart>& Entry
            : TrackedParts)
        {
            ACMHeadPartActor* Head = Cast<ACMHeadPartActor>(Entry.Key.Get());
            if (!Head || Entry.Value.OverlapCount <= 0)
            {
                continue;
            }
            if (bHazardEnabled && HeadVisionEffect.bEnabled)
            {
                ApplyHeadVisionEffect(*Head);
            }
            else
            {
                RemoveHeadVisionEffect(*Head);
            }
        }
    }
    if (ControlEffect.ApplicationPolicy
        == ECMControlStatusApplicationPolicy::WhileOverlapping)
    {
        for (const TPair<TWeakObjectPtr<ACMControlBody>, int32>& Entry
            : ControlBodyOverlapCounts)
        {
            ACMControlBody* ControlBody = Entry.Key.Get();
            if (!ControlBody || Entry.Value <= 0)
            {
                continue;
            }
            if (bHazardEnabled)
            {
                ApplyControlEffect(*ControlBody);
            }
            else
            {
                RemoveControlEffect(*ControlBody);
            }
        }
    }
    UpdatePeriodicTimer();
    UpdateHeadVisionPeriodicTimer();
}

// 파츠는 Actor, 몸통은 Component 단위로 첫 진입만 효과 처리
void UCMHazardComponent::NotifyTargetEntered(
    AActor* TargetActor,
    UPrimitiveComponent* TargetComponent)
{
    AActor* Owner = GetOwner();
    ACMPartActorBase* PartActor = ResolveSupportedPart(
        TargetActor, TargetComponent);
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    if (PartActor)
    {
        FTrackedPart& Tracked = TrackedParts.FindOrAdd(PartActor);
        if (++Tracked.OverlapCount > 1)
        {
            return;
        }

        if (bHazardEnabled && PartEffect.bEnabled)
        {
            ApplyConfiguredEffect(*PartActor);
        }
        if (bHazardEnabled && HeadVisionEffect.bEnabled)
        {
            if (ACMHeadPartActor* Head = Cast<ACMHeadPartActor>(PartActor))
            {
                ApplyHeadVisionEffect(*Head);
            }
        }
        UpdatePeriodicTimer();
        UpdateHeadVisionPeriodicTimer();
        OnTargetEntered.Broadcast(PartActor);
        return;
    }

    ACMChimera* Chimera = Cast<ACMChimera>(TargetActor);
    const int32 SegmentIndex = ResolveBodySegment(
        TargetActor, TargetComponent);
    if (!Chimera || SegmentIndex == INDEX_NONE || !TargetComponent)
    {
        return;
    }

    FTrackedSegment& Tracked = TrackedSegments.FindOrAdd(TargetComponent);
    Tracked.Chimera = Chimera;
    Tracked.SegmentIndex = SegmentIndex;
    Tracked.ControlBodies.Reset();
    for (ACMControlBody* ControlBody
        : FindControlBodiesForSegment(SegmentIndex))
    {
        Tracked.ControlBodies.Add(ControlBody);
    }
    if (++Tracked.OverlapCount > 1)
    {
        return;
    }

    if (bHazardEnabled && PartEffect.bEnabled)
    {
        ApplyConfiguredDamage(*Chimera, SegmentIndex);
    }
    for (const TWeakObjectPtr<ACMControlBody>& WeakControlBody
        : Tracked.ControlBodies)
    {
        ACMControlBody* ControlBody = WeakControlBody.Get();
        if (!ControlBody)
        {
            continue;
        }
        int32& BodyOverlapCount = ControlBodyOverlapCounts.FindOrAdd(
            ControlBody);
        ++BodyOverlapCount;
        if (BodyOverlapCount == 1 && bHazardEnabled)
        {
            ApplyControlEffect(*ControlBody);
        }
    }
    UpdatePeriodicTimer();
    OnTargetEntered.Broadcast(Chimera);
}

// 마지막 콜리전 이탈에서 지속 상태를 제거하고 추적 종료
void UCMHazardComponent::NotifyTargetExited(
    AActor* TargetActor,
    UPrimitiveComponent* TargetComponent)
{
    AActor* Owner = GetOwner();
    ACMPartActorBase* PartActor = ResolveSupportedPart(
        TargetActor, TargetComponent);
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    if (PartActor)
    {
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

        if (HeadVisionEffect.ApplicationPolicy
            == ECMHeadVisionEffectApplicationPolicy::WhileOverlapping)
        {
            if (ACMHeadPartActor* Head = Cast<ACMHeadPartActor>(PartActor))
            {
                RemoveHeadVisionEffect(*Head);
            }
        }

        TrackedParts.Remove(PartActor);
        UpdatePeriodicTimer();
        UpdateHeadVisionPeriodicTimer();
        OnTargetExited.Broadcast(PartActor);
        return;
    }

    if (!TargetComponent)
    {
        return;
    }

    FTrackedSegment* Tracked = TrackedSegments.Find(TargetComponent);
    if (!Tracked || --Tracked->OverlapCount > 0)
    {
        return;
    }

    ACMChimera* Chimera = Tracked->Chimera.Get();
    for (const TWeakObjectPtr<ACMControlBody>& WeakControlBody
        : Tracked->ControlBodies)
    {
        ACMControlBody* ControlBody = WeakControlBody.Get();
        if (!ControlBody)
        {
            continue;
        }

        bool bLastControlBodyOverlap = false;
        if (int32* BodyOverlapCount = ControlBodyOverlapCounts.Find(ControlBody))
        {
            bLastControlBodyOverlap = --*BodyOverlapCount <= 0;
            if (bLastControlBodyOverlap)
            {
                ControlBodyOverlapCounts.Remove(ControlBody);
            }
        }
        if (bLastControlBodyOverlap
            && ControlEffect.ApplicationPolicy
                == ECMControlStatusApplicationPolicy::WhileOverlapping)
        {
            RemoveControlEffect(*ControlBody);
        }
    }
    TrackedSegments.Remove(TargetComponent);
    UpdatePeriodicTimer();
    if (Chimera)
    {
        OnTargetExited.Broadcast(Chimera);
    }
}

// 모든 파츠는 정확한 DamageHurtbox 접촉만 허용한다.
// 상태이상 적용 대상 제한은 ApplyConfiguredEffect에서 처리한다.
ACMPartActorBase* UCMHazardComponent::ResolveSupportedPart(
    AActor* TargetActor,
    const UPrimitiveComponent* TargetComponent) const
{
    ACMPartActorBase* PartActor = Cast<ACMPartActorBase>(TargetActor);
    if (!PartActor || TargetComponent != PartActor->GetDamageHurtbox())
    {
        return nullptr;
    }
    return PartActor;
}

int32 UCMHazardComponent::ResolveBodySegment(
    AActor* TargetActor,
    const UPrimitiveComponent* TargetComponent) const
{
    const ACMChimera* Chimera = Cast<ACMChimera>(TargetActor);
    return Chimera
        ? Chimera->GetSegmentIndexFromHurtbox(TargetComponent)
        : INDEX_NONE;
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

    const ECMPartSlotType PartType = PartActor.GetPartType_Implementation();
    const bool bSupportsPartStatus = PartType == ECMPartSlotType::Arm
        || PartType == ECMPartSlotType::Leg;
    if (!PartActor.IsAlive() || !bSupportsPartStatus
        || !PartEffect.StatusTag.IsValid())
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

TArray<ACMControlBody*> UCMHazardComponent::FindControlBodiesForSegment(
    int32 SegmentIndex) const
{
    TArray<ACMControlBody*> Result;
    const UWorld* World = GetWorld();
    if (!World)
    {
        return Result;
    }
    for (TActorIterator<ACMControlBody> It(World); It; ++It)
    {
        if (It->OwnsSegment(SegmentIndex))
        {
            Result.Add(*It);
        }
    }
    return Result;
}

void UCMHazardComponent::ApplyControlEffect(ACMControlBody& ControlBody)
{
    if (ControlEffect.StatusEffect == ECMControlObstacleStatusEffect::None)
    {
        return;
    }
    const float Duration = ControlEffect.ApplicationPolicy
            == ECMControlStatusApplicationPolicy::WhileOverlapping
        ? 0.0f : ControlEffect.Duration;
    if (ControlEffect.StatusEffect
        == ECMControlObstacleStatusEffect::Confused)
    {
        ControlBody.ApplyConfusion(Duration, this);
        return;
    }

    for (TActorIterator<ACMControlBody> It(GetWorld()); It; ++It)
    {
        if (*It != &ControlBody
            && It->GetAssignedControlCount() > 0
            && It->IsControlInputEnabled()
            && ControlBody.ApplyDelirium(**It, Duration, this))
        {
            return;
        }
    }
}

void UCMHazardComponent::RemoveControlEffect(ACMControlBody& ControlBody)
{
    if (ControlEffect.StatusEffect
        == ECMControlObstacleStatusEffect::Confused)
    {
        ControlBody.RemoveConfusion(this);
    }
    else if (ControlEffect.StatusEffect
        == ECMControlObstacleStatusEffect::Delirious)
    {
        ControlBody.RemoveDelirium(this);
    }
}

void UCMHazardComponent::ApplyHeadVisionEffect(ACMHeadPartActor& Head)
{
    UCMVisionComponent* Vision = Head.GetVisionComponent();
    if (!Vision)
    {
        return;
    }
    const float Duration = HeadVisionEffect.ApplicationPolicy
            == ECMHeadVisionEffectApplicationPolicy::WhileOverlapping
        ? 0.0f : HeadVisionEffect.StatusDuration;
    Vision->ApplyVisionReduction(
        Duration,
        HeadVisionEffect.VisionAngleMultiplier,
        HeadVisionEffect.VisionDistanceMultiplier,
        this);
}

void UCMHazardComponent::RemoveHeadVisionEffect(ACMHeadPartActor& Head)
{
    if (UCMVisionComponent* Vision = Head.GetVisionComponent())
    {
        Vision->RemoveVisionReduction(this);
    }
}

void UCMHazardComponent::ApplyConfiguredDamage(
    ACMChimera& Chimera,
    int32 SegmentIndex)
{
    const float Damage = PartEffect.ApplicationPolicy
            == ECMObstacleEffectApplicationPolicy::KillOnEnter
        ? TNumericLimits<float>::Max()
        : PartEffect.DamagePerApplication;
    if (Damage > 0.0f)
    {
        Chimera.ApplyDamageToSegment(SegmentIndex, Damage);
    }
}

// 주기 정책과 활성 추적 대상 유무에 따라 단일 반복 타이머 갱신
void UCMHazardComponent::UpdatePeriodicTimer()
{
    FTimerManager& TimerManager = GetWorld()->GetTimerManager();
    const bool bNeedsTimer = bHazardEnabled && PartEffect.bEnabled
        && PartEffect.ApplicationPolicy
            == ECMObstacleEffectApplicationPolicy::PeriodicWhileOverlapping
        && (!TrackedParts.IsEmpty() || !TrackedSegments.IsEmpty());
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

    for (auto It = TrackedSegments.CreateIterator(); It; ++It)
    {
        ACMChimera* Chimera = It.Value().Chimera.Get();
        if (!It.Key().IsValid()
            || !IsValid(Chimera)
            || It.Value().OverlapCount <= 0)
        {
            It.RemoveCurrent();
            continue;
        }
        ApplyConfiguredDamage(*Chimera, It.Value().SegmentIndex);
    }
    UpdatePeriodicTimer();
}

void UCMHazardComponent::UpdateHeadVisionPeriodicTimer()
{
    FTimerManager& TimerManager = GetWorld()->GetTimerManager();
    bool bHasTrackedHead = false;
    for (const TPair<TWeakObjectPtr<ACMPartActorBase>, FTrackedPart>& Entry
        : TrackedParts)
    {
        if (Entry.Value.OverlapCount > 0
            && Cast<ACMHeadPartActor>(Entry.Key.Get()))
        {
            bHasTrackedHead = true;
            break;
        }
    }
    const bool bNeedsTimer = bHazardEnabled && HeadVisionEffect.bEnabled
        && HeadVisionEffect.ApplicationPolicy
            == ECMHeadVisionEffectApplicationPolicy::PeriodicWhileOverlapping
        && bHasTrackedHead;
    if (!bNeedsTimer)
    {
        TimerManager.ClearTimer(HeadVisionPeriodicTimerHandle);
    }
    else if (!TimerManager.IsTimerActive(HeadVisionPeriodicTimerHandle))
    {
        TimerManager.SetTimer(
            HeadVisionPeriodicTimerHandle, this,
            &ThisClass::HandleHeadVisionPeriodicApplication,
            HeadVisionEffect.PeriodSeconds, true);
    }
}

void UCMHazardComponent::HandleHeadVisionPeriodicApplication()
{
    for (const TPair<TWeakObjectPtr<ACMPartActorBase>, FTrackedPart>& Entry
        : TrackedParts)
    {
        ACMHeadPartActor* Head = Cast<ACMHeadPartActor>(Entry.Key.Get());
        if (Head && Entry.Value.OverlapCount > 0)
        {
            ApplyHeadVisionEffect(*Head);
        }
    }
    UpdateHeadVisionPeriodicTimer();
}

void UCMHazardComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PeriodicTimerHandle);
        World->GetTimerManager().ClearTimer(HeadVisionPeriodicTimerHandle);
    }
    for (const TPair<TWeakObjectPtr<ACMPartActorBase>, FTrackedPart>& Entry
        : TrackedParts)
    {
        if (ACMHeadPartActor* Head = Cast<ACMHeadPartActor>(Entry.Key.Get()))
        {
            RemoveHeadVisionEffect(*Head);
        }
    }
    if (ControlEffect.ApplicationPolicy
        == ECMControlStatusApplicationPolicy::WhileOverlapping)
    {
        for (const TPair<TWeakObjectPtr<ACMControlBody>, int32>& Entry
            : ControlBodyOverlapCounts)
        {
            if (ACMControlBody* ControlBody = Entry.Key.Get())
            {
                RemoveControlEffect(*ControlBody);
            }
        }
    }
    Super::EndPlay(EndPlayReason);
}
