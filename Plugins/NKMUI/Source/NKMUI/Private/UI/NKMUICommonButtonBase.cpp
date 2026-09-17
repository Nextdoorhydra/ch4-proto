#include "UI/NKMUICommonButtonBase.h"

#include "CommonTextBlock.h"

UNKMUICommonButtonBase::UNKMUICommonButtonBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNKMUICommonButtonBase::SetButtonLabel(const FText& InText)
{
	if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(GetWidgetFromName(TEXT("ButtonText"))))
	{
		Label->SetText(InText);
	}
}

void UNKMUICommonButtonBase::NativeConstruct()
{
	Super::NativeConstruct();
	ReceiveBindViewModel();
}
