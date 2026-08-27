#include "CMNetworkTestWidget.h"

#include "GameMode/CMGameState.h"
#include "GameMode/Lobby/CMLobbyGameState.h"
#include "Player/CMPlayerState.h"
#include "Player/CMPlayerController.h"
#include "CMLobbyPlayerRowWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "ListenServerSessionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraUI, Log, All);

void UCMNetworkTestWidget::NativeConstruct()
{
    Super::NativeConstruct();

    NetworkSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UListenServerSessionSubsystem>()
        : nullptr;
    if (NetworkSubsystem)
    {
        NetworkSubsystem->OnStateChanged.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleNetworkStateChanged
        );
        NetworkSubsystem->OnOperationCompleted.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleOperationCompleted
        );
        NetworkSubsystem->OnSearchResultsChanged.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleSearchResultsChanged
        );
        NetworkSubsystem->OnNetworkFailure.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleNetworkFailure
        );
    }

    if (Btn_Host)
    {
        Btn_Host->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleHostClicked
        );
    }
    if (Btn_Find)
    {
        Btn_Find->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleFindClicked
        );
    }
    if (Btn_JoinFirst)
    {
        Btn_JoinFirst->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleJoinFirstClicked
        );
    }
    if (Btn_QuickMatch)
    {
        Btn_QuickMatch->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleQuickMatchClicked
        );
    }
    if (Btn_Invite)
    {
        Btn_Invite->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleInviteClicked
        );
    }
    if (Btn_StartGame)
    {
        Btn_StartGame->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleStartGameClicked
        );
    }
    if (Btn_StartTestGame)
    {
        Btn_StartTestGame->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleStartTestGameClicked
        );
    }
    if (Btn_Ready)
    {
        Btn_Ready->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleReadyClicked
        );
    }
    if (Btn_Leave)
    {
        Btn_Leave->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleLeaveClicked
        );
    }
    if (Btn_ToggleDetails)
    {
        Btn_ToggleDetails->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleToggleDetailsClicked
        );
    }
    if (Btn_Retry)
    {
        Btn_Retry->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleRetryClicked
        );
    }
    if (Btn_GameLeave)
    {
        Btn_GameLeave->OnClicked.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleGameLeaveClicked
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

void UCMNetworkTestWidget::NativeDestruct()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->OnStateChanged.RemoveDynamic(
            this,
            &UCMNetworkTestWidget::HandleNetworkStateChanged
        );
        NetworkSubsystem->OnOperationCompleted.RemoveDynamic(
            this,
            &UCMNetworkTestWidget::HandleOperationCompleted
        );
        NetworkSubsystem->OnSearchResultsChanged.RemoveDynamic(
            this,
            &UCMNetworkTestWidget::HandleSearchResultsChanged
        );
        NetworkSubsystem->OnNetworkFailure.RemoveDynamic(
            this,
            &UCMNetworkTestWidget::HandleNetworkFailure
        );
    }
    if (BoundGameState.IsValid())
    {
        BoundGameState->OnLobbyRosterChanged.RemoveDynamic(
            this,
            &UCMNetworkTestWidget::HandleLobbyRosterChanged
        );
    }

    Super::NativeDestruct();
}

void UCMNetworkTestWidget::BindCurrentGameState()
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
            &UCMNetworkTestWidget::HandleLobbyRosterChanged
        );
    }

    BoundGameState = CurrentGameState;
    if (BoundGameState.IsValid())
    {
        BoundGameState->OnLobbyRosterChanged.AddUniqueDynamic(
            this,
            &UCMNetworkTestWidget::HandleLobbyRosterChanged
        );
    }
}

void UCMNetworkTestWidget::MovePlayerLegend(bool bUseGamePosition)
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

void UCMNetworkTestWidget::RefreshLobbyRoster()
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

        UCMLobbyPlayerRowWidget* PlayerRow =
            OwningPlayer
                ? CreateWidget<UCMLobbyPlayerRowWidget>(
                    OwningPlayer,
                    LobbyPlayerRowClass
                )
                : nullptr;
        if (PlayerRow)
        {
            PlayerRow->SetPlayerLobbyState(
                CMPlayerState->GetPlayerName(),
                CMPlayerState->GetPlayerColor(),
                CMPlayerState->IsReady()
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

void UCMNetworkTestWidget::RefreshNetworkStatus()
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

void UCMNetworkTestWidget::RefreshSearchCount()
{
    if (NetworkSubsystem && Txt_SearchCount)
    {
        Txt_SearchCount->SetText(FText::Format(
            NSLOCTEXT("ChimeraUI", "SearchCount", "Search results: {0}"),
            FText::AsNumber(NetworkSubsystem->GetSearchResultCount())
        ));
    }
}

void UCMNetworkTestWidget::SetOperationResultText(
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

void UCMNetworkTestWidget::ApplyConnectionUI(
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
    const ACMLobbyGameState* LobbyState = GetWorld()
        ? GetWorld()->GetGameState<ACMLobbyGameState>()
        : nullptr;
    const bool bCanStartGame = LobbyState && LobbyState->CanStartGame();
    const bool bCanStartTestGame =
        LobbyState && LobbyState->CanStartTestGame();
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
        Btn_StartGame->SetIsEnabled(
            bLobby && bHost && bIdle && bCanStartGame);
    }
    if (Btn_StartTestGame)
    {
        Btn_StartTestGame->SetVisibility(
            bLobby && bHost
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed
        );
        Btn_StartTestGame->SetIsEnabled(
            bLobby && bHost && bIdle && bCanStartTestGame);
    }
    if (Btn_Ready)
    {
        Btn_Ready->SetIsEnabled(bLobby && bIdle);
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

void UCMNetworkTestWidget::UpdateDetailsVisibility()
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

void UCMNetworkTestWidget::HandleNetworkStateChanged(
    EListenServerRole Role,
    EListenServerConnectionState ConnectionState,
    EListenServerOperation Operation
)
{
    ApplyConnectionUI(ConnectionState, Role);
    RefreshNetworkStatus();
}

void UCMNetworkTestWidget::HandleLobbyRosterChanged()
{
    RefreshLobbyRoster();
    if (NetworkSubsystem)
    {
        ApplyConnectionUI(
            NetworkSubsystem->GetConnectionState(),
            NetworkSubsystem->GetCurrentRole());
    }
}

void UCMNetworkTestWidget::HandleOperationCompleted(
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

void UCMNetworkTestWidget::HandleSearchResultsChanged()
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

void UCMNetworkTestWidget::HandleNetworkFailure(
    const FListenServerOperationResult& Result
)
{
    SetOperationResultText(EListenServerOperation::Recovering, Result);
    RefreshNetworkStatus();
}

void UCMNetworkTestWidget::HandleHostClicked()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->HostDefaultSession();
    }
}

void UCMNetworkTestWidget::HandleFindClicked()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->FindDefaultSessions();
    }
}

void UCMNetworkTestWidget::HandleJoinFirstClicked()
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

void UCMNetworkTestWidget::HandleQuickMatchClicked()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->QuickMatchDefault();
    }
}

void UCMNetworkTestWidget::HandleInviteClicked()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->ShowInviteUI();
    }
}

void UCMNetworkTestWidget::HandleStartGameClicked()
{
    if (ACMPlayerController* PlayerController =
        Cast<ACMPlayerController>(GetOwningPlayer()))
    {
        PlayerController->RequestStartStageRoute();
    }
}

// 테스트 시작 버튼을 서버의 TestRoute 검증 흐름으로 전달
void UCMNetworkTestWidget::HandleStartTestGameClicked()
{
    if (ACMPlayerController* PlayerController =
        Cast<ACMPlayerController>(GetOwningPlayer()))
    {
        PlayerController->RequestStartTestStageRoute();
    }
}

// 현재 로컬 플레이어의 Ready 상태를 반전해 서버에 요청
void UCMNetworkTestWidget::HandleReadyClicked()
{
    ACMPlayerController* PlayerController =
        Cast<ACMPlayerController>(GetOwningPlayer());
    const ACMPlayerState* PlayerState = PlayerController
        ? PlayerController->GetPlayerState<ACMPlayerState>()
        : nullptr;
    if (PlayerController && PlayerState)
    {
        PlayerController->RequestSetReady(!PlayerState->IsReady());
    }
}

void UCMNetworkTestWidget::HandleLeaveClicked()
{
    UE_LOG(LogChimeraUI, Log, TEXT("Lobby Leave clicked."));
    HandleGameLeaveClicked();
}

void UCMNetworkTestWidget::HandleToggleDetailsClicked()
{
    if (CurrentConnectionState != EListenServerConnectionState::InGame)
    {
        return;
    }

    bGameDetailsVisible = !bGameDetailsVisible;
    UpdateDetailsVisibility();
}

void UCMNetworkTestWidget::HandleRetryClicked()
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

void UCMNetworkTestWidget::HandleGameLeaveClicked()
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
