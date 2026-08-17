#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/NoExportTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "NKMUIPolicy.generated.h"

class UNKMUIActivatableWidget;
class ULocalPlayer;
class UNKMUIRootLayout;

// 역할: NKMUILayout 생성; 레이어 라우팅; 게임 모드에 따라 UI 배치가 달라져도 Policy 클ㄹ스만 교체하면 됨
// 호출 타이밍: UNKKMUIManagerSubsystem에서 CreatLayout 호출
UCLASS(Blueprintable, Abstract)
class NKMUI_API UNKMUIPolicy : public UObject
{
	GENERATED_BODY()

public:
	UNKMUIPolicy(const FObjectInitializer& ObjectInitializer);

	virtual TSoftClassPtr<UNKMUIRootLayout> GetLayoutClass() const;
	bool CreateLayout(ULocalPlayer* LocalPlayer);
	UNKMUIActivatableWidget* PushWidgetToLayer(
		FGameplayTag LayerTag,
		TSubclassOf<UNKMUIActivatableWidget> WidgetClass);
	void RemoveWidgetFromLayer(FGameplayTag LayerTag, UNKMUIActivatableWidget* Widget);
	void ClearLayer(FGameplayTag LayerTag);
	void DestroyLayout();

protected:
	UPROPERTY(EditAnywhere, Category = "UI|Policy")
	TSoftClassPtr<UNKMUIRootLayout> LayoutClass;

	UPROPERTY()
	TObjectPtr<UNKMUIRootLayout> RootLayoutInstance;

	UPROPERTY()
	TObjectPtr<ULocalPlayer> OwningLocalPlayer;
};
