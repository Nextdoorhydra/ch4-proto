#include "ChimeraNetworkWidget.h"

#include "Game/CMGameState.h"
#include "Player/CMPlayerState.h"
#include "Player/CMPlayerController.h"
#include "ChimeraLobbyPlayerRowWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "ListenServerSessionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraUI, Log, All);

void UChimeraNetworkWidget::NativeConstruct()
{
    Super::NativeConstruct();

    NetworkSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UListenServerSessionSubsystem>()
        : nullptr;
    if (NetworkSubsystem)
    {
        NetworkSubsystem->OnStateChanged.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleNetworkStateChanged
        );
        NetworkSubsystem->OnOperationCompleted.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleOperationCompleted
        );
        NetworkSubsystem->OnSearchResultsChanged.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleSearchResultsChanged
        );
        NetworkSubsystem->OnNetworkFailure.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleNetworkFailure
        );
    }

    if (Btn_Host)
    {
        Btn_Host->OnClicked.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleHostClicked
        );
    }
    if (Btn_Find)
    {
        Btn_Find->OnClicked.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleFindClicked
        );
    }
    if (Btn_JoinFirst)
    {
        Btn_JoinFirst->OnClicked.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleJoinFirstClicked
        );
    }
    if (Btn_QuickMatch)
    {
        Btn_QuickMatch->OnClicked.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleQuickMatchClicked
        );
    }
    if (Btn_Invite)
    {
        Btn_Invite->OnClicked.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleInviteClicked
        );
    }
    if (Btn_StartGame)
    {
        Btn_StartGame->OnClicked.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleStartGameClicked
        );
    }
    if (Btn_Leave)
    {
        Btn_Leave->OnClicked.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleLeaveClicked
        );
    }
    if (Btn_ToggleDetails)
    {
        Btn_ToggleDetails->OnClicked.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleToggleDetailsClicked
        );
    }
    if (Btn_Retry)
    {
        Btn_Retry->OnClicked.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleRetryClicked
        );
    }
    if (Btn_GameLeave)
    {
        Btn_GameLeave->OnClicked.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleGameLeaveClicked
        );
    }

    BindCurrentGameState();
    RefreshLobbyRoster();
    RefreshNetworkStatus();
    RefreshSearchCount();

    const EListenServerConnectionState ConnectionState = NetworkSubsystem
        ? NetworkSubsystem->GetConnectionState()
        : EListenServerConnectionState::Offline;
    const EListenServerRole Role = NetworkSubsystem
        ? NetworkSubsystem->GetCurrentRole()
        : EListenServerRole::None;
    ApplyConnectionUI(ConnectionState, Role);

    UE_LOG(
        LogChimeraUI,
        Log,
        TEXT("Network widget ready. StartButton=%d LeaveButton=%d Role=%d State=%d"),
        IsValid(Btn_StartGame),
        IsValid(Btn_Leave),
        static_cast<int32>(Role),
        static_cast<int32>(ConnectionState)
    );
}

void UChimeraNetworkWidget::NativeDestruct()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->OnStateChanged.RemoveDynamic(
            this,
            &UChimeraNetworkWidget::HandleNetworkStateChanged
        );
        NetworkSubsystem->OnOperationCompleted.RemoveDynamic(
            this,
            &UChimeraNetworkWidget::HandleOperationCompleted
        );
        NetworkSubsystem->OnSearchResultsChanged.RemoveDynamic(
            this,
            &UChimeraNetworkWidget::HandleSearchResultsChanged
        );
        NetworkSubsystem->OnNetworkFailure.RemoveDynamic(
            this,
            &UChimeraNetworkWidget::HandleNetworkFailure
        );
    }
    if (BoundGameState.IsValid())
    {
        BoundGameState->OnLobbyRosterChanged.RemoveDynamic(
            this,
            &UChimeraNetworkWidget::HandleLobbyRosterChanged
        );
    }

    Super::NativeDestruct();
}

void UChimeraNetworkWidget::BindCurrentGameState()
{
    ACMGameState* CurrentGameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr;
    if (BoundGameState == CurrentGameState)
    {
        return;
    }

    if (BoundGameState.IsValid())
    {
        BoundGameState->OnLobbyRosterChanged.RemoveDynamic(
            this,
            &UChimeraNetworkWidget::HandleLobbyRosterChanged
        );
    }

    BoundGameState = CurrentGameState;
    if (BoundGameState.IsValid())
    {
        BoundGameState->OnLobbyRosterChanged.AddUniqueDynamic(
            this,
            &UChimeraNetworkWidget::HandleLobbyRosterChanged
        );
    }
}

void UChimeraNetworkWidget::MovePlayerLegend(bool bUseGamePosition)
{
    if (!Panel_PlayerLegend)
    {
        return;
    }

    UPanelWidget* TargetHost = bUseGamePosition
        ? Box_GameLegendHost
        : Box_LobbyLegendHost;
    if (!TargetHost || Panel_PlayerLegend->GetParent() == TargetHost)
    {
        return;
    }

    Panel_PlayerLegend->RemoveFromParent();
    TargetHost->AddChild(Panel_PlayerLegend);
}

void UChimeraNetworkWidget::RefreshLobbyRoster()
{
    BindCurrentGameState();
    if (!BoundGameState.IsValid())
    {
        return;
    }

    if (Txt_PlayerCount)
    {
        Txt_PlayerCount->SetText(FText::Format(
            NSLOCTEXT("ChimeraUI", "LobbyPlayerCount", "{0} / {1}"),
            FText::AsNumber(BoundGameState->GetLobbyPlayerCount()),
            FText::AsNumber(BoundGameState->GetLobbyMaxPlayers())
        ));
    }

    if (!SB_LobbyPlayers)
    {
        return;
    }

    SB_LobbyPlayers->ClearChildren();
    if (!LobbyPlayerRowClass)
    {
        return;
    }

    for (APlayerState* PlayerState : BoundGameState->PlayerArray)
    {
        const ACMPlayerState* CMPlayerState =
            Cast<ACMPlayerState>(PlayerState);
        if (!CMPlayerState
            || CMPlayerState->IsOnlyASpectator())
        {
            continue;
        }

        APlayerController* OwningPlayer = GetOwningPlayer();
        if (!OwningPlayer && GetWorld())
        {
            OwningPlayer = GetWorld()->GetFirstPlayerController();
        }

        UChimeraLobbyPlayerRowWidget* PlayerRow =
            OwningPlayer
                ? CreateWidget<UChimeraLobbyPlayerRowWidget>(
                    OwningPlayer,
                    LobbyPlayerRowClass
                )
                : nullptr;
        if (PlayerRow)
        {
            PlayerRow->SetPlayerDisplayName(
                CMPlayerState->GetPlayerName(),
                CMPlayerState->GetPlayerColor()
            );
            if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(
                SB_LobbyPlayers->AddChild(PlayerRow)
            ))
            {
                RowSlot->SetPadding(FMargin(0.0f));
                RowSlot->SetHorizontalAlignment(HAlign_Fill);
                RowSlot->SetVerticalAlignment(VAlign_Center);
            }
        }
    }
}

void UChimeraNetworkWidget::RefreshNetworkStatus()
{
    if (!NetworkSubsystem || !Txt_State)
    {
        return;
    }

    const FListenServerDebugSnapshot Snapshot =
        NetworkSubsystem->GetDebugSnapshot();
    Txt_State->SetText(FText::Format(
        NSLOCTEXT(
            "ChimeraUI",
            "NetworkState",
            "Role: {0} | State: {1} | Operation: {2}"
        ),
        StaticEnum<EListenServerRole>()->GetDisplayNameTextByValue(
            static_cast<int64>(Snapshot.Role)
        ),
        StaticEnum<EListenServerConnectionState>()->GetDisplayNameTextByValue(
            static_cast<int64>(Snapshot.ConnectionState)
        ),
        StaticEnum<EListenServerOperation>()->GetDisplayNameTextByValue(
            static_cast<int64>(Snapshot.Operation)
        )
    ));
}

void UChimeraNetworkWidget::RefreshSearchCount()
{
    if (NetworkSubsystem && Txt_SearchCount)
    {
        Txt_SearchCount->SetText(FText::Format(
            NSLOCTEXT("ChimeraUI", "SearchCount", "Search results: {0}"),
            FText::AsNumber(NetworkSubsystem->GetSearchResultCount())
        ));
    }
}

void UChimeraNetworkWidget::SetOperationResultText(
    EListenServerOperation Operation,
    const FListenServerOperationResult& Result
)
{
    if (!Txt_Result)
    {
        return;
    }

    const FText OperationText =
        StaticEnum<EListenServerOperation>()->GetDisplayNameTextByValue(
            static_cast<int64>(Operation)
        );
    FText Message = Result.UserMessage;
    if (Message.IsEmpty())
    {
        Message = Result.bSucceeded
            ? NSLOCTEXT("ChimeraUI", "OperationSucceeded", "Succeeded")
            : StaticEnum<EListenServerError>()->GetDisplayNameTextByValue(
                static_cast<int64>(Result.Error)
            );
    }

    Txt_Result->SetText(FText::Format(
        NSLOCTEXT("ChimeraUI", "OperationResult", "{0}: {1}"),
        OperationText,
        Message
    ));
}

void UChimeraNetworkWidget::ApplyConnectionUI(
    EListenServerConnectionState ConnectionState,
    EListenServerRole Role
)
{
    CurrentConnectionState = ConnectionState;
    const bool bOffline =
        ConnectionState == EListenServerConnectionState::Offline;
    const bool bLobby =
        ConnectionState == EListenServerConnectionState::Lobby;
    const bool bInGame =
        ConnectionState == EListenServerConnectionState::InGame;
    const bool bHost = Role == EListenServerRole::Host;
    const bool bIdle = !NetworkSubsystem
        || NetworkSubsystem->GetCurrentOperation()
            == EListenServerOperation::None;

    MovePlayerLegend(bInGame);

    if (Panel_Menu)
    {
        Panel_Menu->SetVisibility(
            bOffline ? ESlateVisibility::Visible : ESlateVisibility::Collapsed
        );
    }
    if (Panel_Lobby)
    {
        Panel_Lobby->SetVisibility(
            bLobby ? ESlateVisibility::Visible : ESlateVisibility::Collapsed
        );
    }
    if (Panel_GameActionBar)
    {
        Panel_GameActionBar->SetVisibility(
            bInGame ? ESlateVisibility::Visible : ESlateVisibility::Collapsed
        );
    }
    if (Panel_PlayerLegend)
    {
        Panel_PlayerLegend->SetVisibility(
            bLobby || bInGame
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Collapsed
        );
    }
    if (Box_LobbyLegendHost)
    {
        Box_LobbyLegendHost->SetVisibility(
            bLobby
                ? ESlateVisibility::SelfHitTestInvisible
                : ESlateVisibility::Collapsed
        );
    }
    if (Box_GameLegendHost)
    {
        Box_GameLegendHost->SetVisibility(
            bInGame
                ? ESlateVisibility::SelfHitTestInvisible
                : ESlateVisibility::Collapsed
        );
    }
    if (Btn_StartGame)
    {
        Btn_StartGame->SetVisibility(
            bLobby && bHost
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed
        );
        Btn_StartGame->SetIsEnabled(bLobby && bHost && bIdle);
    }
    if (Btn_Retry)
    {
        Btn_Retry->SetVisibility(
            bInGame && bHost
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed
        );
        Btn_Retry->SetIsEnabled(bInGame && bHost && bIdle);
    }
    if (Btn_Host)
    {
        Btn_Host->SetIsEnabled(bOffline && bIdle);
    }
    if (Btn_Find)
    {
        Btn_Find->SetIsEnabled(bOffline && bIdle);
    }
    if (Btn_JoinFirst)
    {
        Btn_JoinFirst->SetIsEnabled(
            bOffline
            && bIdle
            && NetworkSubsystem
            && NetworkSubsystem->GetSearchResultCount() > 0
        );
    }
    if (Btn_QuickMatch)
    {
        Btn_QuickMatch->SetIsEnabled(bOffline && bIdle);
    }
    if (Btn_Invite)
    {
        Btn_Invite->SetIsEnabled(bLobby && bIdle);
    }
    if (Btn_Leave)
    {
        Btn_Leave->SetIsEnabled(bLobby && bIdle);
    }
    if (Btn_GameLeave)
    {
        Btn_GameLeave->SetIsEnabled(bInGame && bIdle);
    }
    if (Btn_ToggleDetails)
    {
        Btn_ToggleDetails->SetIsEnabled(bInGame);
    }

    bGameDetailsVisible = !bInGame;
    UpdateDetailsVisibility();

    if (bLobby)
    {
        RefreshLobbyRoster();
    }
}

void UChimeraNetworkWidget::UpdateDetailsVisibility()
{
    if (Panel_Details)
    {
        Panel_Details->SetVisibility(
            bGameDetailsVisible
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Collapsed
        );
    }

    if (Txt_ToggleDetails)
    {
        Txt_ToggleDetails->SetText(
            bGameDetailsVisible ? HideDetailsText : ShowDetailsText
        );
    }
}

void UChimeraNetworkWidget::HandleNetworkStateChanged(
    EListenServerRole Role,
    EListenServerConnectionState ConnectionState,
    EListenServerOperation Operation
)
{
    ApplyConnectionUI(ConnectionState, Role);
    RefreshNetworkStatus();
}

void UChimeraNetworkWidget::HandleLobbyRosterChanged()
{
    RefreshLobbyRoster();
}

void UChimeraNetworkWidget::HandleOperationCompleted(
    EListenServerOperation CompletedOperation,
    const FListenServerOperationResult& Result
)
{
    SetOperationResultText(CompletedOperation, Result);
    RefreshNetworkStatus();
    RefreshSearchCount();
    if (NetworkSubsystem)
    {
        ApplyConnectionUI(
            NetworkSubsystem->GetConnectionState(),
            NetworkSubsystem->GetCurrentRole()
        );
    }
}

void UChimeraNetworkWidget::HandleSearchResultsChanged()
{
    RefreshSearchCount();
    if (NetworkSubsystem)
    {
        ApplyConnectionUI(
            NetworkSubsystem->GetConnectionState(),
            NetworkSubsystem->GetCurrentRole()
        );
    }
}

void UChimeraNetworkWidget::HandleNetworkFailure(
    const FListenServerOperationResult& Result
)
{
    SetOperationResultText(EListenServerOperation::Recovering, Result);
    RefreshNetworkStatus();
}

void UChimeraNetworkWidget::HandleHostClicked()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->HostDefaultSession();
    }
}

void UChimeraNetworkWidget::HandleFindClicked()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->FindDefaultSessions();
    }
}

void UChimeraNetworkWidget::HandleJoinFirstClicked()
{
    if (!NetworkSubsystem)
    {
        return;
    }

    FListenServerSearchResult SearchResult;
    if (NetworkSubsystem->GetSearchResultByIndex(0, SearchResult))
    {
        NetworkSubsystem->JoinSession(SearchResult.Handle);
    }
    else if (Txt_Result)
    {
        Txt_Result->SetText(
            NSLOCTEXT("ChimeraUI", "NoSearchResult", "No session found.")
        );
    }
}

void UChimeraNetworkWidget::HandleQuickMatchClicked()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->QuickMatchDefault();
    }
}

void UChimeraNetworkWidget::HandleInviteClicked()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->ShowInviteUI();
    }
}

void UChimeraNetworkWidget::HandleStartGameClicked()
{
    if (NetworkSubsystem)
    {
        const bool bStarted =
            NetworkSubsystem->HostTravelToDefaultGameMap();
        UE_LOG(
            LogChimeraUI,
            Log,
            TEXT("Start Game clicked. Accepted=%d Role=%d State=%d Operation=%d"),
            bStarted,
            static_cast<int32>(NetworkSubsystem->GetCurrentRole()),
            static_cast<int32>(NetworkSubsystem->GetConnectionState()),
            static_cast<int32>(NetworkSubsystem->GetCurrentOperation())
        );
    }
}

void UChimeraNetworkWidget::HandleLeaveClicked()
{
    UE_LOG(LogChimeraUI, Log, TEXT("Lobby Leave clicked."));
    HandleGameLeaveClicked();
}

void UChimeraNetworkWidget::HandleToggleDetailsClicked()
{
    if (CurrentConnectionState != EListenServerConnectionState::InGame)
    {
        return;
    }

    bGameDetailsVisible = !bGameDetailsVisible;
    UpdateDetailsVisibility();
}

void UChimeraNetworkWidget::HandleRetryClicked()
{
    APlayerController* OwningPlayer = GetOwningPlayer();
    if (!OwningPlayer && GetWorld())
    {
        OwningPlayer = GetWorld()->GetFirstPlayerController();
    }

    if (ACMPlayerController* PlayerController =
        Cast<ACMPlayerController>(OwningPlayer))
    {
        PlayerController->RequestRetryGame();
    }
}

void UChimeraNetworkWidget::HandleGameLeaveClicked()
{
    if (NetworkSubsystem)
    {
        const bool bStarted = NetworkSubsystem->LeaveAndReturnToMenu();
        UE_LOG(
            LogChimeraUI,
            Log,
            TEXT("Leave clicked. Accepted=%d Role=%d State=%d Operation=%d"),
            bStarted,
            static_cast<int32>(NetworkSubsystem->GetCurrentRole()),
            static_cast<int32>(NetworkSubsystem->GetConnectionState()),
            static_cast<int32>(NetworkSubsystem->GetCurrentOperation())
        );
    }
}
