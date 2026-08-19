#include "CoreMinimal.h"

#include "HAL/IConsoleManager.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Player/CMChimera.h"
#include "Player/CMControlBody.h"
#include "Player/CMControlTypes.h"
#include "Player/CMPartSlotComponent.h"
#include "Player/CMPlayerController.h"

#if !UE_BUILD_SHIPPING

DEFINE_LOG_CATEGORY_STATIC(LogChimeraVisionDebug, Log, All);

namespace CMVisionDebugCommands
{
const TCHAR* HeadPartClassPath =
    TEXT("/Game/Chimera/Character/Part/Head/BP_CMHead01HeadPart.BP_CMHead01HeadPart_C");

ACMPlayerController* FindLocalPlayerController(UWorld* World)
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
        ACMPlayerController* PlayerController =
            Cast<ACMPlayerController>(It->Get());
        if (PlayerController && PlayerController->IsLocalController())
        {
            return PlayerController;
        }
    }

    return nullptr;
}

void AttachHead(const TArray<FString>& Args, UWorld* World)
{
    int32 ControlSlotIndex = INDEX_NONE;
    if (Args.Num() != 1
        || !LexTryParseString(ControlSlotIndex, *Args[0])
        || ControlSlotIndex < 0
        || ControlSlotIndex >= CMControl::MaxKeysPerPlayer)
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("Usage: CM.AttachHead <ControlSlotIndex 0-3>"));
        return;
    }

    ACMPlayerController* PlayerController =
        FindLocalPlayerController(World);
    if (!PlayerController)
    {
        UE_LOG(LogChimeraVisionDebug, Warning,
            TEXT("[Attach Head Failed] No local CMPlayerController."));
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
        TEXT("[Attach Head] ControlSlot=%d Slot=(%d,%d) Head=%s"),
        ControlSlotIndex,
        SlotAddress.SegmentIndex,
        SlotAddress.PartSlotIndex,
        *GetNameSafe(HeadPart));
}

FAutoConsoleCommandWithWorldAndArgs AttachHeadCommand(
    TEXT("CM.AttachHead"),
    TEXT("Spawns BP_CMHead01HeadPart and attaches it to one controlled slot. Usage: CM.AttachHead <0-3>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AttachHead)
);
}

#endif
