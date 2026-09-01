#include "Stage/Obstacle/Component/CMChimeraEffectZoneComponent.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Player/CMChimera.h"
#include "TimerManager.h"

namespace CMObstacleEffectDataNames
{
    const FName Duration(TEXT("Data.Obstacle.Duration"));
    const FName PrimaryStatusValue(TEXT("Data.Obstacle.Status.Primary"));
    const FName SecondaryStatusValue(TEXT("Data.Obstacle.Status.Secondary"));
}

UCMChimeraEffectZoneComponent::UCMChimeraEffectZoneComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 준비된 GE와 적용 정책을 저장하고 실행 중 타이머 상태 갱신
void UCMChimeraEffectZoneComponent::ConfigureChimeraEffect(
    const FCMChimeraObstacleEffectConfig& NewConfig,
    TSubclassOf<UGameplayEffect> NewGameplayEffectClass)
{
    ChimeraEffect = NewConfig;
    ChimeraEffect.PeriodSeconds = FMath::Max(
        ChimeraEffect.PeriodSeconds, 0.01f);
    GameplayEffectClass = NewGameplayEffectClass;
    UpdatePeriodicTimer();
}

// 첫 몸통 마디 진입에서 키메라 단위 적용 정책 실행
void UCMChimeraEffectZoneComponent::NotifyTargetEntered(AActor* TargetActor)
{
    AActor* Owner = GetOwner();
    ACMChimera* Chimera = Cast<ACMChimera>(TargetActor);
    if (!Owner || !Owner->HasAuthority() || !Chimera)
    {
        return;
    }

    FTrackedChimera& Tracked = TrackedChimeras.FindOrAdd(Chimera);
    if (++Tracked.OverlapCount > 1 || !bZoneEnabled
        || !ChimeraEffect.bEnabled || !GameplayEffectClass)
    {
        return;
    }

    if (ChimeraEffect.ApplicationPolicy
        == ECMObstacleEffectApplicationPolicy::WhileOverlapping)
    {
        ApplyEffect(*Chimera, &Tracked.PersistentEffectHandle);
    }
    else
    {
        ApplyEffect(*Chimera);
    }
    UpdatePeriodicTimer();
}

// 마지막 마디가 이탈하면 WhileOverlapping GE만 정확한 Handle로 제거
void UCMChimeraEffectZoneComponent::NotifyTargetExited(AActor* TargetActor)
{
    AActor* Owner = GetOwner();
    ACMChimera* Chimera = Cast<ACMChimera>(TargetActor);
    if (!Owner || !Owner->HasAuthority() || !Chimera)
    {
        return;
    }

    FTrackedChimera* Tracked = TrackedChimeras.Find(Chimera);
    if (!Tracked || --Tracked->OverlapCount > 0)
    {
        return;
    }

    RemovePersistentEffect(*Chimera, *Tracked);
    TrackedChimeras.Remove(Chimera);
    UpdatePeriodicTimer();
}

// 장애물 비활성화에서는 접촉 목록을 유지하되 지속 GE와 타이머 정지
void UCMChimeraEffectZoneComponent::SetZoneEnabled(bool bEnabled)
{
    if (bZoneEnabled == bEnabled)
    {
        return;
    }
    bZoneEnabled = bEnabled;

    for (auto It = TrackedChimeras.CreateIterator(); It; ++It)
    {
        ACMChimera* Chimera = It.Key().Get();
        if (!IsValid(Chimera) || It.Value().OverlapCount <= 0)
        {
            It.RemoveCurrent();
            continue;
        }

        if (ChimeraEffect.ApplicationPolicy
            == ECMObstacleEffectApplicationPolicy::WhileOverlapping)
        {
            if (bZoneEnabled && ChimeraEffect.bEnabled
                && GameplayEffectClass)
            {
                ApplyEffect(*Chimera, &It.Value().PersistentEffectHandle);
            }
            else
            {
                RemovePersistentEffect(*Chimera, It.Value());
            }
        }
    }
    UpdatePeriodicTimer();
}

// 장애물 액터를 SourceObject로 기록한 Spec을 키메라 공용 ASC에 적용
bool UCMChimeraEffectZoneComponent::ApplyEffect(
    ACMChimera& Chimera,
    FActiveGameplayEffectHandle* OutHandle) const
{
    UAbilitySystemComponent* ASC = Chimera.GetAbilitySystemComponent();
    if (!ASC || !GameplayEffectClass)
    {
        return false;
    }

    FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
    Context.AddSourceObject(GetOwner());
    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
        GameplayEffectClass, 1.0f, Context);
    if (!Spec.IsValid())
    {
        return false;
    }

    Spec.Data->SetSetByCallerMagnitude(
        CMObstacleEffectDataNames::Duration,
        ChimeraEffect.StatusDuration);
    Spec.Data->SetSetByCallerMagnitude(
        CMObstacleEffectDataNames::PrimaryStatusValue,
        ChimeraEffect.PrimaryStatusValue);
    Spec.Data->SetSetByCallerMagnitude(
        CMObstacleEffectDataNames::SecondaryStatusValue,
        ChimeraEffect.SecondaryStatusValue);

    const FActiveGameplayEffectHandle AppliedHandle =
        ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    if (OutHandle)
    {
        *OutHandle = AppliedHandle;
    }
    return AppliedHandle.IsValid();
}

// 저장한 활성 GE만 제거해 다른 장애물이 부여한 같은 GE 보존
void UCMChimeraEffectZoneComponent::RemovePersistentEffect(
    ACMChimera& Chimera,
    FTrackedChimera& Tracked) const
{
    if (!Tracked.PersistentEffectHandle.IsValid())
    {
        return;
    }
    if (UAbilitySystemComponent* ASC = Chimera.GetAbilitySystemComponent())
    {
        ASC->RemoveActiveGameplayEffect(Tracked.PersistentEffectHandle);
    }
    Tracked.PersistentEffectHandle.Invalidate();
}

// 주기 GE 정책과 추적 대상에 따라 하나의 반복 타이머 유지
void UCMChimeraEffectZoneComponent::UpdatePeriodicTimer()
{
    FTimerManager& TimerManager = GetWorld()->GetTimerManager();
    const bool bNeedsTimer = bZoneEnabled && ChimeraEffect.bEnabled
        && GameplayEffectClass
        && ChimeraEffect.ApplicationPolicy
            == ECMObstacleEffectApplicationPolicy::PeriodicWhileOverlapping
        && !TrackedChimeras.IsEmpty();
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
            ChimeraEffect.PeriodSeconds, true);
    }
}

// 한 번의 펄스에서 현재 접촉한 모든 키메라 공용 ASC에 GE 적용
void UCMChimeraEffectZoneComponent::HandlePeriodicApplication()
{
    for (auto It = TrackedChimeras.CreateIterator(); It; ++It)
    {
        ACMChimera* Chimera = It.Key().Get();
        if (!IsValid(Chimera) || It.Value().OverlapCount <= 0)
        {
            It.RemoveCurrent();
            continue;
        }
        ApplyEffect(*Chimera);
    }
    UpdatePeriodicTimer();
}
