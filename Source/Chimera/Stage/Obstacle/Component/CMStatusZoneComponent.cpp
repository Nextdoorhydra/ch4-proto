#include "Stage/Obstacle/Component/CMStatusZoneComponent.h"

UCMStatusZoneComponent::UCMStatusZoneComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 유효한 진입 대상과 상태 태그를 효과 적용 확장 지점으로 전달
void UCMStatusZoneComponent::NotifyTargetEntered(AActor* TargetActor)
{
    if (bZoneEnabled && IsValid(TargetActor) && GetOwner() && GetOwner()->HasAuthority())
    {
        OnTargetEntered.Broadcast(TargetActor, StatusTag);
    }
}

// 유효한 이탈 대상과 상태 태그를 효과 해제 확장 지점으로 전달
void UCMStatusZoneComponent::NotifyTargetExited(AActor* TargetActor)
{
    if (IsValid(TargetActor) && GetOwner() && GetOwner()->HasAuthority())
    {
        OnTargetExited.Broadcast(TargetActor, StatusTag);
    }
}
