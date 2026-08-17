#pragma once

#include "CommonActivatableWidget.h"
#include "CoreMinimal.h"
#include "NKMUIActivatableWidget.generated.h"

UENUM(BlueprintType)
enum class ENKMUIWidgetInputMode : uint8
{
	Default,
	GameAndMenu,
	Game,
	Menu
};

UCLASS(Abstract, Blueprintable)
class NKMUI_API UNKMUIActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UNKMUIActivatableWidget(const FObjectInitializer& ObjectInitializer);

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnActivated() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|MVVM")
	void ReceiveBindViewModel();

	UPROPERTY(EditDefaultsOnly, Category = "UI|Input")
	ENKMUIWidgetInputMode InputConfig = ENKMUIWidgetInputMode::Default;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Input")
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;
};
