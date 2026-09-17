#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"

#include "CMGroundPlacementBoxComponent.generated.h"

/** 실제 몸통 충돌은 유지하고 End 키 배치 Bounds만 다리 최저점까지 포함하는 박스다. */
UCLASS()
class AI_API UCMGroundPlacementBoxComponent final : public UBoxComponent
{
    GENERATED_BODY()

public:
    void SetGroundContactHeight(float InGroundContactHeight);
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private:
    float GroundContactHeight = 0.0f;
};
