#pragma once

#include "CoreMinimal.h"
#include "UI/NKMUIDialogBase.h"
#include "UI/NKMUIActivatableWidget.h"
#include "UI/Subsystem/NKMUIManagerSubsystem.h"

#include "CMEscapeMenuWidget.generated.h"

class UButton;
class UCMConfirmationDialogWidget;
class UCMOptionWidget;
class UNKMAsyncAction_ShowConfirmation;

UCLASS(Abstract)
class UI_API UCMEscapeMenuWidget : public UNKMUIActivatableWidget
{
    GENERATED_BODY()

public:
    UCMEscapeMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent) override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
    UFUNCTION()
    void HandleResumeClicked();

    UFUNCTION()
    void HandleOptionsClicked();

    UFUNCTION()
    void HandleOptionPushed(
        ENKMUIAsyncResult Result,
        UNKMUIActivatableWidget* Widget);

    UFUNCTION()
    void HandleLeaveGameClicked();

    UFUNCTION()
    void HandleLeaveConfirmation(ENKMUIDialogResult Result);

    void LeaveGame();

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Btn_Resume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Btn_Options;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Btn_LeaveGame;

    UPROPERTY(EditDefaultsOnly, Category = "Chimera|ESC Menu")
    TSoftClassPtr<UCMOptionWidget> OptionWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Chimera|ESC Menu")
    TSoftClassPtr<UCMConfirmationDialogWidget> ConfirmationDialogClass;

    UPROPERTY(Transient)
    TObjectPtr<UNKMAsyncAction_ShowConfirmation> ActiveLeaveAction;

    TWeakObjectPtr<UCMOptionWidget> ActiveOptionWidget;
    bool bOptionRequestPending = false;
};
