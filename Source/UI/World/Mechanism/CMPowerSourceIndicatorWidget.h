#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "CMPowerSourceIndicatorWidget.generated.h"

class UImage;
class UTexture2D;

/** World-space power source icon with connected-power feedback. */
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class UI_API UCMPowerSourceIndicatorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UCMPowerSourceIndicatorWidget(
        const FObjectInitializer& ObjectInitializer);

    void SetPowerState(bool bPoweredConnection, bool bAnimate);
    float GetPowerOnFlashDelay() const { return PulseDuration * 0.5f; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> PowerIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> PowerGlow;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source")
    TSoftObjectPtr<UTexture2D> SparkTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source")
    FLinearColor DisconnectedColor =
        FLinearColor(0.35f, 0.35f, 0.35f, 0.8f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source")
    FLinearColor PoweredColor =
        FLinearColor(1.0f, 0.72f, 0.05f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source")
    FLinearColor FlashColor =
        FLinearColor(1.0f, 0.95f, 0.55f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source")
    FLinearColor GlowColor =
        FLinearColor(1.0f, 0.8f, 0.2f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source|Animation",
        meta = (ClampMin = "1.0"))
    float PulseScale = 1.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source|Animation",
        meta = (ClampMin = "0.0"))
    float PulseDuration = 0.65f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source|Animation",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PulseGlowOpacity = 1.0f;

private:
    void ApplyVisualState();
    void StartAnimationTimer();
    void AdvanceAnimation();
    void StopAnimationTimer();

    float PulseElapsed = 0.0f;
    bool bPowered = false;
    bool bPulsing = false;
    FTimerHandle AnimationTimerHandle;
};
