// Source/NetKarmaGame/UI/UIHUDLayout.cpp
#include "UI/NKMUIRootLayout.h"
#include "Engine/GameInstance.h"
#include "UI/Subsystem/NKMUIManagerSubsystem.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "UI/NKMUITagList.h"

UNKMUIRootLayout::UNKMUIRootLayout(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNKMUIRootLayout::NativeConstruct()
{
	Super::NativeConstruct();

	if (HUDLayer)
	{
		const ESlateVisibility InitialVisibility = HUDLayer->GetVisibility();
		if (InitialVisibility != ESlateVisibility::Collapsed && InitialVisibility != ESlateVisibility::Hidden)
		{
			ShownHUDVisibility = InitialVisibility;
		}
		HUDLayer->SetRenderOpacity(1.f);
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UNKMUIManagerSubsystem* Manager =
			GameInstance->GetSubsystem<UNKMUIManagerSubsystem>())
		{
			HUDVisibilityManager = Manager;
			HUDVisibilityChangedHandle = Manager->OnGameplayHUDVisibilityChanged().AddUObject(
				this,
				&ThisClass::HandleGameplayHUDVisibilityChanged);
			ApplyGameplayHUDVisibility(Manager->IsGameplayHUDVisible(), false);
		}
	}
}

void UNKMUIRootLayout::NativeDestruct()
{
	if (HUDVisibilityManager.IsValid() && HUDVisibilityChangedHandle.IsValid())
	{
		HUDVisibilityManager->OnGameplayHUDVisibilityChanged().Remove(HUDVisibilityChangedHandle);
	}
	HUDVisibilityManager.Reset();
	HUDVisibilityChangedHandle.Reset();
	bHUDVisibilityFadeActive = false;

	Super::NativeDestruct();
}

void UNKMUIRootLayout::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bHUDVisibilityFadeActive || !HUDLayer) return;

	HUDVisibilityFadeElapsedTime += InDeltaTime;
	const float Alpha = HUDVisibilityFadeDuration > 0.f
		? FMath::Clamp(HUDVisibilityFadeElapsedTime / HUDVisibilityFadeDuration, 0.f, 1.f)
		: 1.f;
	const float Opacity = FMath::InterpEaseInOut(
		HUDVisibilityFadeStartOpacity,
		HUDVisibilityFadeTargetOpacity,
		Alpha,
		HUDVisibilityFadeExponent);
	HUDLayer->SetRenderOpacity(Opacity);

	if (Alpha < 1.f) return;

	bHUDVisibilityFadeActive = false;
	HUDLayer->SetRenderOpacity(HUDVisibilityFadeTargetOpacity);
	if (HUDVisibilityFadeTargetOpacity <= 0.f)
	{
		HUDLayer->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UNKMUIRootLayout::HandleGameplayHUDVisibilityChanged(bool bVisible)
{
	ApplyGameplayHUDVisibility(bVisible, true);
}

void UNKMUIRootLayout::ApplyGameplayHUDVisibility(bool bVisible, bool bAnimate)
{

	if (!HUDLayer) return;

	if (bVisible)
	{
		HUDLayer->SetVisibility(ShownHUDVisibility);
	}

	HUDVisibilityFadeStartOpacity = HUDLayer->GetRenderOpacity();
	HUDVisibilityFadeTargetOpacity = bVisible ? 1.f : 0.f;
	HUDVisibilityFadeElapsedTime = 0.f;

	if (!bAnimate
		|| HUDVisibilityFadeDuration <= 0.f
		|| FMath::IsNearlyEqual(HUDVisibilityFadeStartOpacity, HUDVisibilityFadeTargetOpacity))
	{
		bHUDVisibilityFadeActive = false;
		HUDLayer->SetRenderOpacity(HUDVisibilityFadeTargetOpacity);
		if (!bVisible)
		{
			HUDLayer->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	bHUDVisibilityFadeActive = true;
}

UCommonActivatableWidgetContainerBase* UNKMUIRootLayout::GetLayerWidget(FGameplayTag LayerTag) const
{
	if (LayerTag.MatchesTag(UITags::UI_Layer_HUD)) return HUDLayer;
	if (LayerTag.MatchesTag(UITags::UI_Layer_Menu)) return MenuLayer;
	if (LayerTag.MatchesTag(UITags::UI_Layer_Modal)) return ModalLayer;
	return nullptr;
}
