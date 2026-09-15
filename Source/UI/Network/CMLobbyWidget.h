#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ListenServerNetworkTypes.h"

#include "CMLobbyWidget.generated.h"

class ACMGameState;
class UButton;
class UCMMainMenuWidget;
class UCMLobbyPlayerRowWidget;
class UCMStageRouteDefinition;
class UListenServerSessionSubsystem;
class UScrollBox;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class UI_API UCMLobbyWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Lobby")
    TSubclassOf<UCMLobbyPlayerRowWidget> LobbyPlayerRowClass;

    UPROPERTY(EditDefaultsOnly, Category = "Chimera|Lobby")
    TSoftClassPtr<UCMMainMenuWidget> MainMenuWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Chimera|Lobby")
    TSoftObjectPtr<UCMStageRouteDefinition> StageRouteDefinition;

    UPROPERTY(EditDefaultsOnly, Category = "Chimera|Lobby")
    TSoftObjectPtr<UCMStageRouteDefinition> TestStageRouteDefinition;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_StartGame;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_StartTestGame;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Ready;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Invite;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Leave;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_RoomId;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_PlayerCount;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UScrollBox> SB_LobbyPlayers;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Result;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_ReadyState;

private:
    void BindCurrentGameState();
    void RefreshRoomId();
    void RefreshLobbyRoster();
    void UpdateControls();
    void SetResultText(const FText& Message);
    void ShowFrontendMainMenuIfReady();
    bool StartFrontendStageRoute(bool bTestRoute);

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
    void HandleLobbyRosterChanged();

    UFUNCTION()
    void HandleStartGameClicked();

    UFUNCTION()
    void HandleStartTestGameClicked();

    UFUNCTION()
    void HandleReadyClicked();

    UFUNCTION()
    void HandleInviteClicked();

    UFUNCTION()
    void HandleLeaveClicked();

    UPROPERTY(Transient)
    TObjectPtr<UListenServerSessionSubsystem> NetworkSubsystem;

    TWeakObjectPtr<ACMGameState> BoundGameState;
};
