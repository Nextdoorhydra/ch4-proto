#pragma once

#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "NKMUICommonButtonBase.generated.h"

UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class NKMUI_API UNKMUICommonButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UNKMUICommonButtonBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "NKM|UI")
	void SetButtonLabel(const FText& InText);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|MVVM")
	void ReceiveBindViewModel();
};
