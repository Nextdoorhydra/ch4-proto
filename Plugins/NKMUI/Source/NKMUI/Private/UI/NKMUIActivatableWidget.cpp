#include "UI/NKMUIActivatableWidget.h"

UNKMUIActivatableWidget::UNKMUIActivatableWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNKMUIActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	ReceiveBindViewModel();
}

TOptional<FUIInputConfig> UNKMUIActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputConfig)
	{
	case ENKMUIWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case ENKMUIWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case ENKMUIWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case ENKMUIWidgetInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}
