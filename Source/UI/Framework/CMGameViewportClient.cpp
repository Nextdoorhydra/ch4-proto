#include "CMGameViewportClient.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "UI/NKMUIActivatableWidget.h"
#include "UI/NKMUITagList.h"

UCMGameViewportClient::UCMGameViewportClient()
{
    OptionWidgetClass = TSoftClassPtr<UNKMUIActivatableWidget>(FSoftObjectPath(
        TEXT("/Game/Chimera/UI/Option/WBP_CMOption.WBP_CMOption_C")));
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

    OpenOptionsForLocalPlayer(LocalPlayer);
    return true;
}

void UCMGameViewportClient::OpenOptionsForLocalPlayer(ULocalPlayer* LocalPlayer)
{
    if (ActiveOptionWidget.IsValid()
        && ActiveOptionWidget->IsActivated())
    {
        ActiveOptionWidget->DeactivateWidget();
        ActiveOptionWidget.Reset();
        return;
    }

    if (bOptionRequestPending || !LocalPlayer)
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

    bOptionRequestPending = true;
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
        bOptionRequestPending = false;
        PendingLocalPlayer.Reset();
        return;
    }

    FNKMUIWidgetPushCompleted OnPushed;
    OnPushed.BindDynamic(this, &ThisClass::HandleOptionPushed);
    UIManager->PushWidgetAsyncWithResult(
        UITags::UI_Layer_Modal,
        OptionWidgetClass,
        OnPushed);
}

void UCMGameViewportClient::HandleOptionPushed(
    ENKMUIAsyncResult Result,
    UNKMUIActivatableWidget* Widget)
{
    bOptionRequestPending = false;
    PendingLocalPlayer.Reset();
    ActiveOptionWidget = Result == ENKMUIAsyncResult::Succeeded
        ? Widget
        : nullptr;
}
