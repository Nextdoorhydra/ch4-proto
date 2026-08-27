#include "HUD/CMControlHUDSubsystem.h"

#include "HUD/CMControlHUDWidget.h"
#include "Player/CMControlBody.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UI/NKMUITagList.h"
#include "UI/NKMUIActivatableWidget.h"
#include "UI/Subsystem/NKMUIManagerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraControlHUD, Log, All);

void UCMControlHUDSubsystem::Deinitialize()
{
    if (ControlHUDWidget)
    {
        UGameInstance* GameInstance = GetLocalPlayer()
            ? GetLocalPlayer()->GetGameInstance()
            : nullptr;
        if (UNKMUIManagerSubsystem* UIManager = GameInstance
            ? GameInstance->GetSubsystem<UNKMUIManagerSubsystem>()
            : nullptr)
        {
            UIManager->RemoveWidget(
                UITags::UI_Layer_HUD,
                ControlHUDWidget
            );
        }
        ControlHUDWidget = nullptr;
    }

    Super::Deinitialize();
}

void UCMControlHUDSubsystem::Tick(float DeltaTime)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = GetWorld();
    APlayerController* PlayerController = LocalPlayer && World
        ? LocalPlayer->GetPlayerController(World)
        : nullptr;
    ACMControlBody* ControlBody = PlayerController
        ? PlayerController->GetPawn<ACMControlBody>()
        : nullptr;
    if (!ControlBody || !ControlBody->GetSharedChimera())
    {
        return;
    }

    UGameInstance* GameInstance = LocalPlayer->GetGameInstance();
    UNKMUIManagerSubsystem* UIManager = GameInstance
        ? GameInstance->GetSubsystem<UNKMUIManagerSubsystem>()
        : nullptr;
    if (!UIManager || bPolicyInitializationPending)
    {
        return;
    }

    bPolicyInitializationPending = true;
    UIManager->InitializePolicyWithResult(
        LocalPlayer,
        FNKMUIPolicyInitializationCompleted::CreateUObject(
            this,
            &ThisClass::HandlePolicyInitialized
        )
    );
}

TStatId UCMControlHUDSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(
        UCMControlHUDSubsystem,
        STATGROUP_Tickables
    );
}

bool UCMControlHUDSubsystem::IsTickable() const
{
    return !IsTemplate() && !ControlHUDWidget;
}

UWorld* UCMControlHUDSubsystem::GetTickableGameObjectWorld() const
{
    return GetWorld();
}

void UCMControlHUDSubsystem::HandlePolicyInitialized(
    ENKMUIAsyncResult Result
)
{
    bPolicyInitializationPending = false;
    if (Result != ENKMUIAsyncResult::Succeeded || ControlHUDWidget)
    {
        return;
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = GetWorld();
    APlayerController* PlayerController = LocalPlayer && World
        ? LocalPlayer->GetPlayerController(World)
        : nullptr;
    ACMControlBody* ControlBody = PlayerController
        ? PlayerController->GetPawn<ACMControlBody>()
        : nullptr;
    if (!ControlBody || !ControlBody->GetSharedChimera())
    {
        return;
    }

    UGameInstance* GameInstance = LocalPlayer->GetGameInstance();
    UNKMUIManagerSubsystem* UIManager = GameInstance
        ? GameInstance->GetSubsystem<UNKMUIManagerSubsystem>()
        : nullptr;
    static const TSoftClassPtr<UNKMUIActivatableWidget> ControlHUDClass(
        FSoftObjectPath(
            TEXT("/Game/Chimera/UI/HUD/WBP_CMControlHUD.WBP_CMControlHUD_C")
        )
    );
    const TSubclassOf<UNKMUIActivatableWidget> LoadedHUDClass =
        ControlHUDClass.LoadSynchronous();
    ControlHUDWidget = UIManager && LoadedHUDClass
        ? Cast<UCMControlHUDWidget>(UIManager->PushWidget(
            UITags::UI_Layer_HUD,
            LoadedHUDClass
        ))
        : nullptr;
    if (!ControlHUDWidget)
    {
        UE_LOG(LogChimeraControlHUD, Error,
            TEXT("Control HUD push to NKM.UI.Layer.HUD failed."));
        return;
    }

    ControlHUDWidget->SetControlBody(ControlBody);
    UE_LOG(LogChimeraControlHUD, Log,
        TEXT("Control HUD pushed to NKM.UI.Layer.HUD. Controller=%s"),
        *GetNameSafe(PlayerController));
}
