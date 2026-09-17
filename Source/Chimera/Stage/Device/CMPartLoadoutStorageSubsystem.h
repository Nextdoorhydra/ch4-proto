#pragma once

#include "CoreMinimal.h"
#include "Player/CMControlTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "CMPartLoadoutStorageSubsystem.generated.h"

class ACMChimera;
class ACMPartActorBase;

USTRUCT()
struct FCMStoredPartLoadoutRecord
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FCMPartSlotAddress SlotAddress;

    UPROPERTY(SaveGame)
    TSubclassOf<ACMPartActorBase> PartClass;

    UPROPERTY(SaveGame)
    FName PartRowName = NAME_None;

    UPROPERTY(SaveGame)
    FName TierRowName = NAME_None;
};

USTRUCT()
struct FCMStoredPartLoadout
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    bool bOccupied = false;

    UPROPERTY(SaveGame)
    TArray<FCMStoredPartLoadoutRecord> Parts;
};

/** 맵 이동과 스트리밍 언로드를 넘어 서버 세션의 파츠 프리셋 10개를 유지한다. */
UCLASS()
class CHIMERA_API UCMPartLoadoutStorageSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    static constexpr int32 StorageSlotCount = 10;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    bool IsValidStorageSlot(int32 SlotIndex) const;
    bool IsStorageSlotOccupied(int32 SlotIndex) const;
    int32 GetStoredPartCount(int32 SlotIndex) const;
    const FCMStoredPartLoadout* GetStoredLoadout(int32 SlotIndex) const;
    void StoreLoadout(int32 SlotIndex, FCMStoredPartLoadout&& Loadout);

    bool SaveCurrentLoadout(ACMChimera* Chimera, int32 SlotIndex);
    bool LoadSavedLoadout(ACMChimera* Chimera, int32 SlotIndex);

    /** 같은 번호의 영구 슬롯은 새 현재 구성으로 덮어쓴다. */
    bool SavePersistentLoadout(ACMChimera* Chimera, int32 SlotIndex);
    bool LoadPersistentLoadout(ACMChimera* Chimera, int32 SlotIndex);

private:
    bool CaptureCurrentLoadout(
        const ACMChimera* Chimera,
        FCMStoredPartLoadout& OutLoadout) const;
    bool ApplyLoadout(
        ACMChimera* Chimera,
        const FCMStoredPartLoadout& Loadout) const;
    void LoadPersistentStorage();

    UPROPERTY(Transient)
    TArray<FCMStoredPartLoadout> StoredLoadouts;

    UPROPERTY(Transient)
    TArray<FCMStoredPartLoadout> PersistentLoadouts;
};
