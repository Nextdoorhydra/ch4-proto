#include "Stage/Obstacle/Component/CMForceZoneComponent.h"

#include "Player/CMChimera.h"

UCMForceZoneComponent::UCMForceZoneComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// 구역 안에 있는 키메라에 설정된 하나의 총 Force를 서버에서 지속 적용
void UCMForceZoneComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const AActor* Owner = GetOwner();
    if (!bZoneEnabled || !Owner || !Owner->HasAuthority())
    {
        return;
    }

    const FVector Force = GetWorldForceDirection() * ForceStrength;
    if (Force.IsNearlyZero())
    {
        return;
    }

    for (auto It = OverlappingChimeras.CreateIterator(); It; ++It)
    {
        ACMChimera* Chimera = It.Key().Get();
        if (!IsValid(Chimera) || It.Value() <= 0)
        {
            It.RemoveCurrent();
            continue;
        }

        Chimera->ApplyEnvironmentalForce(Force);
    }
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
        if (ACMChimera* Chimera = Cast<ACMChimera>(TargetActor))
        {
            int32& OverlapCount = OverlappingChimeras.FindOrAdd(Chimera);
            ++OverlapCount;
        }

        OnTargetEntered.Broadcast(TargetActor, GetWorldForceDirection());
    }
}

// 구역에서 이탈한 대상을 힘 해제 확장 지점으로 전달
void UCMForceZoneComponent::NotifyTargetExited(AActor* TargetActor)
{
    if (IsValid(TargetActor) && GetOwner() && GetOwner()->HasAuthority())
    {
        if (ACMChimera* Chimera = Cast<ACMChimera>(TargetActor))
        {
            if (int32* OverlapCount = OverlappingChimeras.Find(Chimera))
            {
                --(*OverlapCount);
                if (*OverlapCount <= 0)
                {
                    OverlappingChimeras.Remove(Chimera);
                }
            }
        }

        OnTargetExited.Broadcast(TargetActor, GetWorldForceDirection());
    }
}
