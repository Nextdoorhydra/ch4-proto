#include "CoreMinimal.h"

#include "HAL/IConsoleManager.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Parts/Head/CMVisionComponent.h"
#include "Player/CMChimera.h"
#include "Player/CMControlBody.h"
#include "Player/CMControlTypes.h"
#include "Player/CMPartSlotComponent.h"
#include "Player/CMPlayerController.h"
#include "Player/CMPlayerState.h"

#if !UE_BUILD_SHIPPING

DEFINE_LOG_CATEGORY_STATIC(LogChimeraVisionDebug, Log, All);

namespace CMVisionDebugCommands
{
const TCHAR* HeadPartClassPath =
    TEXT("/Game/Chimera/Character/Part/Head/BP_CMHead01HeadPart.BP_CMHead01HeadPart_C");

ACMPlayerController* FindPlayerController(
    UWorld* World,
    int32 RequestedPlayerSlotId,
    int32& OutPlayerSlotId
)
{
    OutPlayerSlotId = INDEX_NONE;
    if (!World)
    {
        return nullptr;
    }

    TArray<ACMPlayerController*> PlayerControllers;
    for (FConstPlayerControllerIterator It =
            World->GetPlayerControllerIterator();
        It;
        ++It)
    {
        ACMPlayerController* PlayerController =
            Cast<ACMPlayerController>(It->Get());
        if (PlayerController)
        {
            PlayerControllers.Add(PlayerController);
        }
    }

    if (RequestedPlayerSlotId != INDEX_NONE)
    {
        for (ACMPlayerController* PlayerController : PlayerControllers)
        {
            const ACMPlayerState* PlayerState =
                PlayerController->GetPlayerState<ACMPlayerState>();
            if (PlayerState
                && PlayerState->GetPlayerSlotId() == RequestedPlayerSlotId)
            {
                OutPlayerSlotId = RequestedPlayerSlotId;
                return PlayerController;
            }
        }
        return nullptr;
    }

    for (int32 PlayerIndex = 0;
        PlayerIndex < PlayerControllers.Num();
        ++PlayerIndex)
    {
        if (PlayerControllers[PlayerIndex]->IsLocalController())
        {
            const ACMPlayerState* PlayerState =
                PlayerControllers[PlayerIndex]
                    ->GetPlayerState<ACMPlayerState>();
            OutPlayerSlotId = PlayerState
                ? PlayerState->GetPlayerSlotId()
                : INDEX_NONE;
            return PlayerControllers[PlayerIndex];
        }
    }

    return nullptr;
}

void AttachHead(const TArray<FString>& Args, UWorld* World)
{
    int32 ControlSlotIndex = INDEX_NONE;
    int32 RequestedPlayerSlotId = INDEX_NONE;
    if (Args.Num() < 1
        || Args.Num() > 2
        || !LexTryParseString(ControlSlotIndex, *Args[0])
        || ControlSlotIndex < 0
        || ControlSlotIndex >= CMControl::MaxKeysPerPlayer
        || (Args.Num() == 2
            && (!LexTryParseString(RequestedPlayerSlotId, *Args[1])
                || RequestedPlayerSlotId < 0
                || RequestedPlayerSlotId >= CMControl::MaxPlayers)))
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("Usage: CM.AttachHead <ControlSlotIndex 0-3> [PlayerSlotId 0-3]"));
        return;
    }

    int32 ResolvedPlayerSlotId = INDEX_NONE;
    ACMPlayerController* PlayerController =
        FindPlayerController(
            World,
            RequestedPlayerSlotId,
            ResolvedPlayerSlotId
        );
    if (!PlayerController)
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Attach Head Failed] PlayerSlotId %d was not found. Run on the listen-server host after players are assigned."),
            RequestedPlayerSlotId);
        return;
    }

    if (!PlayerController->HasAuthority())
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Attach Head Failed] Run the command in Standalone or on the listen-server host."));
        return;
    }

    ACMControlBody* ControlBody =
        PlayerController->GetPawn<ACMControlBody>();
    ACMChimera* SharedChimera = ControlBody
        ? ControlBody->GetSharedChimera()
        : nullptr;
    if (!ControlBody || !SharedChimera)
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Attach Head Failed] ControlBody or SharedChimera is not ready."));
        return;
    }

    const FCMPartSlotAddress SlotAddress =
        ControlBody->GetPartSlotAddressForControlSlot(ControlSlotIndex);
    UCMPartSlotComponent* PartSlot =
        SharedChimera->GetPartSlotComponent(SlotAddress);
    if (!PartSlot)
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Attach Head Failed] Control slot %d has no assigned physical slot."),
            ControlSlotIndex);
        return;
    }

    if (PartSlot->HasAttachedPart())
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Attach Head Failed] Control slot %d is already occupied by %s."),
            ControlSlotIndex,
            *GetNameSafe(PartSlot->GetAttachedPart()));
        return;
    }

    UClass* HeadPartClass = LoadClass<ACMHeadPartActor>(
        nullptr,
        HeadPartClassPath
    );
    if (!HeadPartClass)
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Attach Head Failed] Could not load %s."),
            HeadPartClassPath);
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = SharedChimera;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACMHeadPartActor* HeadPart = World->SpawnActor<ACMHeadPartActor>(
        HeadPartClass,
        PartSlot->GetComponentTransform(),
        SpawnParameters
    );
    if (!HeadPart)
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Attach Head Failed] Head actor spawn failed."));
        return;
    }

    if (!SharedChimera->AttachPartToSlot(SlotAddress, HeadPart))
    {
        HeadPart->Destroy();
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Attach Head Failed] Slot rejected the spawned Head."));
        return;
    }

    UE_LOG(LogChimeraVisionDebug, Warning,
        TEXT("[Attach Head] PlayerSlotId=%d Controller=%s ControlBody=%s ControlSlot=%d Slot=(%d,%d) Head=%s"),
        ResolvedPlayerSlotId,
        *GetNameSafe(PlayerController),
        *GetNameSafe(ControlBody),
        ControlSlotIndex,
        SlotAddress.SegmentIndex,
        SlotAddress.PartSlotIndex,
        *GetNameSafe(HeadPart));
}

void SetPlayerVisionDebugColor(
    const TArray<FString>& Args,
    UWorld* World
)
{
    int32 RequestedPlayerSlotId = INDEX_NONE;
    int32 Red = 0;
    int32 Green = 0;
    int32 Blue = 0;
    float Strength = 0.65f;
    const bool bClear = Args.Num() == 2
        && Args[1].Equals(TEXT("clear"), ESearchCase::IgnoreCase);
    if ((Args.Num() != 4 && Args.Num() != 5 && !bClear)
        || !LexTryParseString(RequestedPlayerSlotId, *Args[0])
        || RequestedPlayerSlotId < 0
        || RequestedPlayerSlotId >= CMControl::MaxPlayers
        || (!bClear && (!LexTryParseString(Red, *Args[1])
            || !LexTryParseString(Green, *Args[2])
            || !LexTryParseString(Blue, *Args[3])
            || Red < 0 || Red > 255
            || Green < 0 || Green > 255
            || Blue < 0 || Blue > 255
            || (Args.Num() == 5
                && (!LexTryParseString(Strength, *Args[4])
                    || Strength < 0.0f || Strength > 1.0f)))))
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("Usage: CM.Vision.DebugColor <PlayerSlotId 0-3> <R 0-255> <G 0-255> <B 0-255> [Strength 0-1], or CM.Vision.DebugColor <PlayerSlotId> clear"));
        return;
    }

    int32 ResolvedPlayerSlotId = INDEX_NONE;
    ACMPlayerController* PlayerController = FindPlayerController(
        World,
        RequestedPlayerSlotId,
        ResolvedPlayerSlotId
    );
    if (!PlayerController || !PlayerController->HasAuthority())
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Vision Debug Color Failed] Run the command on the listen-server host after PlayerSlotId %d is assigned."),
            RequestedPlayerSlotId);
        return;
    }

    ACMControlBody* ControlBody =
        PlayerController->GetPawn<ACMControlBody>();
    ACMChimera* SharedChimera = ControlBody
        ? ControlBody->GetSharedChimera()
        : nullptr;
    if (!ControlBody || !SharedChimera)
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Vision Debug Color Failed] ControlBody or SharedChimera is not ready."));
        return;
    }

    TArray<ACMHeadPartActor*> Heads;
    for (const FCMPartSlotAddress& SlotAddress
        : ControlBody->GetControlSlots())
    {
        const UCMPartSlotComponent* PartSlot =
            SharedChimera->GetPartSlotComponent(SlotAddress);
        if (ACMHeadPartActor* Head = PartSlot
            ? Cast<ACMHeadPartActor>(PartSlot->GetAttachedPart())
            : nullptr)
        {
            Heads.AddUnique(Head);
        }
    }

    if (Heads.IsEmpty())
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Vision Debug Color Failed] PlayerSlotId %d has no Head in an assigned control slot."),
            ResolvedPlayerSlotId);
        return;
    }

    const FLinearColor Color(
        Red / 255.0f,
        Green / 255.0f,
        Blue / 255.0f
    );
    for (ACMHeadPartActor* Head : Heads)
    {
        if (UCMVisionComponent* Vision = Head->GetVisionComponent())
        {
            Vision->SetVisionTint(Color, bClear ? 0.0f : Strength);
        }
    }

    UE_LOG(LogChimeraVisionDebug, Warning,
        TEXT("[Vision Debug Color] PlayerSlotId=%d Heads=%d Color=(%d,%d,%d) Strength=%.2f"),
        ResolvedPlayerSlotId,
        Heads.Num(),
        Red,
        Green,
        Blue,
        bClear ? 0.0f : Strength);
}

void SetVisionSystemDebugEnabled(
    const TArray<FString>& Args,
    UWorld* World,
    bool bEnabled
)
{
    const bool bAllPlayers = Args.Num() == 1
        && Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase);
    int32 RequestedPlayerSlotId = INDEX_NONE;
    if (Args.Num() != 1
        || (!bAllPlayers
            && (!LexTryParseString(RequestedPlayerSlotId, *Args[0])
                || RequestedPlayerSlotId < 0
                || RequestedPlayerSlotId >= CMControl::MaxPlayers)))
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("Usage: CM.Vision.Debug%s <PlayerSlotId 0-3|all>"),
            bEnabled ? TEXT("Enable") : TEXT("Disable"));
        return;
    }

    if (!World || World->GetNetMode() == NM_Client)
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Vision Debug Failed] Run this command in Standalone or on the listen-server host."));
        return;
    }

    int32 AffectedPlayers = 0;
    if (bAllPlayers)
    {
        for (FConstPlayerControllerIterator It =
                World->GetPlayerControllerIterator();
            It;
            ++It)
        {
            if (ACMPlayerController* PlayerController =
                Cast<ACMPlayerController>(It->Get()))
            {
                if (ACMPlayerState* PlayerState =
                    PlayerController->GetPlayerState<ACMPlayerState>())
                {
                    PlayerState->SetVisionSystemEnabled(bEnabled);
                    ++AffectedPlayers;
                }
            }
        }
    }
    else
    {
        int32 ResolvedPlayerSlotId = INDEX_NONE;
        ACMPlayerController* PlayerController = FindPlayerController(
            World,
            RequestedPlayerSlotId,
            ResolvedPlayerSlotId
        );
        if (PlayerController)
        {
            if (ACMPlayerState* PlayerState =
                PlayerController->GetPlayerState<ACMPlayerState>())
            {
                PlayerState->SetVisionSystemEnabled(bEnabled);
                AffectedPlayers = 1;
            }
        }
    }

    if (AffectedPlayers == 0)
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Vision Debug Failed] No matching player was found."));
        return;
    }

    UE_LOG(LogChimeraVisionDebug, Warning,
        TEXT("[Vision Debug] Requested vision system %s for %s (%d player(s))."),
        bEnabled ? TEXT("enabled") : TEXT("disabled"),
        bAllPlayers ? TEXT("all players") : *Args[0],
        AffectedPlayers);
}

void EnableVisionSystemDebug(
    const TArray<FString>& Args,
    UWorld* World
)
{
    SetVisionSystemDebugEnabled(Args, World, true);
}

void DisableVisionSystemDebug(
    const TArray<FString>& Args,
    UWorld* World
)
{
    SetVisionSystemDebugEnabled(Args, World, false);
}

FAutoConsoleCommandWithWorldAndArgs AttachHeadCommand(
    TEXT("CM.AttachHead"),
    TEXT("Spawns BP_CMHead01HeadPart for a player control slot. Usage: CM.AttachHead <ControlSlot 0-3> [PlayerSlotId 0-3]"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AttachHead)
);

FAutoConsoleCommandWithWorldAndArgs VisionDebugColorCommand(
    TEXT("CM.Vision.DebugColor"),
    TEXT("Colors a player's replicated Head vision. Usage: CM.Vision.DebugColor <PlayerSlotId 0-3> <R 0-255> <G 0-255> <B 0-255> [Strength 0-1], or <PlayerSlotId> clear"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &SetPlayerVisionDebugColor
    )
);

FAutoConsoleCommandWithWorldAndArgs VisionDebugEnableCommand(
    TEXT("CM.Vision.DebugEnable"),
    TEXT("Enables Vision for one or all players. Usage: CM.Vision.DebugEnable <PlayerSlotId 0-3|all>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &EnableVisionSystemDebug
    )
);

FAutoConsoleCommandWithWorldAndArgs VisionDebugDisableCommand(
    TEXT("CM.Vision.DebugDisable"),
    TEXT("Disables Vision for one or all players. Usage: CM.Vision.DebugDisable <PlayerSlotId 0-3|all>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &DisableVisionSystemDebug
    )
);
}

#endif
