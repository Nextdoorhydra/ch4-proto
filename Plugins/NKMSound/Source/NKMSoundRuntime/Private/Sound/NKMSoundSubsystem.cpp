#include "Sound/NKMSoundSubsystem.h"

#include "Engine/World.h"
#include "Sound/NKMSoundCatalogProvider.h"
#include "Sound/NKMSoundDataAsset.h"
#include "Sound/NKMSoundManager.h"
#include "Sound/NKMSoundSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogNKMSound, Log, All);

void UNKMSoundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SoundManager = NewObject<UNKMSoundManager>(this);
	ValidateSoundDataConfiguration();
	ReloadSoundData();
}

void UNKMSoundSubsystem::Deinitialize()
{
	SaveVolumeSettings();
	StopTimeDilationAudioTicker();
	StopGameplayAudioFadeTicker();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DuckingTimerHandle);
	}

	DuckingVolumeMultiplier = 1.0f;
	bGameplayAudioFadeActive = false;
	CurrentTimeDilationPitch = 1.0f;
	CurrentTimeDilationBGMVolume = 1.0f;
	CurrentLowPassCutoffFrequency = NormalLowPassCutoffFrequency;

	StopBGM(0.0f);
	StopCrowdLoop(0.0f);

	if (SoundManager)
	{
		SoundManager->ResetRuntimeState();
	}

	LoadedSoundCatalogs.Empty();
	SoundManager = nullptr;
	VolumeSaveRequested.Clear();

	Super::Deinitialize();
}

void UNKMSoundSubsystem::BeginSourceIsolation(FGameplayTag SoundTagRoot, AActor* AudibleSource)
{
	if (SoundManager)
	{
		SoundManager->BeginSourceIsolation(SoundTagRoot, AudibleSource);
	}
}

void UNKMSoundSubsystem::EndSourceIsolation()
{
	if (SoundManager)
	{
		SoundManager->EndSourceIsolation();
	}
}

void UNKMSoundSubsystem::ResetRuntimeMixState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DuckingTimerHandle);
	}

	DuckingTimerHandle.Invalidate();
	DuckingVolumeMultiplier = 1.0f;
	CrowdIntensity = 0.0f;
	ResetGameplayAudioFade();
}

void UNKMSoundSubsystem::ReloadSoundData()
{
	RebuildSoundManagerEntries();
}

bool UNKMSoundSubsystem::RegisterSoundDataAsset(UNKMSoundDataAsset* SoundDataAsset)
{
	return RegisterSoundCatalog(SoundDataAsset);
}

bool UNKMSoundSubsystem::RegisterSoundCatalog(UObject* SoundCatalog)
{
	if (!SoundCatalog || !Cast<INKMSoundCatalogProvider>(SoundCatalog) || LoadedSoundCatalogs.Contains(SoundCatalog))
	{
		return false;
	}

	LoadedSoundCatalogs.Add(SoundCatalog);
	ValidateSoundDataAssetRoutes(SoundCatalog);
	RebuildSoundManagerEntries();
	return true;
}

void UNKMSoundSubsystem::ClearSoundDataAssets()
{
	LoadedSoundCatalogs.Empty();
	RebuildSoundManagerEntries();
}

void UNKMSoundSubsystem::ValidateSoundDataConfiguration() const
{
	const UNKMSoundSettings* SoundSettings = GetDefault<UNKMSoundSettings>();
	if (!SoundSettings)
	{
		return;
	}

	TSet<FPrimaryAssetId> UniqueAssetIds;
	for (const FPrimaryAssetId& AssetId : SoundSettings->SoundDataAssetIds)
	{
		if (!AssetId.IsValid())
		{
			UE_LOG(LogNKMSound, Error, TEXT("SoundDataAssetIds contains an invalid asset id."));
			continue;
		}

		if (AssetId.PrimaryAssetType != FPrimaryAssetType(TEXT("NKMSoundDataAsset")))
		{
			UE_LOG(LogNKMSound, Error,
				TEXT("SoundDataAssetIds contains a non-sound catalog. AssetId=%s"),
				*AssetId.ToString());
		}

		if (UniqueAssetIds.Contains(AssetId))
		{
			UE_LOG(LogNKMSound, Error,
				TEXT("SoundDataAssetIds contains a duplicate. AssetId=%s"),
				*AssetId.ToString());
		}
		else
		{
			UniqueAssetIds.Add(AssetId);
		}
	}
}

void UNKMSoundSubsystem::ValidateSoundDataAssetRoutes(const UObject* SoundCatalog) const
{
	const INKMSoundCatalogProvider* Provider = Cast<INKMSoundCatalogProvider>(SoundCatalog);
	if (!Provider)
	{
		return;
	}

	const FPrimaryAssetId SourceAssetId = Provider->GetSoundCatalogId();
	for (const FNKMSoundEntry& Entry : Provider->GetSoundEntries())
	{
		if (!Entry.SoundTag.IsValid())
		{
			continue;
		}

		const FPrimaryAssetId RoutedAssetId = ResolveSoundDataAssetIdForTag(Entry.SoundTag);
		if (!RoutedAssetId.IsValid())
		{
			UE_LOG(LogNKMSound, Warning,
				TEXT("No catalog route is configured for SoundTag=%s Source=%s"),
				*Entry.SoundTag.ToString(),
				*SourceAssetId.ToString());
		}
		else if (RoutedAssetId != SourceAssetId)
		{
			UE_LOG(LogNKMSound, Error,
				TEXT("Sound tag route points to another catalog. SoundTag=%s Source=%s Route=%s"),
				*Entry.SoundTag.ToString(),
				*SourceAssetId.ToString(),
				*RoutedAssetId.ToString());
		}
	}
}

void UNKMSoundSubsystem::RebuildSoundManagerEntries()
{
	if (!SoundManager)
	{
		return;
	}

	TArray<UObject*> SoundCatalogs;
	SoundCatalogs.Reserve(LoadedSoundCatalogs.Num());
	for (UObject* SoundCatalog : LoadedSoundCatalogs)
	{
		if (SoundCatalog)
		{
			SoundCatalogs.Add(SoundCatalog);
		}
	}

	SoundManager->Initialize(SoundCatalogs);
}

const FNKMSoundEntry* UNKMSoundSubsystem::FindOrLoadEntry(FGameplayTag SoundTag)
{
	if (!SoundManager || !SoundTag.IsValid())
	{
		return nullptr;
	}

	if (const FNKMSoundEntry* Entry = SoundManager->FindEntry(SoundTag))
	{
		return Entry;
	}

	const FPrimaryAssetId ExpectedAssetId = ResolveSoundDataAssetIdForTag(SoundTag);
	UE_LOG(LogNKMSound, Error,
		TEXT("SoundTag is not registered. The host project must preload and register its catalog. SoundTag=%s ExpectedCatalog=%s"),
		*SoundTag.ToString(),
		*ExpectedAssetId.ToString());
	return nullptr;
}

FPrimaryAssetId UNKMSoundSubsystem::ResolveSoundDataAssetIdForTag(FGameplayTag SoundTag) const
{
	const UNKMSoundSettings* SoundSettings = GetDefault<UNKMSoundSettings>();
	if (!SoundSettings || !SoundTag.IsValid())
	{
		return FPrimaryAssetId();
	}

	FPrimaryAssetId BestAssetId;
	int32 BestRootLength = INDEX_NONE;
	for (const TPair<FGameplayTag, FPrimaryAssetId>& Pair : SoundSettings->SoundDataAssetIdByTagRoot)
	{
		if (!Pair.Key.IsValid() || !Pair.Value.IsValid() || !SoundTag.MatchesTag(Pair.Key))
		{
			continue;
		}

		const int32 RootLength = Pair.Key.ToString().Len();
		if (RootLength > BestRootLength)
		{
			BestRootLength = RootLength;
			BestAssetId = Pair.Value;
		}
	}

	return BestAssetId;
}
