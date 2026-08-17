#include "NKMLocalizationSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Internationalization/StringTable.h"
#include "Modules/ModuleManager.h"
#include "NKMLocalizationSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NKMLocalizationSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogNKMLocalizationRuntime, Log, All);

void UNKMLocalizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FARFilter Filter;
	Filter.PackagePaths.Add(*GetDefault<UNKMLocalizationSettings>()->GetNormalizedStringTableAssetRoot());
	Filter.ClassPaths.Add(UStringTable::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	FAssetRegistryModule::GetRegistry().GetAssets(Filter, Assets);
	RequestedTablePaths.Reserve(Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		RequestedTablePaths.AddUnique(Asset.GetSoftObjectPath());
	}

	if (RequestedTablePaths.IsEmpty())
	{
		CompleteLoad(0);
		return;
	}

	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		RequestedTablePaths,
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleTablesLoaded));
	if (!LoadHandle.IsValid())
	{
		CompleteLoad(RequestedTablePaths.Num());
	}
}

void UNKMLocalizationSubsystem::Deinitialize()
{
	if (LoadHandle.IsValid() && LoadHandle->IsActive())
	{
		LoadHandle->CancelHandle();
	}
	LoadHandle.Reset();
	RequestedTablePaths.Reset();
	LoadedTables.Reset();
	bLoadComplete = false;
	FailedTableCount = 0;
	Super::Deinitialize();
}

void UNKMLocalizationSubsystem::HandleTablesLoaded()
{
	int32 FailedCount = 0;
	for (const FSoftObjectPath& Path : RequestedTablePaths)
	{
		if (UStringTable* Table = Cast<UStringTable>(Path.ResolveObject()))
		{
			LoadedTables.Add(Table);
		}
		else
		{
			++FailedCount;
			UE_LOG(LogNKMLocalizationRuntime, Error, TEXT("Failed to preload String Table '%s'."), *Path.ToString());
		}
	}
	CompleteLoad(FailedCount);
}

void UNKMLocalizationSubsystem::CompleteLoad(const int32 InFailedTableCount)
{
	FailedTableCount = InFailedTableCount;
	bLoadComplete = true;
	OnStringTablesReady.Broadcast();
}
