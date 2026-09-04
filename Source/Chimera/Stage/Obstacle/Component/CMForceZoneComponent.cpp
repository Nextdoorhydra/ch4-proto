#include "Stage/Obstacle/Component/CMForceZoneComponent.h"

#include "Player/CMChimera.h"

UCMForceZoneComponent::UCMForceZoneComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// 구역 안에 있는 키메라에 설정된 가속도를 서버에서 지속 적용
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

    const FVector ForceDirection = GetWorldForceDirection();
    const FVector Acceleration = ForceDirection * ForceStrength;
    if (Acceleration.IsNearlyZero())
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

        if (MaxWindSpeed > 0.0f
            && Chimera->GetAssemblyVelocityAlongDirection(ForceDirection)
                >= MaxWindSpeed)
        {
            continue;
        }

        Chimera->ApplyEnvironmentalForce(Acceleration);
    }

    for (auto ChimeraIt = OverlappingSegments.CreateIterator(); ChimeraIt; ++ChimeraIt)
    {
        ACMChimera* Chimera = ChimeraIt.Key().Get();
        if (!IsValid(Chimera))
        {
            ChimeraIt.RemoveCurrent();
            continue;
        }

        const bool bReachedMaxWindSpeed = MaxWindSpeed > 0.0f
            && Chimera->GetAssemblyVelocityAlongDirection(ForceDirection)
                >= MaxWindSpeed;

        for (auto SegmentIt = ChimeraIt.Value().CreateIterator(); SegmentIt; ++SegmentIt)
        {
            if (SegmentIt.Value() <= 0
                || !Chimera->IsSegmentAlive(SegmentIt.Key()))
            {
                SegmentIt.RemoveCurrent();
                continue;
            }

            if (bReachedMaxWindSpeed)
            {
                continue;
            }

            Chimera->ApplyEnvironmentalForceToSegment(
                SegmentIt.Key(),
                Acceleration);
        }

        if (ChimeraIt.Value().IsEmpty())
        {
            ChimeraIt.RemoveCurrent();
        }
    }
}

// 소유 액터 기준 방향을 월드 방향으로 변환
FVector UCMForceZoneComponent::GetWorldForceDirection() const
{
    const AActor* Owner = GetOwner();
    return Owner ? Owner->GetActorTransform().TransformVectorNoScale(LocalDirection).GetSafeNormal() : FVector::ZeroVector;
}

// 유효한 진입 대상과 힘 방향을 물리 처리 확장 지점으로 전달
void UCMForceZoneComponent::NotifyTargetEntered(
    AActor* TargetActor,
    UPrimitiveComponent* TargetComponent)
{
    if (bZoneEnabled && IsValid(TargetActor) && GetOwner() && GetOwner()->HasAuthority())
    {
        if (ACMChimera* Chimera = Cast<ACMChimera>(TargetActor))
        {
            if (TargetComponent)
            {
                const int32 SegmentIndex =
                    Chimera->GetSegmentIndexFromDamageComponent(TargetComponent);
                if (SegmentIndex != INDEX_NONE)
                {
                    ++OverlappingSegments.FindOrAdd(Chimera)
                        .FindOrAdd(SegmentIndex);
                }
            }
            else
            {
                ++OverlappingChimeras.FindOrAdd(Chimera);
            }
        }

        OnTargetEntered.Broadcast(TargetActor, GetWorldForceDirection());
    }
}

// 구역에서 이탈한 대상을 힘 해제 확장 지점으로 전달
void UCMForceZoneComponent::NotifyTargetExited(
    AActor* TargetActor,
    UPrimitiveComponent* TargetComponent)
{
    if (IsValid(TargetActor) && GetOwner() && GetOwner()->HasAuthority())
    {
        if (ACMChimera* Chimera = Cast<ACMChimera>(TargetActor))
        {
            if (TargetComponent)
            {
                const int32 SegmentIndex =
                    Chimera->GetSegmentIndexFromDamageComponent(TargetComponent);
                if (TMap<int32, int32>* SegmentCounts =
                        OverlappingSegments.Find(Chimera))
                {
                    if (int32* OverlapCount = SegmentCounts->Find(SegmentIndex))
                    {
                        --(*OverlapCount);
                        if (*OverlapCount <= 0)
                        {
                            SegmentCounts->Remove(SegmentIndex);
                        }
                    }
                    if (SegmentCounts->IsEmpty())
                    {
                        OverlappingSegments.Remove(Chimera);
                    }
                }
            }
            else if (int32* OverlapCount = OverlappingChimeras.Find(Chimera))
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
