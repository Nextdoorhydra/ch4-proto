#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ListenServerNetworkTypes.h"

#include "ChimeraNetworkWidget.generated.h"

class ACMGameState;
class UButton;
class UChimeraLobbyPlayerRowWidget;
class UListenServerSessionSubsystem;
class UPanelWidget;
class UScrollBox;
class UTextBlock;
class UWidget;

UCLASS(Abstract, Blueprintable)
class CHIMERA_API UChimeraNetworkWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Lobby")
    TSubclassOf<UChimeraLobbyPlayerRowWidget> LobbyPlayerRowClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|UI")
    FText ShowDetailsText = NSLOCTEXT(
        "ChimeraUI",
        "ShowDetails",
        "Show UI"
    );

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|UI")
    FText HideDetailsText = NSLOCTEXT(
        "ChimeraUI",
        "HideDetails",
        "Hide UI"
    );

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UWidget> Panel_Menu;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UWidget> Panel_Lobby;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UWidget> Panel_GameActionBar;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UWidget> Panel_PlayerLegend;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> Box_LobbyLegendHost;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> Box_GameLegendHost;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UWidget> Panel_Details;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_PlayerCount;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UScrollBox> SB_LobbyPlayers;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Host;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Find;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_JoinFirst;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_QuickMatch;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Invite;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_StartGame;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Leave;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_ToggleDetails;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_ToggleDetails;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Retry;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_GameLeave;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_State;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Result;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_SearchCount;

private:
    void BindCurrentGameState();
    void MovePlayerLegend(bool bUseGamePosition);
    void RefreshLobbyRoster();
    void RefreshNetworkStatus();
    void RefreshSearchCount();
    void SetOperationResultText(
        EListenServerOperation Operation,
        const FListenServerOperationResult& Result
    );
    void ApplyConnectionUI(
        EListenServerConnectionState ConnectionState,
        EListenServerRole Role
    );
    void UpdateDetailsVisibility();

    UFUNCTION()
    void HandleNetworkStateChanged(
        EListenServerRole Role,
        EListenServerConnectionState ConnectionState,
        EListenServerOperation Operation
    );

    UFUNCTION()
    void HandleLobbyRosterChanged();

    UFUNCTION()
    void HandleOperationCompleted(
        EListenServerOperation CompletedOperation,
        const FListenServerOperationResult& Result
    );

    UFUNCTION()
    void HandleSearchResultsChanged();

    UFUNCTION()
    void HandleNetworkFailure(const FListenServerOperationResult& Result);

    UFUNCTION()
    void HandleHostClicked();

    UFUNCTION()
    void HandleFindClicked();

    UFUNCTION()
    void HandleJoinFirstClicked();

    UFUNCTION()
    void HandleQuickMatchClicked();

    UFUNCTION()
    void HandleInviteClicked();

    UFUNCTION()
    void HandleStartGameClicked();

    UFUNCTION()
    void HandleLeaveClicked();

    UFUNCTION()
    void HandleToggleDetailsClicked();

    UFUNCTION()
    void HandleRetryClicked();

    UFUNCTION()
    void HandleGameLeaveClicked();

    UPROPERTY(Transient)
    TObjectPtr<UListenServerSessionSubsystem> NetworkSubsystem;

    TWeakObjectPtr<ACMGameState> BoundGameState;

    bool bGameDetailsVisible = false;
    EListenServerConnectionState CurrentConnectionState =
        EListenServerConnectionState::Offline;
};
