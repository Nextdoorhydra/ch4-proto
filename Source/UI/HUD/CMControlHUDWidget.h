#pragma once

#include "CoreMinimal.h"
#include "UI/NKMUIActivatableWidget.h"

#include "CMControlHUDWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class ACMControlBody;

/** Local-only, persistent Q/W/E/R control assignment HUD. */
UCLASS(Blueprintable)
class UI_API UCMControlHUDWidget : public UNKMUIActivatableWidget
{
    GENERATED_BODY()

public:
    UCMControlHUDWidget(const FObjectInitializer& ObjectInitializer);

    void SetControlBody(ACMControlBody* NewControlBody);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime
    ) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Control HUD|Layout")
    FVector2D HUDPosition = FVector2D(0.0f, -30.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Control HUD|Layout", meta = (ClampMin = "1.0"))
    float CardWidth = 95.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Control HUD|Layout", meta = (ClampMin = "1.0"))
    float CardHeight = 58.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Control HUD|Layout", meta = (ClampMin = "0.0"))
    float CardSpacing = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Control HUD|Text", meta = (ClampMin = "1"))
    int32 KeyFontSize = 20;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Control HUD|Text", meta = (ClampMin = "1"))
    int32 AssignmentFontSize = 11;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Control HUD|Color")
    FLinearColor DisabledCardColor = FLinearColor(0.08f, 0.08f, 0.08f, 0.82f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Control HUD|Color", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EnabledOpacity = 0.62f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Control HUD|Color", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PressedOpacity = 0.95f;

private:
    void BuildFallbackWidgetTree();
    bool CacheWidgetTreeReferences();
    void RefreshControlSlots();

    TWeakObjectPtr<ACMControlBody> ControlBody;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> HUDContainer;

    UPROPERTY(Transient)
    TObjectPtr<UHorizontalBox> ControlSlotBox;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> StaminaProgressBar;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UProgressBar>> BodyHealthBars;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UProgressBar>> PartHealthBars;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> ControlSlotBorders;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> AssignmentTexts;
};
