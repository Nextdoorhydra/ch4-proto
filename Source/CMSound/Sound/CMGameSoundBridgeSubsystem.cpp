#include "Sound/CMGameSoundBridgeSubsystem.h"

#include "AsyncLoadCompleteMessage.h"
#include "AsyncPDALoader.h"
#include "AsyncPDALoaderTags.h"
#include "PrimaryDataAssetBase.h"
#include "Components/AudioComponent.h"
#include "Sound/NKMSoundCatalogProvider.h"
#include "Sound/NKMSoundSettings.h"
#include "Sound/NKMSoundSubsystem.h"

void UCMGameSoundBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Collection.InitializeDependency<UAsyncPDALoader>();
    Collection.InitializeDependency<UNKMSoundSubsystem>();
    SoundSubsystem = GetGameInstance()->GetSubsystem<UNKMSoundSubsystem>();

    LoadCompleteListenerHandle = UGameplayMessageSubsystem::Get(this)
        .RegisterListener<FAsyncLoadCompleteMessage>(
            AsyncPDALoaderTags::Message_Load_Complete,
            this,
            &ThisClass::HandleLoadComplete);

    RebuildRegisteredSoundCatalogs();
}

void UCMGameSoundBridgeSubsystem::Deinitialize()
{
    if (LoadCompleteListenerHandle.IsValid())
    {
        LoadCompleteListenerHandle.Unregister();
    }
    if (SoundSubsystem)
    {
        SoundSubsystem->ClearSoundDataAssets();
    }
    PersistentSFXEntryVolumesByTag.Empty();
    PersistentSFXComponents.Empty();
    SoundSubsystem = nullptr;
    Super::Deinitialize();
}

void UCMGameSoundBridgeSubsystem::HandleLoadComplete(
    FGameplayTag Channel,
    const FAsyncLoadCompleteMessage& Message)
{
    if (Message.LoadedAssetIds.IsEmpty())
    {
        return;
    }
    RebuildRegisteredSoundCatalogs();
}

void UCMGameSoundBridgeSubsystem::RebuildRegisteredSoundCatalogs()
{
    UAsyncPDALoader* Loader = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UAsyncPDALoader>()
        : nullptr;
    const UNKMSoundSettings* Settings = GetDefault<UNKMSoundSettings>();
    if (!Loader || !Settings || !SoundSubsystem)
    {
        return;
    }

    SoundSubsystem->ClearSoundDataAssets();
    PersistentSFXEntryVolumesByTag.Empty();
    bool bRegisteredAnyCatalog = false;
    for (const FPrimaryAssetId& AssetId : Settings->SoundDataAssetIds)
    {
        if (UObject* SoundCatalog = Loader->FindCachedAsset(AssetId))
        {
            bRegisteredAnyCatalog |= SoundSubsystem->RegisterSoundCatalog(SoundCatalog);
            if (const INKMSoundCatalogProvider* Provider = Cast<INKMSoundCatalogProvider>(SoundCatalog))
            {
                for (const FNKMSoundEntry& Entry : Provider->GetSoundEntries())
                {
                    if (Entry.SoundTag.IsValid()
                        && Entry.Layer != ENKMSoundLayer::BGM
                        && Entry.Layer != ENKMSoundLayer::Crowd)
                    {
                        PersistentSFXEntryVolumesByTag.FindOrAdd(Entry.SoundTag, Entry.Volume);
                    }
                }
            }
        }
    }

    if (bRegisteredAnyCatalog)
    {
        OnSoundCatalogsRebuilt.Broadcast();
    }
}

void UCMGameSoundBridgeSubsystem::RegisterPersistentSFX(
    UAudioComponent* AudioComponent,
    FGameplayTag SoundTag)
{
    if (!IsValid(AudioComponent))
    {
        return;
    }

    if (const float* EntryVolume = PersistentSFXEntryVolumesByTag.Find(SoundTag))
    {
        PersistentSFXComponents.Add(AudioComponent, *EntryVolume);
    }
}

void UCMGameSoundBridgeSubsystem::ApplyPersistentSFXVolumes()
{
    if (!SoundSubsystem)
    {
        return;
    }

    const float LayerVolume = SoundSubsystem->GetMasterVolume() * SoundSubsystem->GetSFXVolume();
    for (auto It = PersistentSFXComponents.CreateIterator(); It; ++It)
    {
        UAudioComponent* AudioComponent = It.Key().Get();
        if (!IsValid(AudioComponent) || !AudioComponent->IsPlaying())
        {
            It.RemoveCurrent();
            continue;
        }

        AudioComponent->SetVolumeMultiplier(It.Value() * LayerVolume);
    }
}
