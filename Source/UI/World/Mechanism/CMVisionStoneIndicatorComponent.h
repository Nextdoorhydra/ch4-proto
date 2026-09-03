#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "Stage/Trigger/CMVisionStoneBase.h"

#include "CMVisionStoneIndicatorComponent.generated.h"

/** Binds a world-space watcher count to its owning vision stone. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class UI_API UCMVisionStoneIndicatorComponent : public UWidgetComponent
{
    GENERATED_BODY()

public:
    UCMVisionStoneIndicatorComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Vision Stone|View",
        meta = (ClampMin = "0.0", Units = "cm"))
    float FadeStartDistance = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Vision Stone|View",
        meta = (ClampMin = "0.0", Units = "cm"))
    float FadeEndDistance = 1800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Vision Stone|View",
        meta = (ClampMin = "0.02", Units = "s"))
    float DistanceUpdateInterval = 0.1f;

private:
    UFUNCTION()
    void HandleVisionStateChanged(
        const FCMVisionStonePresentationState& State);

    void ApplyVisionState(
        const FCMVisionStonePresentationState& State,
        bool bAnimate);
    void UpdateDistanceFade();

    TWeakObjectPtr<ACMVisionStoneBase> VisionStone;
    float DistanceUpdateElapsed = 0.0f;
    bool bHasAppliedReadyState = false;
    bool bHasDisplayableState = false;
    bool bInsideFadeRange = false;
};
