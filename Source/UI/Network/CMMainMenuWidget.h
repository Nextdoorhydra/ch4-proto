#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ListenServerNetworkTypes.h"
#include "UI/Subsystem/NKMUIManagerSubsystem.h"

#include "CMMainMenuWidget.generated.h"

class UButton;
class UCMMenuButtonWidget;
class UCMLobbyWidget;
class UEditableText;
class UListenServerSessionSubsystem;
class UTextBlock;
class UCMOptionWidget;
class UNKMUIActivatableWidget;

UCLASS(Abstract, Blueprintable)
class UI_API UCMMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCMMenuButtonWidget> Btn_CreateRoom;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCMMenuButtonWidget> Btn_JoinRoom;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCMMenuButtonWidget> Btn_QuickMatch;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCMMenuButtonWidget> Btn_Options;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCMMenuButtonWidget> Btn_Quit;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Result;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UWidget> Panel_JoinRoomPopup;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UEditableText> Edt_RoomId;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_JoinConfirm;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_JoinCancel;

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Main Menu")
    void OnOptionsRequested();

    UPROPERTY(EditDefaultsOnly, Category = "Chimera|Main Menu")
    TSoftClassPtr<UCMOptionWidget> OptionWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Chimera|Main Menu")
    TSoftClassPtr<UCMLobbyWidget> LobbyWidgetClass;

    UFUNCTION()
    void HandleJoinConfirmClicked();

    UFUNCTION()
    void HandleJoinCancelClicked();
    
private:
    bool CanStartSessionOperation() const;
    void RecoverStaleSessionIfNeeded();
    void ShowLobbyIfReady();
    void UpdateControls();
    void SetResultText(const FText& Message);

    UFUNCTION()
    void HandleNetworkStateChanged(
        EListenServerRole Role,
        EListenServerConnectionState ConnectionState,
        EListenServerOperation Operation
    );

    UFUNCTION()
    void HandleOperationCompleted(
        EListenServerOperation CompletedOperation,
        const FListenServerOperationResult& Result
    );

    UFUNCTION()
    void HandleOptionsClicked();

    void HandleUIPolicyInitialized(ENKMUIAsyncResult Result);

    UFUNCTION()
    void HandleOptionPushed(
        ENKMUIAsyncResult Result,
        UNKMUIActivatableWidget* Widget);

    void HandleOptionClosed();

    UFUNCTION()
    void HandleCreateRoomClicked();

    UFUNCTION()
    void HandleJoinRoomClicked();

    UFUNCTION()
    void HandleQuickMatchClicked();

    UFUNCTION()
    void HandleQuitClicked();

    UPROPERTY(Transient)
    TObjectPtr<UListenServerSessionSubsystem> NetworkSubsystem;

    TWeakObjectPtr<UCMOptionWidget> ActiveOptionWidget;
    ESlateVisibility VisibilityBeforeOptions = ESlateVisibility::Visible;

    FString PendingRoomId;
    bool bWaitingForRoomSearch = false;
    bool bOptionRequestPending = false;
};
