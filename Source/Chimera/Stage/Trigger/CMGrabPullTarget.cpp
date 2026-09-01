#include "Stage/Trigger/CMGrabPullTarget.h"

ECMGrabPullResult ICMGrabPullTarget::HandlePullWithResult_Implementation(
    AActor* PullingActor, FVector PullOrigin, float PullStrength)
{
    return Execute_TryHandlePull(_getUObject(), PullingActor, PullOrigin, PullStrength)
        ? ECMGrabPullResult::Applied : ECMGrabPullResult::Unhandled;
}
