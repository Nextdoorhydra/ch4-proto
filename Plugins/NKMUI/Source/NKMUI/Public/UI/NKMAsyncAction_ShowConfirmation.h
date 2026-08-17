#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UI/NKMUIDialogBase.h"
#include "NKMAsyncAction_ShowConfirmation.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNKMDialogResult, ENKMUIDialogResult, Result);

UCLASS()
class NKMUI_API UNKMAsyncAction_ShowConfirmation : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	// 블루프린트 노드의 출력 핀(Execution Pin)이 될 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnNKMDialogResult OnResult;

	// 블루프린트에서 호출할 진입점 함수
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "NKM|UI")
	static UNKMAsyncAction_ShowConfirmation* ShowConfirmation(UObject* WorldContextObject, TSubclassOf<UNKMUIDialogBase> DialogClass, FNKMUIDialogDescriptor Descriptor);

	virtual void Activate() override;

private:
	UFUNCTION()
	void HandleDialogClosed(ENKMUIDialogResult Result);

	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject;

	UPROPERTY()
	TSubclassOf<UNKMUIDialogBase> TargetDialogClass;

	FNKMUIDialogDescriptor TargetDescriptor;
	bool bCompleted = false;
};
