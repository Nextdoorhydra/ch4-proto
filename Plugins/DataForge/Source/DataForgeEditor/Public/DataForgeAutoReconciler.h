#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"

class UDataForgeRuleSet;

struct DATAFORGEEDITOR_API FDataForgeReconcileBatchResult
{
	int32 ProcessedCount = 0;
	int32 AppliedCount = 0;
	TArray<FString> Failures;

	bool IsSuccess() const { return Failures.IsEmpty(); }
};

/**
 * Coalesces editor-side source-of-truth changes and materializes affected RuleSets.
 * Every mutation goes through Preview before Apply; failed previews never change outputs.
 */
class DATAFORGEEDITOR_API FDataForgeAutoReconciler
{
public:
	static FDataForgeAutoReconciler& Get();

	void Startup();
	void Shutdown();

	void Request(UDataForgeRuleSet& RuleSet, FString Reason, double DelaySeconds = 0.35);
	void Cancel(UDataForgeRuleSet& RuleSet);
	int32 RequestForSourceAsset(UObject& SourceAsset, FString Reason, double DelaySeconds = 0.0);
	FDataForgeReconcileBatchResult ReconcileSourceAssetNow(UObject& SourceAsset, FString Reason);
	FDataForgeReconcileBatchResult FlushPending(bool bForceAll = true);
	void RefreshFileWatches();

private:
	struct FPendingRequest
	{
		TWeakObjectPtr<UDataForgeRuleSet> RuleSet;
		FString Reason;
		double DueTime = 0.0;
	};

	struct FDirectoryWatch
	{
		FDelegateHandle Handle;
		TMap<FString, TSet<FSoftObjectPath>> RuleSetsByFile;
	};

	bool Tick(float DeltaSeconds);
	void HandleDirectoryChanged(const TArray<struct FFileChangeData>& Changes, FString Directory);
	void HandleObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event);
	void HandleObjectsReinstanced(const TMap<UObject*, UObject*>& Replacements);
	void HandleReloadComplete(enum class EReloadCompleteReason Reason);
	void HandleAssetAdded(const struct FAssetData& AssetData);
	void HandleAssetUpdated(const struct FAssetData& AssetData);
	void HandleAssetRemoved(const struct FAssetData& AssetData);
	void HandleAssetRenamed(const struct FAssetData& AssetData, const FString& OldObjectPath);
	void HandleAssetChange(const struct FAssetData& AssetData, const FString& Reason);
	int32 RequestAffectedByPackage(const FString& PackageName, const FString& Reason, double DelaySeconds = 0.35);
	void RequestAll(const FString& Reason);
	FDataForgeReconcileBatchResult Reconcile(TArray<FPendingRequest> Requests);
	void SuppressPlannedPackages(const UDataForgeRuleSet& RuleSet, const struct FDataForgeApplyPlan& Plan);
	bool IsPackageSuppressed(const FString& PackageName);

	TMap<FSoftObjectPath, FPendingRequest> PendingRequests;
	TMap<FString, FDirectoryWatch> DirectoryWatches;
	TMap<FString, double> SuppressedPackages;
	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle PropertyChangedHandle;
	FDelegateHandle ObjectsReinstancedHandle;
	FDelegateHandle ReloadCompleteHandle;
	FDelegateHandle AssetAddedHandle;
	FDelegateHandle AssetUpdatedHandle;
	FDelegateHandle AssetRemovedHandle;
	FDelegateHandle AssetRenamedHandle;
	bool bStarted = false;
	bool bApplying = false;
};
