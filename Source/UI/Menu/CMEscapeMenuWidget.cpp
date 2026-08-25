#include "CMEscapeMenuWidget.h"

#include "Components/Button.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "ListenServerNetworkSettings.h"
#include "ListenServerSessionSubsystem.h"
#include "Menu/CMConfirmationDialogWidget.h"
#include "Misc/PackageName.h"
#include "Option/CMOptionWidget.h"
#include "UI/NKMAsyncAction_ShowConfirmation.h"
#include "UI/NKMUITagList.h"
#include "UI/Subsystem/NKMUIManagerSubsystem.h"

UCMEscapeMenuWidget::UCMEscapeMenuWidget(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InputConfig = ENKMUIWidgetInputMode::GameAndMenu;
    GameMouseCaptureMode = EMouseCaptureMode::NoCapture;
    SetIsFocusable(true);

    OptionWidgetClass = TSoftClassPtr<UCMOptionWidget>(FSoftObjectPath(
        TEXT("/Game/Chimera/UI/Option/WBP_CMOption.WBP_CMOption_C")));
    ConfirmationDialogClass = TSoftClassPtr<UCMConfirmationDialogWidget>(
        FSoftObjectPath(TEXT(
            "/Game/Chimera/UI/Menu/WBP_CMConfirmationDialog.WBP_CMConfirmationDialog_C")));
}

void UCMEscapeMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_Resume->OnClicked.AddUniqueDynamic(
        this, &ThisClass::HandleResumeClicked);
    Btn_Options->OnClicked.AddUniqueDynamic(
        this, &ThisClass::HandleOptionsClicked);
    Btn_LeaveGame->OnClicked.AddUniqueDynamic(
        this, &ThisClass::HandleLeaveGameClicked);
}

void UCMEscapeMenuWidget::NativeDestruct()
{
    Btn_Resume->OnClicked.RemoveDynamic(
        this, &ThisClass::HandleResumeClicked);
    Btn_Options->OnClicked.RemoveDynamic(
        this, &ThisClass::HandleOptionsClicked);
    Btn_LeaveGame->OnClicked.RemoveDynamic(
        this, &ThisClass::HandleLeaveGameClicked);
    if (ActiveLeaveAction)
    {
        ActiveLeaveAction->OnResult.RemoveAll(this);
        ActiveLeaveAction = nullptr;
    }
    Super::NativeDestruct();
}

FReply UCMEscapeMenuWidget::NativeOnPreviewKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        DeactivateWidget();
        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

UWidget* UCMEscapeMenuWidget::NativeGetDesiredFocusTarget() const
{
    return Btn_Resume;
}

void UCMEscapeMenuWidget::HandleResumeClicked()
{
    DeactivateWidget();
}

void UCMEscapeMenuWidget::HandleOptionsClicked()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UNKMUIManagerSubsystem* UIManager =
            GameInstance->GetSubsystem<UNKMUIManagerSubsystem>())
        {
            UIManager->PushWidgetAsync(
                UITags::UI_Layer_Modal, OptionWidgetClass);
        }
    }
}

void UCMEscapeMenuWidget::HandleLeaveGameClicked()
{
    if (ActiveLeaveAction)
    {
        return;
    }

    UListenServerSessionSubsystem* Network = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UListenServerSessionSubsystem>()
        : nullptr;
    const UWorld* World = GetWorld();
    const bool bIsHost = (Network
            && Network->GetCurrentRole() == EListenServerRole::Host)
        || (World && World->GetNetMode() == NM_ListenServer);

    FNKMUIDialogDescriptor Descriptor;
    Descriptor.Header = NSLOCTEXT(
        "ChimeraEscapeMenu", "LeaveHeader", "게임 나가기");
    Descriptor.Body = bIsHost
        ? NSLOCTEXT("ChimeraEscapeMenu", "LeaveHostBody",
            "게임을 나가면 방이 종료되고 모든 플레이어의 연결이 끊깁니다. 계속하시겠습니까?")
        : NSLOCTEXT("ChimeraEscapeMenu", "LeaveClientBody",
            "게임에서 나가 메인 메뉴로 이동하시겠습니까?");
    Descriptor.ConfirmText = NSLOCTEXT(
        "ChimeraEscapeMenu", "LeaveConfirm", "게임 나가기");
    Descriptor.CancelText = NSLOCTEXT(
        "ChimeraEscapeMenu", "LeaveCancel", "취소");

    const TSubclassOf<UCMConfirmationDialogWidget> DialogClass =
        ConfirmationDialogClass.LoadSynchronous();
    if (!DialogClass)
    {
        return;
    }

    ActiveLeaveAction = UNKMAsyncAction_ShowConfirmation::ShowConfirmation(
        this, DialogClass, Descriptor);
    if (!ActiveLeaveAction)
    {
        return;
    }

    ActiveLeaveAction->OnResult.AddDynamic(
        this, &ThisClass::HandleLeaveConfirmation);
    ActiveLeaveAction->Activate();
}

void UCMEscapeMenuWidget::HandleLeaveConfirmation(
    ENKMUIDialogResult Result)
{
    if (ActiveLeaveAction)
    {
        ActiveLeaveAction->OnResult.RemoveAll(this);
        ActiveLeaveAction = nullptr;
    }

    if (Result == ENKMUIDialogResult::Confirmed)
    {
        LeaveGame();
    }
}

void UCMEscapeMenuWidget::LeaveGame()
{
    DeactivateWidget();

    UListenServerSessionSubsystem* Network = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UListenServerSessionSubsystem>()
        : nullptr;
    if (Network && Network->LeaveAndReturnToMenu())
    {
        return;
    }

    const UListenServerNetworkSettings* Settings =
        GetDefault<UListenServerNetworkSettings>();
    const FString MainMenuPackage = Settings
        ? Settings->MainMenuMap.GetLongPackageName()
        : FString();
    if (!MainMenuPackage.IsEmpty())
    {
        UGameplayStatics::OpenLevel(this, FName(*MainMenuPackage));
    }
}
