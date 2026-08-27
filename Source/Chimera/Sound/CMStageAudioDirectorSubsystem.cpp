#include "Sound/CMStageAudioDirectorSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/Play/CMPlayGameState.h"
#include "GameMode/StageRoute/CMStageRouteDefinition.h"
#include "GameMode/StageRoute/CMStageRouteSubsystem.h"
#include "Sound/NKMSoundSubsystem.h"

void UCMStageAudioDirectorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    if (InWorld.GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    CachedPlayGameState = InWorld.GetGameState<ACMPlayGameState>();
    if (!CachedPlayGameState.IsValid())
    {
        return;
    }

    CachedPlayGameState->OnPlayStateChanged.AddUniqueDynamic(
        this,
        &ThisClass::HandlePlayStateChanged);
    HandlePlayStateChanged();
}

void UCMStageAudioDirectorSubsystem::Deinitialize()
{
    if (CachedPlayGameState.IsValid())
    {
        CachedPlayGameState->OnPlayStateChanged.RemoveAll(this);
    }
    CachedPlayGameState.Reset();
    Super::Deinitialize();
}

void UCMStageAudioDirectorSubsystem::HandlePlayStateChanged()
{
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    UNKMSoundSubsystem* SoundSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UNKMSoundSubsystem>()
        : nullptr;
    if (!SoundSubsystem || !CachedPlayGameState.IsValid())
    {
        return;
    }

    switch (CachedPlayGameState->GetPlayPhase())
    {
    case ECMPlayPhase::Starting:
    case ECMPlayPhase::Playing:
        if (const UCMStageRouteSubsystem* StageRoute =
                GameInstance->GetSubsystem<UCMStageRouteSubsystem>())
        {
            const UCMStageRouteDefinition* RouteDefinition =
                StageRoute->GetStageRouteDefinition();
            const FCMStageRouteEntry* Stage = RouteDefinition
                ? RouteDefinition->GetStage(CachedPlayGameState->GetCurrentStageIndex())
                : nullptr;
            if (Stage && Stage->StageBGMTag.IsValid())
            {
                SoundSubsystem->PlayBGM(Stage->StageBGMTag);
            }
        }
        break;

    case ECMPlayPhase::Completed:
    case ECMPlayPhase::Victory:
    case ECMPlayPhase::Failed:
    case ECMPlayPhase::Defeat:
    case ECMPlayPhase::Ending:
    case ECMPlayPhase::Loading:
        SoundSubsystem->StopBGM();
        break;

    case ECMPlayPhase::WaitingForPlayers:
    default:
        break;
    }
}
