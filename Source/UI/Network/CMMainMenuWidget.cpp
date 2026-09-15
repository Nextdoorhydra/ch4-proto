#include "CMMainMenuWidget.h"

#include "CMRoomId.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ListenServerNetworkSettings.h"
#include "ListenServerSessionSubsystem.h"
#include "Menu/CMMenuButtonWidget.h"
#include "Option/CMOptionWidget.h"
#include "UI/NKMUITagList.h"
#include "UI/Subsystem/NKMUIManagerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraMainMenuUI, Log, All);

namespace
{
    FListenServerHostRequest MakeDefaultHostRequest()
    {
        const UListenServerNetworkSettings* Settings =
            GetDefault<UListenServerNetworkSettings>();
        FListenServerHostRequest Request;
        Request.MaxPlayers = Settings->DefaultMaxPlayers;
        Request.GameModeId = Settings->DefaultGameModeId;
        Request.Region = Settings->DefaultRegion;
        Request.SessionDisplayName = Settings->DefaultSessionDisplayName;
        Request.LobbyMap = Settings->LobbyMap;
        return Request;
    }

    FListenServerSearchRequest MakeDefaultSearchRequest()
    {
        const UListenServerNetworkSettings* Settings =
            GetDefault<UListenServerNetworkSettings>();
        FListenServerSearchRequest Request;
        Request.MaxSearchResults = Settings->DefaultMaxSearchResults;
        Request.GameModeId = Settings->DefaultGameModeId;
        Request.Region = Settings->DefaultRegion;
        return Request;
    }
}

void UCMMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (OptionWidgetClass.IsNull())
    {
        OptionWidgetClass = TSoftClassPtr<UCMOptionWidget>(FSoftObjectPath(
            TEXT("/Game/Chimera/UI/Option/WBP_CMOption.WBP_CMOption_C")));
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UNKMUIManagerSubsystem* UIManager =
            GameInstance->GetSubsystem<UNKMUIManagerSubsystem>())
        {
            UIManager->InitializePolicy(GetOwningLocalPlayer());
        }
    }

    NetworkSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UListenServerSessionSubsystem>()
        : nullptr;
    if (NetworkSubsystem)
    {
        NetworkSubsystem->OnStateChanged.AddUniqueDynamic(
            this,
            &UCMMainMenuWidget::HandleNetworkStateChanged
        );
        NetworkSubsystem->OnOperationCompleted.AddUniqueDynamic(
            this,
            &UCMMainMenuWidget::HandleOperationCompleted
        );
    }

    if (Btn_Options)
    {
        Btn_Options->OnClicked.AddUniqueDynamic(
            this,
            &UCMMainMenuWidget::HandleOptionsClicked
        );
    }
    if (Btn_CreateRoom)
    {
        Btn_CreateRoom->OnClicked.AddUniqueDynamic(
            this,
            &UCMMainMenuWidget::HandleCreateRoomClicked
        );
    }
    if (Btn_JoinRoom)
    {
        Btn_JoinRoom->OnClicked.AddUniqueDynamic(
            this,
            &UCMMainMenuWidget::HandleJoinRoomClicked
        );
    }
    if (Btn_QuickMatch)
    {
        Btn_QuickMatch->OnClicked.AddUniqueDynamic(
            this,
            &UCMMainMenuWidget::HandleQuickMatchClicked
        );
    }
    if (Btn_Quit)
    {
        Btn_Quit->OnClicked.AddUniqueDynamic(
            this,
            &UCMMainMenuWidget::HandleQuitClicked
        );
    }
    if (Panel_JoinRoomPopup)
    {
        Panel_JoinRoomPopup->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (Btn_JoinConfirm)
    {
        Btn_JoinConfirm->OnClicked.AddUniqueDynamic(
            this,
            &UCMMainMenuWidget::HandleJoinConfirmClicked
        );
    }

    if (Btn_JoinCancel)
    {
        Btn_JoinCancel->OnClicked.AddUniqueDynamic(
            this,
            &UCMMainMenuWidget::HandleJoinCancelClicked
        );
    }
    RecoverStaleSessionIfNeeded();
    UpdateControls();
}

void UCMMainMenuWidget::NativeDestruct()
{
    if (ActiveOptionWidget.IsValid())
    {
        ActiveOptionWidget->OnDeactivated().RemoveAll(this);
        ActiveOptionWidget.Reset();
    }
    if (NetworkSubsystem)
    {
        NetworkSubsystem->OnStateChanged.RemoveDynamic(
            this,
            &UCMMainMenuWidget::HandleNetworkStateChanged
        );
        NetworkSubsystem->OnOperationCompleted.RemoveDynamic(
            this,
            &UCMMainMenuWidget::HandleOperationCompleted
        );
    }
    bWaitingForRoomSearch = false;
    PendingRoomId.Reset();

    Super::NativeDestruct();
}

void UCMMainMenuWidget::HandleJoinRoomClicked()
{
    if (!CanStartSessionOperation())
    {
        return;
    }

    if (Panel_JoinRoomPopup)
    {
        Panel_JoinRoomPopup->SetVisibility(ESlateVisibility::Visible);
    }

    if (Edt_RoomId)
    {
        Edt_RoomId->SetText(FText::GetEmpty());
        Edt_RoomId->SetKeyboardFocus();
    }
}

void UCMMainMenuWidget::HandleJoinCancelClicked()
{
    if (Panel_JoinRoomPopup)
    {
        Panel_JoinRoomPopup->SetVisibility(ESlateVisibility::Collapsed);
    }
}

bool UCMMainMenuWidget::CanStartSessionOperation() const
{
    return NetworkSubsystem
        && NetworkSubsystem->GetConnectionState()
            == EListenServerConnectionState::Offline
        && NetworkSubsystem->GetCurrentOperation()
            == EListenServerOperation::None;
}

void UCMMainMenuWidget::RecoverStaleSessionIfNeeded()
{
    UWorld* World = GetWorld();
    if (!NetworkSubsystem
        || !World
        || World->GetNetMode() != NM_Standalone
        || NetworkSubsystem->GetCurrentOperation()
            != EListenServerOperation::None
        || NetworkSubsystem->GetConnectionState()
            == EListenServerConnectionState::Offline)
    {
        return;
    }

    UE_LOG(
        LogChimeraMainMenuUI,
        Warning,
        TEXT("Cleaning up stale session state after returning to the standalone main menu.")
    );
    SetResultText(NSLOCTEXT(
        "ChimeraUI",
        "CleaningUpFailedConnection",
        "Cleaning up the failed connection..."
    ));
    NetworkSubsystem->LeaveSession();
}

void UCMMainMenuWidget::UpdateControls()
{
    const bool bCanStartSessionOperation = CanStartSessionOperation();
    const ESlateVisibility SessionButtonVisibility = bCanStartSessionOperation
        ? ESlateVisibility::Visible
        : ESlateVisibility::HitTestInvisible;

    if (Btn_CreateRoom)
    {
        Btn_CreateRoom->SetIsEnabled(true);
        Btn_CreateRoom->SetVisibility(SessionButtonVisibility);
    }
    if (Btn_JoinRoom)
    {
        Btn_JoinRoom->SetIsEnabled(true);
        Btn_JoinRoom->SetVisibility(SessionButtonVisibility);
    }
    if (Btn_QuickMatch)
    {
        Btn_QuickMatch->SetIsEnabled(true);
        Btn_QuickMatch->SetVisibility(SessionButtonVisibility);
    }
    if (Edt_RoomId)
    {
        Edt_RoomId->SetIsEnabled(bCanStartSessionOperation);
    }
}

void UCMMainMenuWidget::SetResultText(const FText& Message)
{
    if (Txt_Result)
    {
        Txt_Result->SetText(Message);
    }
}

void UCMMainMenuWidget::HandleNetworkStateChanged(
    EListenServerRole,
    EListenServerConnectionState,
    EListenServerOperation
)
{
    UpdateControls();
}

void UCMMainMenuWidget::HandleOperationCompleted(
    EListenServerOperation CompletedOperation,
    const FListenServerOperationResult& Result
)
{
    SetResultText(Result.UserMessage);

    if (CompletedOperation == EListenServerOperation::Searching
        && bWaitingForRoomSearch)
    {
        bWaitingForRoomSearch = false;
        FListenServerSearchResult SearchResult;
        if (Result.bSucceeded
            && NetworkSubsystem
            && NetworkSubsystem->GetSearchResultByIndex(0, SearchResult))
        {
            const FString RequestedRoomId = PendingRoomId;
            PendingRoomId.Reset();
            UE_LOG(
                LogChimeraMainMenuUI,
                Log,
                TEXT("Joining room code %s using search result %d."),
                *RequestedRoomId,
                SearchResult.Handle.ResultIndex
            );
            NetworkSubsystem->JoinSession(SearchResult.Handle);
        }
        else
        {
            PendingRoomId.Reset();
        }
    }

    UpdateControls();
}

void UCMMainMenuWidget::HandleOptionsClicked()
{
    if (bOptionRequestPending
        || (ActiveOptionWidget.IsValid()
            && ActiveOptionWidget->IsActivated()))
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UNKMUIManagerSubsystem* UIManager = GameInstance
        ? GameInstance->GetSubsystem<UNKMUIManagerSubsystem>()
        : nullptr;
    ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    if (!UIManager || !LocalPlayer)
    {
        OnOptionsRequested();
        return;
    }

    bOptionRequestPending = true;
    UIManager->InitializePolicyWithResult(
        LocalPlayer,
        FNKMUIPolicyInitializationCompleted::CreateUObject(
            this,
            &ThisClass::HandleUIPolicyInitialized));
}

void UCMMainMenuWidget::HandleUIPolicyInitialized(ENKMUIAsyncResult Result)
{
    if (Result != ENKMUIAsyncResult::Succeeded)
    {
        bOptionRequestPending = false;
        OnOptionsRequested();
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UNKMUIManagerSubsystem* UIManager =
            GameInstance->GetSubsystem<UNKMUIManagerSubsystem>())
        {
            FNKMUIWidgetPushCompleted OnPushed;
            OnPushed.BindDynamic(this, &ThisClass::HandleOptionPushed);
            UIManager->PushWidgetAsyncWithResult(
                UITags::UI_Layer_Modal,
                OptionWidgetClass,
                OnPushed);
            return;
        }
    }

    bOptionRequestPending = false;
    OnOptionsRequested();
}

void UCMMainMenuWidget::HandleOptionPushed(
    ENKMUIAsyncResult Result,
    UNKMUIActivatableWidget* Widget)
{
    bOptionRequestPending = false;
    ActiveOptionWidget = Result == ENKMUIAsyncResult::Succeeded
        ? Cast<UCMOptionWidget>(Widget)
        : nullptr;

    if (ActiveOptionWidget.IsValid())
    {
        VisibilityBeforeOptions = GetVisibility();
        SetVisibility(ESlateVisibility::Collapsed);
        ActiveOptionWidget->OnDeactivated().AddUObject(
            this, &ThisClass::HandleOptionClosed);
    }
}

void UCMMainMenuWidget::HandleOptionClosed()
{
    if (ActiveOptionWidget.IsValid())
    {
        ActiveOptionWidget->OnDeactivated().RemoveAll(this);
        ActiveOptionWidget.Reset();
    }
    SetVisibility(VisibilityBeforeOptions);
}

void UCMMainMenuWidget::HandleCreateRoomClicked()
{
    if (!CanStartSessionOperation())
    {
        return;
    }

    const FString RoomId = CMRoomId::Generate();
    FListenServerHostRequest Request = MakeDefaultHostRequest();
    FListenServerSessionAttribute& RoomIdAttribute =
        Request.ExtraAdvertisedAttributes.AddDefaulted_GetRef();
    RoomIdAttribute.Key = CMRoomId::AttributeKey();
    RoomIdAttribute.Value = RoomId;

    SetResultText(FText::Format(
        NSLOCTEXT(
            "ChimeraUI",
            "CreatingRoom",
            "Creating room {0}..."
        ),
        FText::FromString(RoomId)
    ));
    UE_LOG(
        LogChimeraMainMenuUI,
        Log,
        TEXT("Creating room code %s."),
        *RoomId
    );
    NetworkSubsystem->HostSession(Request);
}

void UCMMainMenuWidget::HandleJoinConfirmClicked()
{
    if (!NetworkSubsystem || !Edt_RoomId)
    {
        return;
    }

    FString RoomId;
    if (!CMRoomId::NormalizeAndValidate(
        Edt_RoomId->GetText().ToString(),
        RoomId
    ))
    {
        SetResultText(NSLOCTEXT(
            "ChimeraUI",
            "InvalidRoomId",
            "Enter a 6-character room ID containing letters and numbers."
        ));
        return;
    }

    Edt_RoomId->SetText(FText::FromString(RoomId));
    FListenServerSearchRequest Request = MakeDefaultSearchRequest();
    FListenServerSessionAttribute& RoomIdAttribute =
        Request.RequiredAttributes.AddDefaulted_GetRef();
    RoomIdAttribute.Key = CMRoomId::AttributeKey();
    RoomIdAttribute.Value = RoomId;

    PendingRoomId = RoomId;
    bWaitingForRoomSearch = true;
    SetResultText(FText::Format(
        NSLOCTEXT(
            "ChimeraUI",
            "FindingRoom",
            "Finding room {0}..."
        ),
        FText::FromString(RoomId)
    ));
    if (!NetworkSubsystem->FindSessions(Request))
    {
        bWaitingForRoomSearch = false;
        PendingRoomId.Reset();
    }
}

void UCMMainMenuWidget::HandleQuickMatchClicked()
{
    if (!CanStartSessionOperation())
    {
        return;
    }

    FListenServerQuickMatchRequest Request;
    Request.SearchRequest = MakeDefaultSearchRequest();
    Request.HostRequest = MakeDefaultHostRequest();
    Request.bAllowHostFallback = true;

    const FString FallbackRoomId = CMRoomId::Generate();
    FListenServerSessionAttribute& RoomIdAttribute =
        Request.HostRequest.ExtraAdvertisedAttributes
            .AddDefaulted_GetRef();
    RoomIdAttribute.Key = CMRoomId::AttributeKey();
    RoomIdAttribute.Value = FallbackRoomId;

    SetResultText(NSLOCTEXT(
        "ChimeraUI",
        "QuickMatchSearching",
        "Finding a room..."
    ));
    NetworkSubsystem->StartQuickMatch(Request);
}

void UCMMainMenuWidget::HandleQuitClicked()
{
    APlayerController* PlayerController = GetOwningPlayer();
    if (!PlayerController && GetWorld())
    {
        PlayerController = GetWorld()->GetFirstPlayerController();
    }

    UKismetSystemLibrary::QuitGame(
        this,
        PlayerController,
        EQuitPreference::Quit,
        false
    );
}
