// Source/NetKarmaGame/UI/UIPolicy.cpp
#include "UI/NKMUIPolicy.h"
#include "UI/NKMUIActivatableWidget.h"
#include "UI/NKMUIRootLayout.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UNKMUIPolicy::UNKMUIPolicy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSoftClassPtr<UNKMUIRootLayout> UNKMUIPolicy::GetLayoutClass() const
{
	return LayoutClass;
}

bool UNKMUIPolicy::CreateLayout(ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer || !LocalPlayer->ViewportClient)
	{
		return false;
	}

	const TSubclassOf<UNKMUIRootLayout> LoadedClass = GetLayoutClass().Get();
	if (!LoadedClass)
	{
		return false;
	}

	OwningLocalPlayer = LocalPlayer;
	RootLayoutInstance = CreateWidget<UNKMUIRootLayout>(LocalPlayer->GetWorld(), LoadedClass);
	if (!RootLayoutInstance)
	{
		return false;
	}

	RootLayoutInstance->AddToPlayerScreen();
	return true;
}

UNKMUIActivatableWidget* UNKMUIPolicy::PushWidgetToLayer(
	FGameplayTag LayerTag,
	TSubclassOf<UNKMUIActivatableWidget> WidgetClass)
{
	if (!RootLayoutInstance || !WidgetClass)
	{
		return nullptr;
	}

	if (UCommonActivatableWidgetContainerBase* TargetLayer = RootLayoutInstance->GetLayerWidget(LayerTag))
	{
		return TargetLayer->AddWidget<UNKMUIActivatableWidget>(WidgetClass);
	}

	return nullptr;
}

void UNKMUIPolicy::RemoveWidgetFromLayer(FGameplayTag LayerTag, UNKMUIActivatableWidget* Widget)
{
	if (!RootLayoutInstance || !Widget)
	{
		return;
	}

	if (UCommonActivatableWidgetContainerBase* TargetLayer = RootLayoutInstance->GetLayerWidget(LayerTag))
	{
		TargetLayer->RemoveWidget(*Widget);
	}
}

void UNKMUIPolicy::ClearLayer(FGameplayTag LayerTag)
{
	if (!RootLayoutInstance)
	{
		return;
	}

	if (UCommonActivatableWidgetContainerBase* TargetLayer = RootLayoutInstance->GetLayerWidget(LayerTag))
	{
		TargetLayer->ClearWidgets();
	}
}

void UNKMUIPolicy::DestroyLayout()
{
	if (RootLayoutInstance)
	{
		RootLayoutInstance->RemoveFromParent();
		RootLayoutInstance = nullptr;
	}

	OwningLocalPlayer = nullptr;
}
