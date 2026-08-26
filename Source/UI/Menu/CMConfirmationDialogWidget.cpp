#include "CMConfirmationDialogWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

void UCMConfirmationDialogWidget::SetupDialog(
    const FNKMUIDialogDescriptor& Descriptor)
{
    Super::SetupDialog(Descriptor);

    Text_Header->SetText(Descriptor.Header);
    Text_Body->SetText(Descriptor.Body);
    Text_Confirm->SetText(Descriptor.ConfirmText);
    Text_Cancel->SetText(Descriptor.CancelText);
}

void UCMConfirmationDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_Confirm->OnClicked.AddUniqueDynamic(
        this, &ThisClass::HandleConfirmClicked);
    Btn_Cancel->OnClicked.AddUniqueDynamic(
        this, &ThisClass::HandleCancelClicked);
}

void UCMConfirmationDialogWidget::NativeDestruct()
{
    Btn_Confirm->OnClicked.RemoveDynamic(
        this, &ThisClass::HandleConfirmClicked);
    Btn_Cancel->OnClicked.RemoveDynamic(
        this, &ThisClass::HandleCancelClicked);
    Super::NativeDestruct();
}

FReply UCMConfirmationDialogWidget::NativeOnPreviewKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        CloseDialog(ENKMUIDialogResult::Cancelled);
        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

UWidget* UCMConfirmationDialogWidget::NativeGetDesiredFocusTarget() const
{
    return Btn_Cancel;
}

void UCMConfirmationDialogWidget::HandleConfirmClicked()
{
    CloseDialog(ENKMUIDialogResult::Confirmed);
}

void UCMConfirmationDialogWidget::HandleCancelClicked()
{
    CloseDialog(ENKMUIDialogResult::Declined);
}
