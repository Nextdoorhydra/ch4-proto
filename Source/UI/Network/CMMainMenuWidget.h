#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ListenServerNetworkTypes.h"

#include "CMMainMenuWidget.generated.h"

class UButton;
class UEditableText;
class UListenServerSessionSubsystem;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class UI_API UCMMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_CreateRoom;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_JoinRoom;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_QuickMatch;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Options;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Quit;

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

    UFUNCTION()
    void HandleJoinConfirmClicked();

    UFUNCTION()
    void HandleJoinCancelClicked();
    
private:
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

    FString PendingRoomId;
    bool bWaitingForRoomSearch = false;
};
