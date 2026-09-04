#include "Player/CMPlayerController.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

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
            TEXT("[Cheat Usage] CM.AttachPart <SlotNumber> <DefaultArm|DefaultHead|SpringArm|LegTier1..5>"));
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

void SetupTestParts(const TArray<FString>& Args, UWorld* World)
{
    ACMPlayerController* Controller = FindLocalController(World);
    if (!Controller)
    {
        return;
    }

    struct FTestPartPlacement
    {
        int32 OneBasedSlotIndex;
        FName PartName;
    };

    const FTestPartPlacement Placements[] = {
        { 1, TEXT("LegTier3") },
        { 2, TEXT("LegTier3") },
        { 3, TEXT("DefaultArm") },
        { 4, TEXT("DefaultArm") },
        { 5, TEXT("LegTier3") },
        { 6, TEXT("LegTier3") },
        { 7, TEXT("DefaultHead") },
        { 8, TEXT("DefaultHead") },
        { 9, TEXT("SpringArm") },
        { 10, TEXT("SpringArm") },
        { 11, TEXT("LegTier3") },
        { 12, TEXT("LegTier3") },
        { 13, TEXT("DefaultHead") },
        { 14, TEXT("DefaultHead") },
        { 15, TEXT("LegTier3") },
        { 16, TEXT("LegTier3") }
    };

    for (const FTestPartPlacement& Placement : Placements)
    {
        Controller->RequestCheatAttachPart(
            Placement.OneBasedSlotIndex,
            Placement.PartName
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

#if !UE_BUILD_SHIPPING
FAutoConsoleCommandWithWorldAndArgs GoToCheckpointCommand(
    TEXT("CM.GoToCheckpoint"),
    TEXT("Loads and registers a checkpoint by one-based Rooms array order, including unreached rooms. Usage: CM.GoToCheckpoint 2"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>& Args, UWorld* World)
        {
            int64 CheckpointNumber = 0;
            if (Args.Num() != 1 || Args[0].IsEmpty() || Args[0].Len() > 10
                || Args[0].GetCharArray().ContainsByPredicate([](TCHAR C)
                    { return C != 0 && (C < TEXT('0') || C > TEXT('9')); })
                || (CheckpointNumber = FCString::Atoi64(*Args[0])) < 1
                || CheckpointNumber > MAX_int32)
            {
                UE_LOG(LogTemp, Warning, TEXT("Usage: CM.GoToCheckpoint <positive checkpoint number>, e.g. CM.GoToCheckpoint 2 (Rooms index 1)"));
                return;
            }
            if (ACMPlayerController* Controller = FindLocalController(World))
            {
                Controller->RequestCheatGoToCheckpoint(static_cast<int32>(CheckpointNumber));
            }
        }),
    ECVF_Cheat);

FAutoConsoleCommandWithWorldAndArgs GoToStageCommand(
    TEXT("CM.GoToStage"),
    TEXT("Travel to a one-based StageRoute entry. Usage: CM.GoToStage 2"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>& Args, UWorld* World)
        {
            int64 StageNumber = 0;
            if (Args.Num() != 1 || Args[0].IsEmpty() || Args[0].Len() > 10
                || Args[0].GetCharArray().ContainsByPredicate([](TCHAR C)
                    { return C != 0 && (C < TEXT('0') || C > TEXT('9')); })
                || (StageNumber = FCString::Atoi64(*Args[0])) < 1 || StageNumber > MAX_int32)
            {
                UE_LOG(LogTemp, Warning, TEXT("Usage: CM.GoToStage <positive stage number>, e.g. CM.GoToStage 2"));
                return;
            }
            if (ACMPlayerController* Controller = FindLocalController(World))
            {
                Controller->RequestCheatGoToStage(static_cast<int32>(StageNumber));
            }
        }),
    ECVF_Cheat);

FAutoConsoleCommandWithWorldAndArgs NextStageCommand(
    TEXT("CM.NextStage"),
    TEXT("Skips to the next StageRoute map for all players. Rejected during loading or on the last stage."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>& Args, UWorld* World)
        {
            if (ACMPlayerController* Controller = FindLocalController(World))
            {
                Controller->RequestCheatNextStage();
            }
        }),
    ECVF_Cheat);
#endif

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
    TEXT("Replaces a one-based slot Part. Usage: CM.AttachPart <SlotNumber> <DefaultArm|DefaultHead|SpringArm|LegTier1..5>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &AttachPart
    )
);

FAutoConsoleCommandWithWorldAndArgs SetupTestPartsCommand(
    TEXT("CM.Testpart"),
    TEXT("Replaces slots 1-16 with the fixed four-player test Part layout."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &SetupTestParts
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
