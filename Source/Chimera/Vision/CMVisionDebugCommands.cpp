#include "CoreMinimal.h"

#include "HAL/IConsoleManager.h"
#include "Parts/Head/CMHeadPartActor.h"
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

FAutoConsoleCommandWithWorldAndArgs AttachHeadCommand(
    TEXT("CM.AttachHead"),
    TEXT("Spawns BP_CMHead01HeadPart for a player control slot. Usage: CM.AttachHead <ControlSlot 0-3> [PlayerSlotId 0-3]"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AttachHead)
);
}

#endif
