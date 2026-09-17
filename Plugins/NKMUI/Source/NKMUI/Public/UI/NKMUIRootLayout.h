#pragma once

#include "CommonUserWidget.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NKMUIRootLayout.generated.h"

class UCommonActivatableWidgetContainerBase;
class UNKMUIManagerSubsystem;

UCLASS(Abstract, Blueprintable)
class NKMUI_API UNKMUIRootLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UNKMUIRootLayout(const FObjectInitializer& ObjectInitializer);

	// 태그에 대응하는 레이어 위젯(Stack)을 반환하는 유틸리티
	virtual UCommonActivatableWidgetContainerBase* GetLayerWidget(FGameplayTag LayerTag) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetContainerBase> HUDLayer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetContainerBase> MenuLayer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetContainerBase> ModalLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD Visibility", meta=(ClampMin="0.0"))
	float HUDVisibilityFadeDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD Visibility", meta=(ClampMin="1.0"))
	float HUDVisibilityFadeExponent = 2.f;

private:
	void HandleGameplayHUDVisibilityChanged(bool bVisible);
	void ApplyGameplayHUDVisibility(bool bVisible, bool bAnimate);

	TWeakObjectPtr<UNKMUIManagerSubsystem> HUDVisibilityManager;
	FDelegateHandle HUDVisibilityChangedHandle;
	ESlateVisibility ShownHUDVisibility = ESlateVisibility::SelfHitTestInvisible;
	float HUDVisibilityFadeStartOpacity = 1.f;
	float HUDVisibilityFadeTargetOpacity = 1.f;
	float HUDVisibilityFadeElapsedTime = 0.f;
	bool bHUDVisibilityFadeActive = false;
};
