#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Stage/Device/CMPartLoadoutSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMPartLoadoutSaveGameSerializationTest,
    "Chimera.Device.PartLoadout.SaveGameSerialization",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMPartLoadoutSaveGameSerializationTest::RunTest(
    const FString& Parameters)
{
    UCMPartLoadoutSaveGame* Source =
        NewObject<UCMPartLoadoutSaveGame>();
    Source->StoredLoadouts.SetNum(
        UCMPartLoadoutStorageSubsystem::StorageSlotCount);

    FCMStoredPartLoadout& Slot = Source->StoredLoadouts[4];
    Slot.bOccupied = true;
    FCMStoredPartLoadoutRecord& Record =
        Slot.Parts.AddDefaulted_GetRef();
    Record.SlotAddress.SegmentIndex = 2;
    Record.SlotAddress.PartSlotIndex = 1;
    Record.PartClass = ACMPartActorBase::StaticClass();
    Record.PartRowName = TEXT("DefaultArm");
    Record.TierRowName = TEXT("Tier3");

    TArray<uint8> SaveData;
    TestTrue(TEXT("SaveGame serializes to memory"),
        UGameplayStatics::SaveGameToMemory(Source, SaveData));

    UCMPartLoadoutSaveGame* Restored = Cast<UCMPartLoadoutSaveGame>(
        UGameplayStatics::LoadGameFromMemory(SaveData));
    TestNotNull(TEXT("Serialized SaveGame restores"), Restored);
    if (!Restored || !Restored->StoredLoadouts.IsValidIndex(4)
        || Restored->StoredLoadouts[4].Parts.Num() != 1)
    {
        return false;
    }

    const FCMStoredPartLoadoutRecord& RestoredRecord =
        Restored->StoredLoadouts[4].Parts[0];
    TestTrue(TEXT("Persistent slot stays occupied"),
        Restored->StoredLoadouts[4].bOccupied);
    TestEqual(TEXT("Segment address persists"),
        RestoredRecord.SlotAddress.SegmentIndex, 2);
    TestEqual(TEXT("Part-slot address persists"),
        RestoredRecord.SlotAddress.PartSlotIndex, 1);
    TestEqual(TEXT("Part class persists"),
        RestoredRecord.PartClass.Get(), ACMPartActorBase::StaticClass());
    TestEqual(TEXT("Part row persists"),
        RestoredRecord.PartRowName, FName(TEXT("DefaultArm")));
    TestEqual(TEXT("Tier row persists"),
        RestoredRecord.TierRowName, FName(TEXT("Tier3")));
    return true;
}

#endif
