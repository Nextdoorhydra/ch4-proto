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

void RespawnAtLatestCheckpoint(const TArray<FString>& Args, UWorld* World)
{
    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatRespawnAtCheckpoint();
    }
}

void KillSegment(UWorld* World, int32 SegmentIndex)
{
    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatKillSegment(SegmentIndex);
    }
}

void DamageBody(const TArray<FString>& Args, UWorld* World)
{
    if (Args.Num() < 2)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Cheat Usage] CM.DamageBody <BodyIndex> <Damage>"));
        return;
    }

    const float Damage = FCString::Atof(*Args[1]);
    if (Damage <= 0.0f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Cheat Usage] Damage must be greater than zero."));
        return;
    }

    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatDamageSegment(
            FCString::Atoi(*Args[0]),
            Damage);
    }
}

void DamagePart(const TArray<FString>& Args, UWorld* World)
{
    if (Args.Num() < 2)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Cheat Usage] CM.DamagePart <SlotNumber> <Damage>"));
        return;
    }

    const int32 OneBasedSlotIndex = FCString::Atoi(*Args[0]);
    const float Damage = FCString::Atof(*Args[1]);
    if (OneBasedSlotIndex < 1
        || OneBasedSlotIndex > CMControl::MaxPartSlots
        || Damage <= 0.0f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Cheat Usage] CM.DamagePart <SlotNumber> <Damage> (SlotNumber=1..%d)"),
            CMControl::MaxPartSlots);
        return;
    }

    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatDamagePart(OneBasedSlotIndex, Damage);
    }
}

void SpawnRandomParts(const TArray<FString>& Args, UWorld* World)
{
    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatSpawnRandomParts();
    }
}

void AttachPart(const TArray<FString>& Args, UWorld* World)
{
    if (Args.Num() < 2)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Cheat Usage] CM.AttachPart <SlotNumber> <DefaultArm|SpringArm|LegTier1..5>"));
        return;
    }

    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatAttachPart(
            FCString::Atoi(*Args[0]),
            FName(*Args[1])
        );
    }
}

void FillAllSlotsWithPart(UWorld* World, FName PartName)
{
    if (ACMPlayerController* Controller = FindLocalController(World))
    {
        Controller->RequestCheatFillAllSlotsWithPart(PartName);
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

FAutoConsoleCommandWithWorldAndArgs RespawnAtLatestCheckpointCommand(
    TEXT("CM.Checkpoint"),
    TEXT("Restores the shared Chimera at the latest active room checkpoint."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &RespawnAtLatestCheckpoint
    )
);

FAutoConsoleCommandWithWorldAndArgs DamageBodyCommand(
    TEXT("CM.DamageBody"),
    TEXT("Damages a zero-based Chimera body segment. Usage: CM.DamageBody <BodyIndex> <Damage>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DamageBody)
);

FAutoConsoleCommandWithWorldAndArgs DamagePartCommand(
    TEXT("CM.DamagePart"),
    TEXT("Damages the Part attached to a one-based global Chimera slot. Usage: CM.DamagePart <SlotNumber> <Damage>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DamagePart)
);

FAutoConsoleCommandWithWorldAndArgs SpawnRandomPartsCommand(
    TEXT("CM.RandomParts"),
    TEXT("Attaches random registered production Part Blueprints to empty active slots."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &SpawnRandomParts
    )
);

FAutoConsoleCommandWithWorldAndArgs AttachPartCommand(
    TEXT("CM.AttachPart"),
    TEXT("Replaces a one-based slot Part. Usage: CM.AttachPart <SlotNumber> <DefaultArm|SpringArm|LegTier1..5>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &AttachPart
    )
);

#define CM_REGISTER_FILL_ALL_LEG_TIER_COMMAND(Tier) \
    FAutoConsoleCommandWithWorldAndArgs FillAllLegTier##Tier##Command( \
        TEXT("CM.AllLegTier" #Tier), \
        TEXT("Replaces every active slot with a production Tier " #Tier " Leg Part."), \
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda( \
            [](const TArray<FString>& Args, UWorld* World) \
            { \
                FillAllSlotsWithPart(World, FName(TEXT("LegTier" #Tier))); \
            } \
        ) \
    )

CM_REGISTER_FILL_ALL_LEG_TIER_COMMAND(3);
CM_REGISTER_FILL_ALL_LEG_TIER_COMMAND(4);
CM_REGISTER_FILL_ALL_LEG_TIER_COMMAND(5);

#undef CM_REGISTER_FILL_ALL_LEG_TIER_COMMAND

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
CM_REGISTER_SEGMENT_DEATH_COMMAND(8);
CM_REGISTER_SEGMENT_DEATH_COMMAND(9);
CM_REGISTER_SEGMENT_DEATH_COMMAND(10);
CM_REGISTER_SEGMENT_DEATH_COMMAND(11);
CM_REGISTER_SEGMENT_DEATH_COMMAND(12);
CM_REGISTER_SEGMENT_DEATH_COMMAND(13);
CM_REGISTER_SEGMENT_DEATH_COMMAND(14);
CM_REGISTER_SEGMENT_DEATH_COMMAND(15);

#undef CM_REGISTER_SEGMENT_DEATH_COMMAND
}

#endif
