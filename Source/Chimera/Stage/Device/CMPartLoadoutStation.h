#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/CMControlTypes.h"

#include "CMPartLoadoutStation.generated.h"

class ACMChimera;
class ACMPartActorBase;
class UBoxComponent;
class UCMPartLoadoutStationWidget;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

USTRUCT()
struct FCMStoredPartLoadoutRecord
{
    GENERATED_BODY()

    UPROPERTY()
    FCMPartSlotAddress SlotAddress;

    UPROPERTY()
    TSubclassOf<ACMPartActorBase> PartClass;

    UPROPERTY()
    FName PartRowName = NAME_None;

    UPROPERTY()
    FName TierRowName = NAME_None;
};

USTRUCT()
struct FCMStoredPartLoadout
{
    GENERATED_BODY()

    UPROPERTY()
    bool bOccupied = false;

    UPROPERTY()
    TArray<FCMStoredPartLoadoutRecord> Parts;
};

/** 서버/호스트가 근처에서 현재 파츠 구성을 10개 슬롯에 저장하고 복원하는 장치다. */
UCLASS(Blueprintable)
class CHIMERA_API ACMPartLoadoutStation : public AActor
{
    GENERATED_BODY()

public:
    ACMPartLoadoutStation();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part Loadout Station")
    bool SaveCurrentLoadout(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part Loadout Station")
    bool LoadSavedLoadout(int32 SlotIndex);

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Loadout Station")
    int32 GetStorageSlotCount() const { return StorageSlotCount; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Loadout Station")
    bool IsStorageSlotOccupied(int32 SlotIndex) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Loadout Station")
    int32 GetStoredPartCount(int32 SlotIndex) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Loadout Station")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Loadout Station")
    TObjectPtr<UStaticMeshComponent> StationMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Loadout Station")
    TObjectPtr<UBoxComponent> InteractionVolume;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Loadout Station|UI")
    TSubclassOf<UCMPartLoadoutStationWidget> StationWidgetClass;

private:
    static constexpr int32 StorageSlotCount = 10;

    UFUNCTION()
    void HandleInteractionBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleInteractionEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);

    void ShowStationUI();
    void HideStationUI();
    bool IsValidStorageSlot(int32 SlotIndex) const;

    UPROPERTY(Transient)
    TArray<FCMStoredPartLoadout> StoredLoadouts;

    UPROPERTY(Transient)
    TObjectPtr<UCMPartLoadoutStationWidget> StationWidget;

    TWeakObjectPtr<ACMChimera> NearbyChimera;
    TSet<TWeakObjectPtr<UPrimitiveComponent>> OverlappingChimeraComponents;
    bool bPreviousShowMouseCursor = false;
};
