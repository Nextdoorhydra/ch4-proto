#pragma once

#include "CommonGameViewportClient.h"
#include "UI/Subsystem/NKMUIManagerSubsystem.h"

#include "CMGameViewportClient.generated.h"

class ULocalPlayer;
class UNKMUIActivatableWidget;

UCLASS(Within=Engine, transient, config=Engine)
class UI_API UCMGameViewportClient : public UCommonGameViewportClient
{
    GENERATED_BODY()

public:
    UCMGameViewportClient();

    virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;

private:
    void OpenOptionsForLocalPlayer(ULocalPlayer* LocalPlayer);
    void HandlePolicyInitialized(ENKMUIAsyncResult Result);

    UFUNCTION()
    void HandleOptionPushed(
        ENKMUIAsyncResult Result,
        UNKMUIActivatableWidget* Widget);

    TSoftClassPtr<UNKMUIActivatableWidget> OptionWidgetClass;
    TWeakObjectPtr<UNKMUIActivatableWidget> ActiveOptionWidget;
    TWeakObjectPtr<ULocalPlayer> PendingLocalPlayer;
    bool bOptionRequestPending = false;
};
