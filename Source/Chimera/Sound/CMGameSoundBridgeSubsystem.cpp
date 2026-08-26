#include "Sound/CMGameSoundBridgeSubsystem.h"

#include "AsyncLoadCompleteMessage.h"
#include "AsyncPDALoader.h"
#include "AsyncPDALoaderTags.h"
#include "PrimaryDataAssetBase.h"
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
    for (const FPrimaryAssetId& AssetId : Settings->SoundDataAssetIds)
    {
        if (UObject* SoundCatalog = Loader->FindCachedAsset(AssetId))
        {
            SoundSubsystem->RegisterSoundCatalog(SoundCatalog);
        }
    }
}
