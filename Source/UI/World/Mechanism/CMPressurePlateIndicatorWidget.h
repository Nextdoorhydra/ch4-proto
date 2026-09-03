#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "CMPressurePlateIndicatorWidget.generated.h"

class UTextBlock;
class UImage;

/** World-space pressure plate readout with a short count and scale response. */
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class UI_API UCMPressurePlateIndicatorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Chimera|World UI|Pressure Plate")
    void SetWeightState(
        float CurrentWeight,
        float RequiredWeight,
        bool bTriggered,
        bool bAnimate
    );

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    // The derived WBP must contain a TextBlock with this exact name.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UTextBlock> WeightText;

    // Optional soft glow image placed behind WeightText in the derived WBP.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> CompletionGlow;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|Animation",
        meta = (ClampMin = "0.01"))
    float CountWeightPerSecond = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|Animation",
        meta = (ClampMin = "0.0"))
    float MinCountDuration = 0.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|Animation",
        meta = (ClampMin = "0.0"))
    float MaxCountDuration = 0.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|Animation",
        meta = (ClampMin = "1.0"))
    float IncreasePulseScale = 1.15f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|Animation",
        meta = (ClampMin = "0.0"))
    float IncreasePulseDuration = 0.22f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|Completion")
    FLinearColor IncompleteTextColor = FLinearColor::White;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|Completion")
    FLinearColor CompleteTextColor = FLinearColor(1.0f, 0.72f, 0.05f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|Completion")
    FLinearColor CompleteGlowColor = FLinearColor(1.0f, 0.55f, 0.02f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|Completion",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CompleteGlowOpacity = 0.45f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|Completion",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CompletePulseGlowOpacity = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|World UI|Pressure Plate|Completion",
        meta = (ClampMin = "0.0"))
    float CompletePulseDuration = 0.4f;

private:
    void RefreshWeightText();
    void ApplyCompletionStyle();
    void StartAnimationTimer();
    void AdvanceAnimations();
    void StopAnimationTimer();

    float DisplayedWeight = 0.0f;
    float CountStartWeight = 0.0f;
    float TargetWeight = 0.0f;
    float RequiredWeightValue = 0.0f;
    float CountElapsed = 0.0f;
    float ActiveCountDuration = 0.0f;
    float PulseElapsed = 0.0f;
    float CompletePulseElapsed = 0.0f;
    bool bCounting = false;
    bool bPulsing = false;
    bool bComplete = false;
    bool bCompletePulsing = false;
    FTimerHandle AnimationTimerHandle;
};
