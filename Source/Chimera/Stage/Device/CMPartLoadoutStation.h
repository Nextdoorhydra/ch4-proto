#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMPartLoadoutStation.generated.h"

class ACMChimera;
class UBoxComponent;
class UCMPartLoadoutStorageSubsystem;
class UCMPartLoadoutStationWidget;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

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
    int32 GetStorageSlotCount() const;

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
    UCMPartLoadoutStorageSubsystem* GetStorageSubsystem() const;

    UPROPERTY(Transient)
    TObjectPtr<UCMPartLoadoutStationWidget> StationWidget;

    TWeakObjectPtr<ACMChimera> NearbyChimera;
    TSet<TWeakObjectPtr<UPrimitiveComponent>> OverlappingChimeraComponents;
    bool bPreviousShowMouseCursor = false;
};
