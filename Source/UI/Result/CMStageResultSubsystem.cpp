#include "Result/CMStageResultSubsystem.h"

#include "Result/CMStageResultWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/Play/CMPlayGameState.h"
#include "Player/CMPlayerController.h"
#include "UI/NKMUITagList.h"
#include "UI/Subsystem/NKMUIManagerSubsystem.h"

void UCMStageResultSubsystem::Deinitialize()
{
    RemoveResultWidget();
    Super::Deinitialize();
}

void UCMStageResultSubsystem::Tick(float DeltaTime)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = GetWorld();
    if (!LocalPlayer
        || !World
        || World->GetNetMode() == NM_DedicatedServer
        || !LocalPlayer->ViewportClient)
    {
        RemoveResultWidget();
        return;
    }

    ACMPlayGameState* PlayState = World->GetGameState<ACMPlayGameState>();
    if (!PlayState || PlayState->GetPlayPhase() != ECMPlayPhase::Completed)
    {
        RemoveResultWidget();
        return;
    }

    if (IsValid(ResultWidget))
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
            this, &ThisClass::HandlePolicyInitialized));
}

TStatId UCMStageResultSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(
        UCMStageResultSubsystem, STATGROUP_Tickables);
}

bool UCMStageResultSubsystem::IsTickable() const
{
    return !IsTemplate();
}

UWorld* UCMStageResultSubsystem::GetTickableGameObjectWorld() const
{
    return GetWorld();
}

void UCMStageResultSubsystem::HandlePolicyInitialized(
    ENKMUIAsyncResult Result)
{
    bPolicyInitializationPending = false;
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = GetWorld();
    ACMPlayGameState* PlayState = World
        ? World->GetGameState<ACMPlayGameState>()
        : nullptr;
    if (Result != ENKMUIAsyncResult::Succeeded
        || !LocalPlayer
        || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Completed)
    {
        return;
    }

    UGameInstance* GameInstance = LocalPlayer->GetGameInstance();
    UNKMUIManagerSubsystem* UIManager = GameInstance
        ? GameInstance->GetSubsystem<UNKMUIManagerSubsystem>()
        : nullptr;
    static const TSoftClassPtr<UCMStageResultWidget> ResultWidgetClass(
        FSoftObjectPath(TEXT(
            "/Game/Chimera/UI/Menu/WBP_CMStageResult.WBP_CMStageResult_C")));
    const TSubclassOf<UCMStageResultWidget> LoadedResultClass =
        ResultWidgetClass.LoadSynchronous();
    ResultWidget = UIManager && LoadedResultClass
        ? Cast<UCMStageResultWidget>(UIManager->PushWidget(
            UITags::UI_Layer_Modal,
            LoadedResultClass))
        : nullptr;
    if (!ResultWidget)
    {
        return;
    }

    const ACMPlayerController* PlayerController = LocalPlayer
        ? Cast<ACMPlayerController>(LocalPlayer->GetPlayerController(World))
        : nullptr;
    ResultWidget->SetResult(
        PlayState->GetCompletedStageTime(),
        PlayerController && PlayerController->CanControlStageResult(),
        PlayState->GetCurrentStageIndex() + 1
            < PlayState->GetTotalStageCount());
}

void UCMStageResultSubsystem::RemoveResultWidget()
{
    if (!IsValid(ResultWidget))
    {
        ResultWidget = nullptr;
        return;
    }

    UGameInstance* GameInstance = GetLocalPlayer()
        ? GetLocalPlayer()->GetGameInstance()
        : nullptr;
    if (UNKMUIManagerSubsystem* UIManager = GameInstance
        ? GameInstance->GetSubsystem<UNKMUIManagerSubsystem>()
        : nullptr)
    {
        UIManager->RemoveWidget(UITags::UI_Layer_Modal, ResultWidget);
    }
    ResultWidget = nullptr;
}
