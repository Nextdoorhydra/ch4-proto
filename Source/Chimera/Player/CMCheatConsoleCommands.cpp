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

void SpawnLegParts(const TArray<FString>& Args, UWorld* World)
{
    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatSpawnLegParts();
    }
}

void ClearLegParts(const TArray<FString>& Args, UWorld* World)
{
    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatClearLegParts();
    }
}

// CM.DebugMove.Enable 1 또는 0으로 로컬 화살표 이동을 전환
void SetDebugMovementEnabled(const TArray<FString>& Args, UWorld* World)
{
    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        const bool bEnabled = !Args.IsEmpty()
            && FCString::Atoi(*Args[0]) != 0;
        Controller->SetCheatDebugMovementEnabled(bEnabled);
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
    TEXT("Attaches random registered production Part Blueprints to empty active slots."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &SpawnRandomParts
    )
);

FAutoConsoleCommandWithWorldAndArgs ClearRandomPartsCommand(
    TEXT("CM.ClearRandomParts"),
    TEXT("Detaches and destroys only Parts created by CM.RandomParts."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &ClearRandomParts
    )
);

FAutoConsoleCommandWithWorldAndArgs SpawnLegPartsCommand(
    TEXT("CM.LegParts"),
    TEXT("Spawns and attaches production Leg Parts to every empty active slot."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &SpawnLegParts
    )
);

FAutoConsoleCommandWithWorldAndArgs ClearLegPartsCommand(
    TEXT("CM.ClearLegParts"),
    TEXT("Detaches and destroys only Leg Parts created by CM.LegParts."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &ClearLegParts
    )
);

FAutoConsoleCommandWithWorldAndArgs DebugMovementEnableCommand(
    TEXT("CM.DebugMove.Enable"),
    TEXT("Enables arrow-key Chimera movement. Usage: CM.DebugMove.Enable 1|0"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &SetDebugMovementEnabled)
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
