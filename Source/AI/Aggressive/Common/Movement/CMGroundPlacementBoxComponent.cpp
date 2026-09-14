#include "Aggressive/Common/Movement/CMGroundPlacementBoxComponent.h"

void UCMGroundPlacementBoxComponent::SetGroundContactHeight(float InGroundContactHeight)
{
    GroundContactHeight = InGroundContactHeight;
    UpdateBounds();
}

FBoxSphereBounds UCMGroundPlacementBoxComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    const FBoxSphereBounds BodyBounds = Super::CalcBounds(LocalToWorld);
    FBox GroundedBounds = BodyBounds.GetBox();
    GroundedBounds += LocalToWorld.TransformPosition(FVector(0.0f, 0.0f, GroundContactHeight));
    return FBoxSphereBounds(GroundedBounds);
}
