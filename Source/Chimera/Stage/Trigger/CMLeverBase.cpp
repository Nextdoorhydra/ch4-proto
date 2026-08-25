#include "Stage/Trigger/CMLeverBase.h"

ACMLeverBase::ACMLeverBase()
{
}

// 당김 힘과 레버의 허용 방향이 모두 맞을 때만 서버에서 작동
bool ACMLeverBase::TryHandlePull_Implementation(
    AActor* PullingActor,
    FVector PullOrigin,
    float PullStrength)
{
    if (!HasAuthority() || PullStrength < RequiredPullStrength)
    {
        return false;
    }

    const FVector PullDirection =
        (PullOrigin - GetActorLocation()).GetSafeNormal();
    const FVector PullAxis =
        GetActorTransform().TransformVectorNoScale(LocalPullAxis).GetSafeNormal();
    if (PullDirection.IsNearlyZero() || PullAxis.IsNearlyZero()
        || FVector::DotProduct(PullDirection, PullAxis) < MinimumPullAlignment)
    {
        return false;
    }

    if (!PressButton(PullingActor))
    {
        return false;
    }

    OnLeverPulled(PullingActor);
    return true;
}
