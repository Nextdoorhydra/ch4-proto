#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CMGrabPullTarget.generated.h"

UINTERFACE(BlueprintType)
class CHIMERA_API UCMGrabPullTarget : public UInterface
{
    GENERATED_BODY()
};

// 그랩 또는 훅 계열 팔의 당김 입력을 받을 수 있는 장치 규약
class CHIMERA_API ICMGrabPullTarget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Chimera|Mechanism|Pull")
    bool TryHandlePull(AActor* PullingActor, FVector PullOrigin, float PullStrength);
};
