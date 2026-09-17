#include "UI/NKMUIDialogBase.h"

#include "UI/NKMUICommonButtonBase.h"

void UNKMUIDialogBase::SetupDialog(const FNKMUIDialogDescriptor& Descriptor)
{
	bDialogResolved = false;
	DialogDescriptor = Descriptor;

	// PushWidget으로 위젯이 활성화된 다음 Descriptor가 주입되므로, 활성화 이벤트가 아니라
	// 이 시점에 WBP가 실제 텍스트 위젯과 버튼 문구를 갱신하도록 알립니다.
	ReceiveSetupDialog(DialogDescriptor);

	if (UNKMUICommonButtonBase* YesButton =
		Cast<UNKMUICommonButtonBase>(GetWidgetFromName(TEXT("YesButton"))))
	{
		YesButton->SetButtonLabel(DialogDescriptor.ConfirmText);
	}
	if (UNKMUICommonButtonBase* NoButton =
		Cast<UNKMUICommonButtonBase>(GetWidgetFromName(TEXT("NoButton"))))
	{
		NoButton->SetButtonLabel(DialogDescriptor.CancelText);
	}
}

void UNKMUIDialogBase::CloseDialog(ENKMUIDialogResult Result)
{
	if (!bDialogResolved)
	{
		bDialogResolved = true;
		OnDialogClosed.Broadcast(Result);
	}

	DeactivateWidget();
}

void UNKMUIDialogBase::NativeOnDeactivated()
{
	if (!bDialogResolved)
	{
		bDialogResolved = true;
		OnDialogClosed.Broadcast(ENKMUIDialogResult::Cancelled);
	}

	Super::NativeOnDeactivated();
}
