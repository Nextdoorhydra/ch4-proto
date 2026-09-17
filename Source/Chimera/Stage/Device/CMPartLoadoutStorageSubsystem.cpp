#include "Stage/Device/CMPartLoadoutStorageSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"
#include "Stage/Device/CMPartLoadoutSaveGame.h"

namespace
{
constexpr TCHAR PersistentSaveSlotName[] = TEXT("CMPartLoadoutCheatSlots");
constexpr int32 PersistentSaveUserIndex = 0;

struct FExistingPartRecord
{
    FCMPartSlotAddress SlotAddress;
    TObjectPtr<ACMPartActorBase> Part;
};
}

void UCMPartLoadoutStorageSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    StoredLoadouts.SetNum(StorageSlotCount);
    PersistentLoadouts.SetNum(StorageSlotCount);
    LoadPersistentStorage();
}

bool UCMPartLoadoutStorageSubsystem::IsValidStorageSlot(
    int32 SlotIndex) const
{
    return StoredLoadouts.IsValidIndex(SlotIndex)
        && SlotIndex >= 0 && SlotIndex < StorageSlotCount;
}

bool UCMPartLoadoutStorageSubsystem::IsStorageSlotOccupied(
    int32 SlotIndex) const
{
    return IsValidStorageSlot(SlotIndex)
        && StoredLoadouts[SlotIndex].bOccupied;
}

int32 UCMPartLoadoutStorageSubsystem::GetStoredPartCount(
    int32 SlotIndex) const
{
    return IsStorageSlotOccupied(SlotIndex)
        ? StoredLoadouts[SlotIndex].Parts.Num()
        : 0;
}

const FCMStoredPartLoadout*
UCMPartLoadoutStorageSubsystem::GetStoredLoadout(int32 SlotIndex) const
{
    return IsStorageSlotOccupied(SlotIndex)
        ? &StoredLoadouts[SlotIndex]
        : nullptr;
}

void UCMPartLoadoutStorageSubsystem::StoreLoadout(
    int32 SlotIndex,
    FCMStoredPartLoadout&& Loadout)
{
    if (IsValidStorageSlot(SlotIndex))
    {
        StoredLoadouts[SlotIndex] = MoveTemp(Loadout);
    }
}

bool UCMPartLoadoutStorageSubsystem::SaveCurrentLoadout(
    ACMChimera* Chimera,
    int32 SlotIndex)
{
    if (!IsValidStorageSlot(SlotIndex))
    {
        return false;
    }

    FCMStoredPartLoadout NewLoadout;
    if (!CaptureCurrentLoadout(Chimera, NewLoadout))
    {
        return false;
    }

    StoredLoadouts[SlotIndex] = MoveTemp(NewLoadout);
    return true;
}

bool UCMPartLoadoutStorageSubsystem::LoadSavedLoadout(
    ACMChimera* Chimera,
    int32 SlotIndex)
{
    const FCMStoredPartLoadout* Loadout = GetStoredLoadout(SlotIndex);
    return Loadout && ApplyLoadout(Chimera, *Loadout);
}

bool UCMPartLoadoutStorageSubsystem::SavePersistentLoadout(
    ACMChimera* Chimera,
    int32 SlotIndex)
{
    if (!IsValidStorageSlot(SlotIndex))
    {
        return false;
    }

    FCMStoredPartLoadout NewLoadout;
    if (!CaptureCurrentLoadout(Chimera, NewLoadout))
    {
        return false;
    }

    UCMPartLoadoutSaveGame* SaveGame = Cast<UCMPartLoadoutSaveGame>(
        UGameplayStatics::CreateSaveGameObject(
            UCMPartLoadoutSaveGame::StaticClass()));
    if (!SaveGame)
    {
        return false;
    }

    TArray<FCMStoredPartLoadout> NewPersistentLoadouts =
        PersistentLoadouts;
    NewPersistentLoadouts.SetNum(StorageSlotCount);
    NewPersistentLoadouts[SlotIndex] = NewLoadout;
    SaveGame->StoredLoadouts = NewPersistentLoadouts;
    if (!UGameplayStatics::SaveGameToSlot(
        SaveGame, PersistentSaveSlotName, PersistentSaveUserIndex))
    {
        return false;
    }

    PersistentLoadouts = MoveTemp(NewPersistentLoadouts);
    StoredLoadouts[SlotIndex] = MoveTemp(NewLoadout);
    return true;
}

bool UCMPartLoadoutStorageSubsystem::LoadPersistentLoadout(
    ACMChimera* Chimera,
    int32 SlotIndex)
{
    if (!PersistentLoadouts.IsValidIndex(SlotIndex)
        || !PersistentLoadouts[SlotIndex].bOccupied)
    {
        return false;
    }

    if (!ApplyLoadout(Chimera, PersistentLoadouts[SlotIndex]))
    {
        return false;
    }

    StoredLoadouts[SlotIndex] = PersistentLoadouts[SlotIndex];
    return true;
}

bool UCMPartLoadoutStorageSubsystem::CaptureCurrentLoadout(
    const ACMChimera* Chimera,
    FCMStoredPartLoadout& OutLoadout) const
{
    if (!IsValid(Chimera) || !Chimera->HasAuthority())
    {
        return false;
    }

    OutLoadout = FCMStoredPartLoadout();
    OutLoadout.bOccupied = true;
    const int32 ActiveSlotCount = Chimera->GetActiveSegmentCount()
        * CMControl::PartSlotsPerSegment;
    for (int32 FlatIndex = 0; FlatIndex < ActiveSlotCount; ++FlatIndex)
    {
        const FCMPartSlotAddress Address =
            CMControl::FromFlatPartSlotIndex(FlatIndex);
        const UCMPartSlotComponent* PartSlot =
            Chimera->GetPartSlotComponent(Address);
        const ACMPartActorBase* Part = PartSlot
            ? Cast<ACMPartActorBase>(PartSlot->GetAttachedPart())
            : nullptr;
        if (!IsValid(Part))
        {
            continue;
        }

        FCMStoredPartLoadoutRecord& Record =
            OutLoadout.Parts.AddDefaulted_GetRef();
        Record.SlotAddress = Address;
        Record.PartClass = Part->GetClass();
        Record.PartRowName = Part->GetPartRowName();
        Record.TierRowName = Part->GetTierRowName();
    }
    return true;
}

bool UCMPartLoadoutStorageSubsystem::ApplyLoadout(
    ACMChimera* Chimera,
    const FCMStoredPartLoadout& Loadout) const
{
    if (!IsValid(Chimera) || !Chimera->HasAuthority()
        || !Loadout.bOccupied)
    {
        return false;
    }

    const int32 ActiveSlotCount = Chimera->GetActiveSegmentCount()
        * CMControl::PartSlotsPerSegment;
    for (const FCMStoredPartLoadoutRecord& Record : Loadout.Parts)
    {
        const int32 FlatIndex =
            CMControl::ToFlatPartSlotIndex(Record.SlotAddress);
        if (!Record.PartClass || FlatIndex < 0 || FlatIndex >= ActiveSlotCount
            || !Chimera->GetPartSlotComponent(Record.SlotAddress))
        {
            return false;
        }
    }

    TArray<TObjectPtr<ACMPartActorBase>> NewParts;
    NewParts.Reserve(Loadout.Parts.Num());
    for (const FCMStoredPartLoadoutRecord& Record : Loadout.Parts)
    {
        const UCMPartSlotComponent* PartSlot =
            Chimera->GetPartSlotComponent(Record.SlotAddress);
        ACMPartActorBase* NewPart = ACMPartActorBase::SpawnPartFromDataRows(
            Chimera,
            Record.PartClass,
            Record.PartRowName,
            Record.TierRowName,
            PartSlot->GetComponentTransform(),
            Chimera);
        if (!NewPart)
        {
            for (ACMPartActorBase* SpawnedPart : NewParts)
            {
                if (IsValid(SpawnedPart))
                {
                    SpawnedPart->Destroy();
                }
            }
            return false;
        }
        NewParts.Add(NewPart);
    }

    TArray<FExistingPartRecord> ExistingParts;
    for (int32 FlatIndex = 0; FlatIndex < ActiveSlotCount; ++FlatIndex)
    {
        const FCMPartSlotAddress Address =
            CMControl::FromFlatPartSlotIndex(FlatIndex);
        UCMPartSlotComponent* PartSlot =
            Chimera->GetPartSlotComponent(Address);
        if (ACMPartActorBase* Part = PartSlot
            ? Cast<ACMPartActorBase>(PartSlot->DetachPart())
            : nullptr)
        {
            ExistingParts.Add({Address, Part});
        }
    }

    int32 AttachedCount = 0;
    for (int32 Index = 0; Index < Loadout.Parts.Num(); ++Index)
    {
        UCMPartSlotComponent* PartSlot = Chimera->GetPartSlotComponent(
            Loadout.Parts[Index].SlotAddress);
        if (!PartSlot->AttachPart(NewParts[Index]))
        {
            break;
        }
        ++AttachedCount;
    }

    if (AttachedCount != NewParts.Num())
    {
        for (int32 Index = 0; Index < AttachedCount; ++Index)
        {
            if (UCMPartSlotComponent* PartSlot =
                Chimera->GetPartSlotComponent(
                    Loadout.Parts[Index].SlotAddress))
            {
                PartSlot->DetachPart();
            }
        }
        for (ACMPartActorBase* NewPart : NewParts)
        {
            if (IsValid(NewPart))
            {
                NewPart->Destroy();
            }
        }
        for (const FExistingPartRecord& Existing : ExistingParts)
        {
            if (UCMPartSlotComponent* PartSlot =
                Chimera->GetPartSlotComponent(Existing.SlotAddress))
            {
                PartSlot->AttachPart(Existing.Part);
            }
        }
        return false;
    }

    for (const FExistingPartRecord& Existing : ExistingParts)
    {
        if (IsValid(Existing.Part))
        {
            Existing.Part->Destroy();
        }
    }
    return true;
}

void UCMPartLoadoutStorageSubsystem::LoadPersistentStorage()
{
    UCMPartLoadoutSaveGame* SaveGame = Cast<UCMPartLoadoutSaveGame>(
        UGameplayStatics::LoadGameFromSlot(
            PersistentSaveSlotName, PersistentSaveUserIndex));
    if (!SaveGame)
    {
        return;
    }

    PersistentLoadouts = SaveGame->StoredLoadouts;
    PersistentLoadouts.SetNum(StorageSlotCount);
    for (int32 SlotIndex = 0; SlotIndex < StorageSlotCount; ++SlotIndex)
    {
        if (PersistentLoadouts[SlotIndex].bOccupied)
        {
            StoredLoadouts[SlotIndex] = PersistentLoadouts[SlotIndex];
        }
    }
}
