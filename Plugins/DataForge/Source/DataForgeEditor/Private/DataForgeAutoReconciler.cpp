#include "DataForgeAutoReconciler.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeEditorService.h"
#include "DataForgeRuleSet.h"
#include "DirectoryWatcherModule.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "IDirectoryWatcher.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr double DefaultSuppressionSeconds = 1.0;

	FString NormalizeSourceFilename(const FString& ConfiguredFilename)
	{
		if (ConfiguredFilename.IsEmpty()) return FString();

		FString Filename = ConfiguredFilename;
		FPaths::NormalizeFilename(Filename);
		if (FPaths::IsRelative(Filename))
		{
			const FString ProjectDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
			const FString UnrealResolved = FPaths::ConvertRelativePathToFull(Filename);
			Filename = FPaths::IsUnderDirectory(UnrealResolved, ProjectDirectory)
				? UnrealResolved
				: FPaths::ConvertRelativePathToFull(ProjectDirectory, Filename);
		}
		else
		{
			Filename = FPaths::ConvertRelativePathToFull(Filename);
		}
		FPaths::NormalizeFilename(Filename);
		return Filename;
	}

	void GetRuleSetAssets(TArray<FAssetData>& OutAssets)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.GetAssetsByClass(UDataForgeRuleSet::StaticClass()->GetClassPathName(), OutAssets, true);
		OutAssets.Sort([](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.GetSoftObjectPath().ToString() < Right.GetSoftObjectPath().ToString();
		});
	}

	bool ReferencesSourceAsset(const UDataForgeRuleSet& RuleSet, const FSoftObjectPath& SourcePath)
	{
		if (RuleSet.Source.SourceAsset.ToSoftObjectPath() == SourcePath) return true;
		return RuleSet.Source.Inputs.ContainsByPredicate([&SourcePath](const FDataForgeSourceInput& Input)
		{
			return Input.SourceAsset.ToSoftObjectPath() == SourcePath;
		});
	}

	bool PackageMayAffectRuleSet(const UDataForgeRuleSet& RuleSet, const FString& PackageName)
	{
		if (RuleSet.Output.AssetPath == PackageName || RuleSet.GetOutermost()->GetName() == PackageName) return true;
		for (const FDataForgeAssetRule& Rule : RuleSet.AssetRules)
		{
			if (!Rule.BaseFolder.IsEmpty() && (PackageName == Rule.BaseFolder || PackageName.StartsWith(Rule.BaseFolder + TEXT("/"))))
			{
				return true;
			}
		}
		return false;
	}
}

FDataForgeAutoReconciler& FDataForgeAutoReconciler::Get()
{
	static FDataForgeAutoReconciler Instance;
	return Instance;
}

void FDataForgeAutoReconciler::Startup()
{
	if (bStarted) return;
	bStarted = true;
	if (IsRunningCommandlet() || FApp::IsUnattended()) return;

	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FDataForgeAutoReconciler::HandleObjectPropertyChanged);
	ObjectsReinstancedHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddRaw(this, &FDataForgeAutoReconciler::HandleObjectsReinstanced);
	ReloadCompleteHandle = FCoreUObjectDelegates::ReloadCompleteDelegate.AddRaw(this, &FDataForgeAutoReconciler::HandleReloadComplete);

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetAddedHandle = Registry.OnAssetAdded().AddRaw(this, &FDataForgeAutoReconciler::HandleAssetAdded);
	AssetUpdatedHandle = Registry.OnAssetUpdated().AddRaw(this, &FDataForgeAutoReconciler::HandleAssetUpdated);
	AssetRemovedHandle = Registry.OnAssetRemoved().AddRaw(this, &FDataForgeAutoReconciler::HandleAssetRemoved);
	AssetRenamedHandle = Registry.OnAssetRenamed().AddRaw(this, &FDataForgeAutoReconciler::HandleAssetRenamed);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FDataForgeAutoReconciler::Tick));
	RefreshFileWatches();
}

void FDataForgeAutoReconciler::Shutdown()
{
	if (!bStarted) return;
	bStarted = false;

	if (TickerHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
	FCoreUObjectDelegates::OnObjectsReinstanced.Remove(ObjectsReinstancedHandle);
	FCoreUObjectDelegates::ReloadCompleteDelegate.Remove(ReloadCompleteHandle);

	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		IAssetRegistry& Registry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.OnAssetAdded().Remove(AssetAddedHandle);
		Registry.OnAssetUpdated().Remove(AssetUpdatedHandle);
		Registry.OnAssetRemoved().Remove(AssetRemovedHandle);
		Registry.OnAssetRenamed().Remove(AssetRenamedHandle);
	}
	if (FModuleManager::Get().IsModuleLoaded(TEXT("DirectoryWatcher")))
	{
		IDirectoryWatcher* Watcher = FModuleManager::GetModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")).Get();
		if (Watcher)
		{
			for (const TPair<FString, FDirectoryWatch>& Pair : DirectoryWatches)
			{
				Watcher->UnregisterDirectoryChangedCallback_Handle(Pair.Key, Pair.Value.Handle);
			}
		}
	}
	PendingRequests.Reset();
	DirectoryWatches.Reset();
	SuppressedPackages.Reset();
}

void FDataForgeAutoReconciler::Request(UDataForgeRuleSet& RuleSet, FString Reason, double DelaySeconds)
{
	if (bApplying) return;
	const FSoftObjectPath Path(&RuleSet);
	FPendingRequest& Pending = PendingRequests.FindOrAdd(Path);
	Pending.RuleSet = &RuleSet;
	Pending.Reason = MoveTemp(Reason);
	Pending.DueTime = FPlatformTime::Seconds() + FMath::Max(0.0, DelaySeconds);
}

int32 FDataForgeAutoReconciler::RequestForSourceAsset(UObject& SourceAsset, FString Reason, double DelaySeconds)
{
	const FSoftObjectPath SourcePath(&SourceAsset);
	TArray<FAssetData> Assets;
	GetRuleSetAssets(Assets);
	int32 Count = 0;
	for (const FAssetData& Asset : Assets)
	{
		if (UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Asset.GetAsset()); RuleSet && ReferencesSourceAsset(*RuleSet, SourcePath))
		{
			Request(*RuleSet, Reason, DelaySeconds);
			++Count;
		}
	}
	return Count;
}

FDataForgeReconcileBatchResult FDataForgeAutoReconciler::ReconcileSourceAssetNow(UObject& SourceAsset, FString Reason)
{
	const FSoftObjectPath SourcePath(&SourceAsset);
	TArray<FAssetData> Assets;
	GetRuleSetAssets(Assets);
	TArray<FPendingRequest> Requests;
	for (const FAssetData& Asset : Assets)
	{
		UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Asset.GetAsset());
		if (!RuleSet || !ReferencesSourceAsset(*RuleSet, SourcePath)) continue;

		const FSoftObjectPath RulePath(RuleSet);
		PendingRequests.Remove(RulePath);
		FPendingRequest& RequestItem = Requests.AddDefaulted_GetRef();
		RequestItem.RuleSet = RuleSet;
		RequestItem.Reason = Reason;
	}
	return Reconcile(MoveTemp(Requests));
}

FDataForgeReconcileBatchResult FDataForgeAutoReconciler::FlushPending(bool bForceAll)
{
	const double Now = FPlatformTime::Seconds();
	TArray<FPendingRequest> Due;
	for (auto It = PendingRequests.CreateIterator(); It; ++It)
	{
		if (bForceAll || It.Value().DueTime <= Now)
		{
			Due.Add(MoveTemp(It.Value()));
			It.RemoveCurrent();
		}
	}
	return Reconcile(MoveTemp(Due));
}

bool FDataForgeAutoReconciler::Tick(float)
{
	FlushPending(false);
	return true;
}

FDataForgeReconcileBatchResult FDataForgeAutoReconciler::Reconcile(TArray<FPendingRequest> Requests)
{
	FDataForgeReconcileBatchResult Batch;
	if (bApplying) return Batch;
	TGuardValue<bool> Guard(bApplying, true);
	for (const FPendingRequest& RequestItem : Requests)
	{
		UDataForgeRuleSet* RuleSet = RequestItem.RuleSet.Get();
		if (!RuleSet) continue;
		++Batch.ProcessedCount;

		FDataForgeApplyPlan Plan;
		const bool bGraph = !RuleSet->Dependencies.IsEmpty();
		const FDataForgeResult Preview = bGraph
			? FDataForgeEditorService::PreviewDependencyGraph(*RuleSet, &Plan)
			: FDataForgeEditorService::Preview(*RuleSet, &Plan);
		if (!Preview.bSuccess)
		{
			Batch.Failures.Add(FString::Printf(TEXT("%s [%s] Preview: %s"), *RuleSet->GetPathName(), *RequestItem.Reason, *Preview.Summary));
			FDataForgeEditorService::LogResult(*RuleSet, Preview, false);
			continue;
		}

		SuppressPlannedPackages(*RuleSet, Plan);
		const FDataForgeResult Apply = bGraph
			? FDataForgeEditorService::ApplyDependencyGraph(*RuleSet)
			: FDataForgeEditorService::Apply(*RuleSet);
		if (!Apply.bSuccess)
		{
			Batch.Failures.Add(FString::Printf(TEXT("%s [%s] Apply: %s"), *RuleSet->GetPathName(), *RequestItem.Reason, *Apply.Summary));
			FDataForgeEditorService::LogResult(*RuleSet, Apply, false);
			continue;
		}
		++Batch.AppliedCount;
	}
	return Batch;
}

void FDataForgeAutoReconciler::SuppressPlannedPackages(const UDataForgeRuleSet& RuleSet, const FDataForgeApplyPlan& Plan)
{
	const double Until = FPlatformTime::Seconds() + DefaultSuppressionSeconds;
	if (!RuleSet.Output.AssetPath.IsEmpty()) SuppressedPackages.Add(RuleSet.Output.AssetPath, Until);
	for (const FDataForgePlannedAsset& Asset : Plan.ManagedAssets)
	{
		if (!Asset.PackageName.IsEmpty()) SuppressedPackages.Add(Asset.PackageName, Until);
		if (!Asset.PreviousPackageName.IsEmpty()) SuppressedPackages.Add(Asset.PreviousPackageName, Until);
	}
}

bool FDataForgeAutoReconciler::IsPackageSuppressed(const FString& PackageName)
{
	const double Now = FPlatformTime::Seconds();
	for (auto It = SuppressedPackages.CreateIterator(); It; ++It)
	{
		if (It.Value() <= Now) It.RemoveCurrent();
	}
	if (const double* Until = SuppressedPackages.Find(PackageName)) return *Until > Now;
	return false;
}

void FDataForgeAutoReconciler::RefreshFileWatches()
{
	if (!bStarted || IsRunningCommandlet() || FApp::IsUnattended()) return;
	IDirectoryWatcher* Watcher = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")).Get();
	if (!Watcher) return;
	for (const TPair<FString, FDirectoryWatch>& Pair : DirectoryWatches)
	{
		Watcher->UnregisterDirectoryChangedCallback_Handle(Pair.Key, Pair.Value.Handle);
	}
	DirectoryWatches.Reset();

	TArray<FAssetData> Assets;
	GetRuleSetAssets(Assets);
	for (const FAssetData& Asset : Assets)
	{
		UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Asset.GetAsset());
		if (!RuleSet) continue;
		TArray<FString> Files;
		if (!RuleSet->Source.File.FilePath.IsEmpty()) Files.Add(RuleSet->Source.File.FilePath);
		for (const FDataForgeSourceInput& Input : RuleSet->Source.Inputs)
		{
			if (!Input.File.FilePath.IsEmpty()) Files.Add(Input.File.FilePath);
		}
		for (const FString& ConfiguredFile : Files)
		{
			const FString Filename = NormalizeSourceFilename(ConfiguredFile);
			if (Filename.IsEmpty()) continue;
			FDirectoryWatch& DirectoryWatch = DirectoryWatches.FindOrAdd(FPaths::GetPath(Filename));
			DirectoryWatch.RuleSetsByFile.FindOrAdd(Filename).Add(FSoftObjectPath(RuleSet));
		}
	}

	for (TPair<FString, FDirectoryWatch>& Pair : DirectoryWatches)
	{
		Watcher->RegisterDirectoryChangedCallback_Handle(Pair.Key,
			IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FDataForgeAutoReconciler::HandleDirectoryChanged, Pair.Key), Pair.Value.Handle);
	}
}

void FDataForgeAutoReconciler::HandleDirectoryChanged(const TArray<FFileChangeData>& Changes, FString Directory)
{
	const FDirectoryWatch* Watch = DirectoryWatches.Find(Directory);
	if (!Watch || bApplying) return;
	for (const FFileChangeData& Change : Changes)
	{
		const FString Filename = NormalizeSourceFilename(Change.Filename);
		if (const TSet<FSoftObjectPath>* RulePaths = Watch->RuleSetsByFile.Find(Filename))
		{
			for (const FSoftObjectPath& RulePath : *RulePaths)
			{
				if (UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(RulePath.TryLoad()))
				{
					Request(*RuleSet, TEXT("source file changed"));
				}
			}
		}
	}
}

void FDataForgeAutoReconciler::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent&)
{
	if (!Object || bApplying) return;
	if (UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Object))
	{
		Request(*RuleSet, TEXT("RuleSet edited"));
		RefreshFileWatches();
		return;
	}
	RequestAffectedByPackage(Object->GetOutermost()->GetName(), TEXT("managed asset or schema edited"));
}

void FDataForgeAutoReconciler::HandleObjectsReinstanced(const TMap<UObject*, UObject*>&)
{
	RequestAll(TEXT("referenced type reinstanced"));
}

void FDataForgeAutoReconciler::HandleReloadComplete(EReloadCompleteReason)
{
	RequestAll(TEXT("code reload completed"));
}

void FDataForgeAutoReconciler::HandleAssetAdded(const FAssetData& AssetData)
{
	HandleAssetChange(AssetData, TEXT("asset added"));
}

void FDataForgeAutoReconciler::HandleAssetUpdated(const FAssetData& AssetData)
{
	HandleAssetChange(AssetData, TEXT("asset updated"));
}

void FDataForgeAutoReconciler::HandleAssetRemoved(const FAssetData& AssetData)
{
	HandleAssetChange(AssetData, TEXT("asset removed"));
}

void FDataForgeAutoReconciler::HandleAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	HandleAssetChange(AssetData, TEXT("asset renamed"));
	RequestAffectedByPackage(FPackageName::ObjectPathToPackageName(OldObjectPath), TEXT("asset renamed"));
}

void FDataForgeAutoReconciler::HandleAssetChange(const FAssetData& AssetData, const FString& Reason)
{
	if (bApplying || IsPackageSuppressed(AssetData.PackageName.ToString())) return;
	if (AssetData.AssetClassPath == UDataForgeRuleSet::StaticClass()->GetClassPathName()) RefreshFileWatches();
	RequestAffectedByPackage(AssetData.PackageName.ToString(), Reason);
}

void FDataForgeAutoReconciler::RequestAffectedByPackage(const FString& PackageName, const FString& Reason)
{
	if (PackageName.IsEmpty() || IsPackageSuppressed(PackageName)) return;
	TArray<FAssetData> Assets;
	GetRuleSetAssets(Assets);
	for (const FAssetData& Asset : Assets)
	{
		if (UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Asset.GetAsset()); RuleSet && PackageMayAffectRuleSet(*RuleSet, PackageName))
		{
			Request(*RuleSet, Reason);
		}
	}
}

void FDataForgeAutoReconciler::RequestAll(const FString& Reason)
{
	TArray<FAssetData> Assets;
	GetRuleSetAssets(Assets);
	for (const FAssetData& Asset : Assets)
	{
		if (UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Asset.GetAsset())) Request(*RuleSet, Reason);
	}
	RefreshFileWatches();
}
