#include "Stage/Obstacle/Component/CMHazardComponent.h"

UCMHazardComponent::UCMHazardComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// Definition PDA에서 로드된 위험 효과 설정을 컴포넌트에 반영
void UCMHazardComponent::ConfigureHazard(
    TSubclassOf<UGameplayEffect> NewGameplayEffectClass,
    FGameplayTag NewEffectTag,
    ECMHazardApplicationMode NewApplicationMode,
    float NewPeriodSeconds)
{
    GameplayEffectClass = NewGameplayEffectClass;
    EffectTag = NewEffectTag;
    ApplicationMode = NewApplicationMode;
    PeriodSeconds = FMath::Max(NewPeriodSeconds, UE_SMALL_NUMBER);
}

// 유효한 진입 대상을 향후 피해 처리 확장 지점으로 전달
void UCMHazardComponent::NotifyTargetEntered(AActor* TargetActor)
{
    if (bHazardEnabled && IsValid(TargetActor) && GetOwner() && GetOwner()->HasAuthority())
    {
        OnTargetEntered.Broadcast(TargetActor);
    }
}

// 유효한 이탈 대상을 주기 효과 해제 확장 지점으로 전달
void UCMHazardComponent::NotifyTargetExited(AActor* TargetActor)
{
    if (IsValid(TargetActor) && GetOwner() && GetOwner()->HasAuthority())
    {
        OnTargetExited.Broadcast(TargetActor);
    }
}
