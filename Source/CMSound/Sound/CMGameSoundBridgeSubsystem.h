#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "CMGameSoundBridgeSubsystem.generated.h"

class UNKMSoundSubsystem;
class UAudioComponent;
struct FAsyncLoadCompleteMessage;

DECLARE_MULTICAST_DELEGATE(FCMSoundCatalogsRebuilt);

UCLASS()
// AsyncPDALoader의 현재 캐시를 태그 기반 사운드 조회표로 변환
class CMSOUND_API UCMGameSoundBridgeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Rebuilds the runtime sound lookup from assets currently held by AsyncPDALoader. */
    void RebuildRegisteredSoundCatalogs();

    void RegisterPersistentSFX(UAudioComponent* AudioComponent, FGameplayTag SoundTag);
    void ApplyPersistentSFXVolumes();

    FCMSoundCatalogsRebuilt OnSoundCatalogsRebuilt;

private:
    void HandleLoadComplete(FGameplayTag Channel, const FAsyncLoadCompleteMessage& Message);

    FGameplayMessageListenerHandle LoadCompleteListenerHandle;

    UPROPERTY()
    TObjectPtr<UNKMSoundSubsystem> SoundSubsystem;

    TMap<FGameplayTag, float> PersistentSFXEntryVolumesByTag;
    TMap<TWeakObjectPtr<UAudioComponent>, float> PersistentSFXComponents;
};
