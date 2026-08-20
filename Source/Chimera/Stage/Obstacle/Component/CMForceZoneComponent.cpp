#include "Stage/Obstacle/Component/CMForceZoneComponent.h"

UCMForceZoneComponent::UCMForceZoneComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 소유 액터 기준 방향을 월드 방향으로 변환
FVector UCMForceZoneComponent::GetWorldForceDirection() const
{
    const AActor* Owner = GetOwner();
    return Owner ? Owner->GetActorTransform().TransformVectorNoScale(LocalDirection).GetSafeNormal() : FVector::ZeroVector;
}

// 유효한 진입 대상과 힘 방향을 물리 처리 확장 지점으로 전달
void UCMForceZoneComponent::NotifyTargetEntered(AActor* TargetActor)
{
    if (bZoneEnabled && IsValid(TargetActor) && GetOwner() && GetOwner()->HasAuthority())
    {
        OnTargetEntered.Broadcast(TargetActor, GetWorldForceDirection());
    }
}

// 구역에서 이탈한 대상을 힘 해제 확장 지점으로 전달
void UCMForceZoneComponent::NotifyTargetExited(AActor* TargetActor)
{
    if (IsValid(TargetActor) && GetOwner() && GetOwner()->HasAuthority())
    {
        OnTargetExited.Broadcast(TargetActor, GetWorldForceDirection());
    }
}
