#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Stage/Trigger/CMVisionStoneBase.h"
#include "TimerManager.h"

#include "CMVisionStoneIndicatorWidget.generated.h"

class UImage;
class UTextBlock;

/** World-space watcher count for a vision stone. */
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class UI_API UCMVisionStoneIndicatorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetVisionState(
        const FCMVisionStonePresentationState& State,
        bool bAnimate);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UTextBlock> WatcherCountText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> OpenEyeIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> ClosedEyeIcon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Vision Stone")
    FLinearColor IncompleteColor = FLinearColor::White;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Vision Stone")
    FLinearColor CompleteColor =
        FLinearColor(1.0f, 0.72f, 0.05f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Vision Stone")
    FLinearColor WarningColor =
        FLinearColor(1.0f, 0.05f, 0.02f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Vision Stone|Animation",
        meta = (ClampMin = "1.0"))
    float CountPulseScale = 1.15f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Vision Stone|Animation",
        meta = (ClampMin = "1.0"))
    float WarningPulseScale = 1.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Vision Stone|Animation",
        meta = (ClampMin = "0.0"))
    float PulseDuration = 0.25f;

private:
    void ApplyState();
    void AdvancePulse();
    void StartPulseTimer();
    void StopPulseTimer();

    FCMVisionStonePresentationState CurrentState;
    int32 PreviousWatchingPlayerCount = 0;
    float PulseElapsed = 0.0f;
    bool bHasState = false;
    bool bPulsing = false;
    FTimerHandle PulseTimerHandle;
};
