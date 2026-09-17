#include "CMGameViewportClient.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "ListenServerNetworkSettings.h"
#include "Misc/PackageName.h"
#include "UI/NKMUIActivatableWidget.h"
#include "UI/NKMUITagList.h"

UCMGameViewportClient::UCMGameViewportClient()
{
    EscapeMenuWidgetClass = TSoftClassPtr<UNKMUIActivatableWidget>(
        FSoftObjectPath(TEXT(
            "/Game/Chimera/UI/Menu/WBP_CMEscapeMenu.WBP_CMEscapeMenu_C")));
}

bool UCMGameViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
    if (Super::InputKey(EventArgs))
    {
        return true;
    }

    if (EventArgs.Key != EKeys::Escape || EventArgs.Event != IE_Pressed)
    {
        return false;
    }

    UWorld* CurrentWorld = GetWorld();
    if (!CurrentWorld)
    {
        return false;
    }

    const UListenServerNetworkSettings* NetworkSettings =
        GetDefault<UListenServerNetworkSettings>();
    const FString MainMenuPackage = NetworkSettings
        ? NetworkSettings->MainMenuMap.GetLongPackageName()
        : FString();
    const FString MainMenuName = FPackageName::GetShortName(MainMenuPackage);
    if (!MainMenuName.IsEmpty()
        && UGameplayStatics::GetCurrentLevelName(CurrentWorld, true)
            == MainMenuName)
    {
        return false;
    }

    UGameInstance* CurrentGameInstance = GetGameInstance();
    ULocalPlayer* LocalPlayer = CurrentGameInstance
        ? CurrentGameInstance->FindLocalPlayerFromDeviceId(EventArgs.InputDevice)
        : nullptr;
    if (!LocalPlayer && CurrentGameInstance)
    {
        LocalPlayer = CurrentGameInstance->FindLocalPlayerFromControllerId(
            EventArgs.ControllerId);
    }

    if (!LocalPlayer)
    {
        return false;
    }

    OpenEscapeMenuForLocalPlayer(LocalPlayer);
    return true;
}

void UCMGameViewportClient::OpenEscapeMenuForLocalPlayer(
    ULocalPlayer* LocalPlayer)
{
    if (ActiveEscapeMenuWidget.IsValid()
        && ActiveEscapeMenuWidget->IsActivated())
    {
        ActiveEscapeMenuWidget->DeactivateWidget();
        ActiveEscapeMenuWidget.Reset();
        return;
    }

    if (bEscapeMenuRequestPending || !LocalPlayer)
    {
        return;
    }

    UGameInstance* CurrentGameInstance = GetGameInstance();
    UNKMUIManagerSubsystem* UIManager = CurrentGameInstance
        ? CurrentGameInstance->GetSubsystem<UNKMUIManagerSubsystem>()
        : nullptr;
    if (!UIManager)
    {
        return;
    }

    bEscapeMenuRequestPending = true;
    PendingLocalPlayer = LocalPlayer;
    UIManager->InitializePolicyWithResult(
        LocalPlayer,
        FNKMUIPolicyInitializationCompleted::CreateUObject(
            this,
            &ThisClass::HandlePolicyInitialized));
}

void UCMGameViewportClient::HandlePolicyInitialized(ENKMUIAsyncResult Result)
{
    UGameInstance* CurrentGameInstance = GetGameInstance();
    UNKMUIManagerSubsystem* UIManager = CurrentGameInstance
        ? CurrentGameInstance->GetSubsystem<UNKMUIManagerSubsystem>()
        : nullptr;
    if (Result != ENKMUIAsyncResult::Succeeded
        || !UIManager
        || !PendingLocalPlayer.IsValid())
    {
        bEscapeMenuRequestPending = false;
        PendingLocalPlayer.Reset();
        return;
    }

    FNKMUIWidgetPushCompleted OnPushed;
    OnPushed.BindDynamic(this, &ThisClass::HandleEscapeMenuPushed);
    UIManager->PushWidgetAsyncWithResult(
        UITags::UI_Layer_Modal,
        EscapeMenuWidgetClass,
        OnPushed);
}

void UCMGameViewportClient::HandleEscapeMenuPushed(
    ENKMUIAsyncResult Result,
    UNKMUIActivatableWidget* Widget)
{
    bEscapeMenuRequestPending = false;
    PendingLocalPlayer.Reset();
    ActiveEscapeMenuWidget = Result == ENKMUIAsyncResult::Succeeded
        ? Widget
        : nullptr;
}
