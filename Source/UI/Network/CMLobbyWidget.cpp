#include "CMLobbyWidget.h"

#include "CMLobbyPlayerRowWidget.h"
#include "CMRoomId.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "GameMode/CMGameState.h"
#include "GameMode/Lobby/CMLobbyGameState.h"
#include "ListenServerSessionSubsystem.h"
#include "Player/CMPlayerController.h"
#include "Player/CMPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraLobbyUI, Log, All);

void UCMLobbyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    NetworkSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UListenServerSessionSubsystem>()
        : nullptr;
    if (NetworkSubsystem)
    {
        NetworkSubsystem->OnStateChanged.AddUniqueDynamic(
            this,
            &UCMLobbyWidget::HandleNetworkStateChanged
        );
        NetworkSubsystem->OnOperationCompleted.AddUniqueDynamic(
            this,
            &UCMLobbyWidget::HandleOperationCompleted
        );
    }

    if (Btn_StartGame)
    {
        Btn_StartGame->OnClicked.AddUniqueDynamic(
            this,
            &UCMLobbyWidget::HandleStartGameClicked
        );
    }
    if (Btn_StartTestGame)
    {
        Btn_StartTestGame->OnClicked.AddUniqueDynamic(
            this,
            &UCMLobbyWidget::HandleStartTestGameClicked
        );
    }
    if (Btn_Ready)
    {
        Btn_Ready->OnClicked.AddUniqueDynamic(
            this,
            &UCMLobbyWidget::HandleReadyClicked
        );
    }
    if (Btn_Invite)
    {
        Btn_Invite->OnClicked.AddUniqueDynamic(
            this,
            &UCMLobbyWidget::HandleInviteClicked
        );
    }
    if (Btn_Leave)
    {
        Btn_Leave->OnClicked.AddUniqueDynamic(
            this,
            &UCMLobbyWidget::HandleLeaveClicked
        );
    }

    BindCurrentGameState();
    RefreshRoomId();
    RefreshLobbyRoster();
    UpdateControls();
}

void UCMLobbyWidget::NativeDestruct()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->OnStateChanged.RemoveDynamic(
            this,
            &UCMLobbyWidget::HandleNetworkStateChanged
        );
        NetworkSubsystem->OnOperationCompleted.RemoveDynamic(
            this,
            &UCMLobbyWidget::HandleOperationCompleted
        );
    }
    if (BoundGameState.IsValid())
    {
        BoundGameState->OnLobbyRosterChanged.RemoveDynamic(
            this,
            &UCMLobbyWidget::HandleLobbyRosterChanged
        );
    }

    Super::NativeDestruct();
}

void UCMLobbyWidget::BindCurrentGameState()
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
            &UCMLobbyWidget::HandleLobbyRosterChanged
        );
    }

    BoundGameState = CurrentGameState;
    if (BoundGameState.IsValid())
    {
        BoundGameState->OnLobbyRosterChanged.AddUniqueDynamic(
            this,
            &UCMLobbyWidget::HandleLobbyRosterChanged
        );
    }
}

void UCMLobbyWidget::RefreshRoomId()
{
    if (!Txt_RoomId)
    {
        return;
    }

    FString RoomId;
    const bool bHasRoomId = NetworkSubsystem
        && NetworkSubsystem->GetCurrentSessionAttribute(
            CMRoomId::AttributeKey(),
            RoomId
        );
    Txt_RoomId->SetText(FText::FromString(
        bHasRoomId ? RoomId : TEXT("------")
    ));
    Txt_RoomId->SetVisibility(ESlateVisibility::Visible);
}

void UCMLobbyWidget::RefreshLobbyRoster()
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

    APlayerController* OwningPlayer = GetOwningPlayer();
    if (!OwningPlayer && GetWorld())
    {
        OwningPlayer = GetWorld()->GetFirstPlayerController();
    }
    for (APlayerState* PlayerState : BoundGameState->PlayerArray)
    {
        const ACMPlayerState* CMPlayerState =
            Cast<ACMPlayerState>(PlayerState);
        if (!CMPlayerState || CMPlayerState->IsOnlyASpectator())
        {
            continue;
        }

        UCMLobbyPlayerRowWidget* PlayerRow = OwningPlayer
            ? CreateWidget<UCMLobbyPlayerRowWidget>(
                OwningPlayer,
                LobbyPlayerRowClass
            )
            : nullptr;
        if (!PlayerRow)
        {
            continue;
        }

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

void UCMLobbyWidget::UpdateControls()
{
    const EListenServerConnectionState ConnectionState = NetworkSubsystem
        ? NetworkSubsystem->GetConnectionState()
        : EListenServerConnectionState::Offline;
    const EListenServerRole Role = NetworkSubsystem
        ? NetworkSubsystem->GetCurrentRole()
        : EListenServerRole::None;
    const bool bLobby =
        ConnectionState == EListenServerConnectionState::Lobby;
    const bool bIdle = NetworkSubsystem
        && NetworkSubsystem->GetCurrentOperation()
            == EListenServerOperation::None;
    const bool bHost = Role == EListenServerRole::Host;
    const ACMLobbyGameState* LobbyState = GetWorld()
        ? GetWorld()->GetGameState<ACMLobbyGameState>()
        : nullptr;
    const bool bCanStartGame = LobbyState && LobbyState->CanStartGame();
    const ACMPlayerState* LocalPlayerState = GetOwningPlayer()
        ? GetOwningPlayer()->GetPlayerState<ACMPlayerState>()
        : nullptr;

    if (Btn_StartGame)
    {
        Btn_StartGame->SetVisibility(
            bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed
        );
        Btn_StartGame->SetIsEnabled(
            bLobby && bHost && bIdle && bCanStartGame);
    }
    if (Btn_StartTestGame)
    {
        Btn_StartTestGame->SetVisibility(
            bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed
        );
        Btn_StartTestGame->SetIsEnabled(
            bLobby && bHost && bIdle && bCanStartGame);
    }
    if (Btn_Ready)
    {
        Btn_Ready->SetIsEnabled(bLobby && bIdle && LocalPlayerState);
    }
    if (Txt_ReadyState)
    {
        Txt_ReadyState->SetText(
            LocalPlayerState && LocalPlayerState->IsReady()
                ? NSLOCTEXT("ChimeraUI", "CancelReady", "Unready")
                : NSLOCTEXT("ChimeraUI", "Ready", "Ready")
        );
    }
    if (Btn_Invite)
    {
        Btn_Invite->SetIsEnabled(bLobby && bIdle);
    }
    if (Btn_Leave)
    {
        Btn_Leave->SetIsEnabled(bLobby && bIdle);
    }
}

void UCMLobbyWidget::SetResultText(const FText& Message)
{
    if (Txt_Result)
    {
        Txt_Result->SetText(Message);
    }
}

void UCMLobbyWidget::HandleNetworkStateChanged(
    EListenServerRole,
    EListenServerConnectionState,
    EListenServerOperation
)
{
    RefreshRoomId();
    RefreshLobbyRoster();
    UpdateControls();
}

void UCMLobbyWidget::HandleOperationCompleted(
    EListenServerOperation,
    const FListenServerOperationResult& Result
)
{
    SetResultText(Result.UserMessage);
    RefreshRoomId();
    UpdateControls();
}

void UCMLobbyWidget::HandleLobbyRosterChanged()
{
    RefreshLobbyRoster();
    UpdateControls();
}

// 정식 시작 버튼을 서버의 StageRoute 검증 흐름으로 전달
void UCMLobbyWidget::HandleStartGameClicked()
{
    if (ACMPlayerController* PlayerController =
        Cast<ACMPlayerController>(GetOwningPlayer()))
    {
        PlayerController->RequestStartStageRoute();
    }
}

// 테스트 시작 버튼을 서버의 TestRoute 검증 흐름으로 전달
void UCMLobbyWidget::HandleStartTestGameClicked()
{
    if (ACMPlayerController* PlayerController =
        Cast<ACMPlayerController>(GetOwningPlayer()))
    {
        PlayerController->RequestStartTestStageRoute();
    }
}

// 현재 로컬 플레이어의 Ready 상태를 반전해 서버에 요청
void UCMLobbyWidget::HandleReadyClicked()
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

void UCMLobbyWidget::HandleInviteClicked()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->ShowInviteUI();
    }
}

void UCMLobbyWidget::HandleLeaveClicked()
{
    if (NetworkSubsystem)
    {
        NetworkSubsystem->LeaveAndReturnToMenu();
    }
}
