#pragma once

#include "CoreMinimal.h"
#include "Stage/Trigger/CMGrabPullTarget.h"
#include "Stage/Trigger/CMStageButtonBase.h"

#include "CMLeverBase.generated.h"

UCLASS(Blueprintable)
// 지정 방향과 세기의 그랩팔 당김을 버튼 작동으로 변환하는 레버
class CHIMERA_API ACMLeverBase
    : public ACMStageButtonBase
    , public ICMGrabPullTarget
{
    GENERATED_BODY()

public:
    ACMLeverBase();

    virtual bool TryHandlePull_Implementation(
        AActor* PullingActor,
        FVector PullOrigin,
        float PullStrength) override;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Lever",
        meta = (ClampMin = "0.0"))
    float RequiredPullStrength = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Lever")
    FVector LocalPullAxis = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Lever",
        meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float MinimumPullAlignment = 0.5f;

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Lever")
    void OnLeverPulled(AActor* PullingActor);
};
