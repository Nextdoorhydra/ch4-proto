#include "Player/CMPlayerController.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

namespace CMCheatConsoleCommands
{
ACMPlayerController* FindLocalController(UWorld* World)
{
    if (!World)
    {
        return nullptr;
    }

    for (FConstPlayerControllerIterator It =
            World->GetPlayerControllerIterator();
        It;
        ++It)
    {
        ACMPlayerController* Controller =
            Cast<ACMPlayerController>(It->Get());
        if (Controller && Controller->IsLocalController())
        {
            return Controller;
        }
    }

    return nullptr;
}

void KillAllSegments(const TArray<FString>& Args, UWorld* World)
{
    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatKillAllSegments();
    }
}

void KillSegment(UWorld* World, int32 SegmentIndex)
{
    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatKillSegment(SegmentIndex);
    }
}

void SpawnRandomParts(const TArray<FString>& Args, UWorld* World)
{
    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatSpawnRandomParts();
    }
}

void ClearRandomParts(const TArray<FString>& Args, UWorld* World)
{
    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatClearRandomParts();
    }
}

FAutoConsoleCommandWithWorldAndArgs KillAllSegmentsCommand(
    TEXT("CM.AllDead"),
    TEXT("Kills every living Chimera body segment on the server."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &KillAllSegments
    )
);

FAutoConsoleCommandWithWorldAndArgs SpawnRandomPartsCommand(
    TEXT("CM.RandomParts"),
    TEXT("Attaches random Head/Arm/Leg diagnostic Parts to empty active slots."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &SpawnRandomParts
    )
);

FAutoConsoleCommandWithWorldAndArgs ClearRandomPartsCommand(
    TEXT("CM.ClearRandomParts"),
    TEXT("Detaches and destroys diagnostic Parts only."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &ClearRandomParts
    )
);

#define CM_REGISTER_SEGMENT_DEATH_COMMAND(Index) \
    FAutoConsoleCommandWithWorldAndArgs KillBody##Index##Command( \
        TEXT("CM.Body" #Index "Dead"), \
        TEXT("Kills Chimera body segment " #Index " on the server."), \
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda( \
            [](const TArray<FString>& Args, UWorld* World) \
            { \
                KillSegment(World, Index); \
            } \
        ) \
    )

CM_REGISTER_SEGMENT_DEATH_COMMAND(0);
CM_REGISTER_SEGMENT_DEATH_COMMAND(1);
CM_REGISTER_SEGMENT_DEATH_COMMAND(2);
CM_REGISTER_SEGMENT_DEATH_COMMAND(3);
CM_REGISTER_SEGMENT_DEATH_COMMAND(4);
CM_REGISTER_SEGMENT_DEATH_COMMAND(5);
CM_REGISTER_SEGMENT_DEATH_COMMAND(6);
CM_REGISTER_SEGMENT_DEATH_COMMAND(7);

#undef CM_REGISTER_SEGMENT_DEATH_COMMAND
}

#endif
