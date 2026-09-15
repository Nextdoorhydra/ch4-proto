#include "Loading/CMLoadingScreenSubsystem.h"

#include "Loading/CMLoadingScreenWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/CMGameState.h"
#include "GameMode/Play/CMPlayGameState.h"

#define LOCTEXT_NAMESPACE "CMLoadingScreenSubsystem"

void UCMLoadingScreenSubsystem::Deinitialize()
{
    RemoveLoadingScreen();
    Super::Deinitialize();
}

void UCMLoadingScreenSubsystem::Tick(float DeltaTime)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = GetWorld();
    if (!LocalPlayer
        || !World
        || World->GetNetMode() == NM_DedicatedServer
        || !LocalPlayer->ViewportClient)
    {
        RemoveLoadingScreen();
        return;
    }

    const ACMPlayGameState* PlayState =
        World->GetGameState<ACMPlayGameState>();
    const bool bStageLoading = PlayState
        && PlayState->GetPlayPhase() == ECMPlayPhase::Loading
        && PlayState->GetStageLoadRequest().IsValid();
    const ACMGameState* GameState = World->GetGameState<ACMGameState>();
    const bool bWorldPresentationLoading = GameState
        && GameState->GetWorldPresentationState()
            == ECMWorldPresentationState::Loading;
    if (!bStageLoading && !bWorldPresentationLoading)
    {
        RemoveLoadingScreen();
        return;
    }

    if (IsValid(LoadingScreenWidget)
        && LoadingScreenWorld.Get() == World)
    {
        return;
    }

    RemoveLoadingScreen();
    ShowLoadingScreen(World);
}

TStatId UCMLoadingScreenSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(
        UCMLoadingScreenSubsystem,
        STATGROUP_Tickables);
}

bool UCMLoadingScreenSubsystem::IsTickable() const
{
    return !IsTemplate();
}

UWorld* UCMLoadingScreenSubsystem::GetTickableGameObjectWorld() const
{
    return GetWorld();
}

void UCMLoadingScreenSubsystem::ShowLoadingScreen(UWorld* World)
{
    APlayerController* PlayerController = GetLocalPlayer()
        ? GetLocalPlayer()->GetPlayerController(World)
        : nullptr;
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        return;
    }

    static const TSoftClassPtr<UCMLoadingScreenWidget> LoadingScreenClass(
        FSoftObjectPath(TEXT(
            "/Game/Chimera/UI/Loading/WBP_CMLoadingScreen.WBP_CMLoadingScreen_C")));
    const TSubclassOf<UCMLoadingScreenWidget> LoadedClass =
        LoadingScreenClass.LoadSynchronous();
    if (!LoadedClass)
    {
        return;
    }

    LoadingScreenWidget = CreateWidget<UCMLoadingScreenWidget>(
        PlayerController,
        LoadedClass);
    if (!LoadingScreenWidget)
    {
        return;
    }

    LoadingScreenWidget->SetLoadingText(
        LOCTEXT("MapLoading", "맵 불러오는 중..."));
    LoadingScreenWidget->AddToViewport(1000);
    LoadingScreenWorld = World;
}

void UCMLoadingScreenSubsystem::RemoveLoadingScreen()
{
    if (IsValid(LoadingScreenWidget))
    {
        LoadingScreenWidget->RemoveFromParent();
    }
    LoadingScreenWidget = nullptr;
    LoadingScreenWorld.Reset();
}

#undef LOCTEXT_NAMESPACE
