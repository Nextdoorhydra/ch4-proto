#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "Stage/Trigger/CMTriggerPresentationState.h"

#include "CMPressurePlateIndicatorComponent.generated.h"

class ACMPressurePlateBase;

/** Binds a world-space widget to its owning pressure plate's replicated state. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class UI_API UCMPressurePlateIndicatorComponent : public UWidgetComponent
{
    GENERATED_BODY()

public:
    UCMPressurePlateIndicatorComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate")
    bool bAnimateWeightChanges = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|View",
        meta = (ClampMin = "0.0", Units = "cm"))
    float FadeStartDistance = 800.0f;

    // Zero disables distance hiding. Values above FadeStartDistance fade out.
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|View",
        meta = (ClampMin = "0.0", Units = "cm"))
    float FadeEndDistance = 1800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|View",
        meta = (ClampMin = "0.02", Units = "s"))
    float DistanceUpdateInterval = 0.1f;

private:
    UFUNCTION()
    void HandlePresentationStateChanged(
        const FCMTriggerPresentationState& State
    );

    void ApplyPresentationState(
        const FCMTriggerPresentationState& State,
        bool bAnimate
    );
    void UpdateDistanceFade();

    TWeakObjectPtr<ACMPressurePlateBase> PressurePlate;
    float DistanceUpdateElapsed = 0.0f;
    bool bHasAppliedReadyState = false;
    bool bHasDisplayableState = false;
    bool bInsideFadeRange = false;
};
