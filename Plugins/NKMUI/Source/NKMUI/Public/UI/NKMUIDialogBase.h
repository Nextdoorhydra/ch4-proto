#pragma once

#include "CoreMinimal.h"
#include "UI/NKMUIActivatableWidget.h"
#include "NKMUIDialogBase.generated.h"

// 사용자가 누른 버튼의 결과
UENUM(BlueprintType)
enum class ENKMUIDialogResult : uint8
{
	Confirmed,  // 확인/수락
	Declined,   // 거절
	Cancelled,  // 취소/닫기 (ESC 등)
	Ignored     // 무시됨 (에러 등)
};

// 팝업창을 구성할 데이터 (제목, 본문, 버튼 텍스트)
USTRUCT(BlueprintType)
struct NKMUI_API FNKMUIDialogDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText Header;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText Body;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText ConfirmText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText CancelText;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNKMUIDialogResultDelegate, ENKMUIDialogResult, Result);

UCLASS(Abstract, Blueprintable)
class NKMUI_API UNKMUIDialogBase : public UNKMUIActivatableWidget
{
	GENERATED_BODY()

public:
	// 비동기 노드에서 데이터를 주입할 때 호출
	virtual void SetupDialog(const FNKMUIDialogDescriptor& Descriptor);

	// 위젯이 닫힐 때 결과를 브로드캐스트할 델리게이트
	UPROPERTY(BlueprintAssignable)
	FNKMUIDialogResultDelegate OnDialogClosed;

protected:
	virtual void NativeOnDeactivated() override;

	// SetupDialog가 Descriptor를 저장한 직후 호출됩니다.
	// 다이얼로그 WBP는 이 이벤트에서 Text/YesButton/NoButton에 전달받은 문구를 반영합니다.
	// NativeOnActivated보다 Descriptor 주입이 늦으므로 표시 갱신은 이 이벤트를 사용해야 합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "NKM|Dialog", meta = (DisplayName = "On Dialog Setup"))
	void ReceiveSetupDialog(const FNKMUIDialogDescriptor& Descriptor);

	// 블루프린트(WBP)에서 확인/취소 버튼 클릭 시 호출해야 할 함수
	UFUNCTION(BlueprintCallable, Category = "NKM|Dialog")
	virtual void CloseDialog(ENKMUIDialogResult Result);

	// 자식 WBP에서 텍스트 바인딩 시 사용할 데이터
	UPROPERTY(BlueprintReadOnly, Category = "NKM|Dialog")
	FNKMUIDialogDescriptor DialogDescriptor;

private:
	bool bDialogResolved = false;
};
