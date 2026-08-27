#pragma once

#include "CoreMinimal.h"
#include "UI/NKMUIDialogBase.h"

#include "CMConfirmationDialogWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(Abstract)
class UI_API UCMConfirmationDialogWidget : public UNKMUIDialogBase
{
    GENERATED_BODY()

public:
    virtual void SetupDialog(const FNKMUIDialogDescriptor& Descriptor) override;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent) override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
    UFUNCTION()
    void HandleConfirmClicked();

    UFUNCTION()
    void HandleCancelClicked();

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Header;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Body;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Confirm;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Cancel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Btn_Confirm;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Btn_Cancel;
};
