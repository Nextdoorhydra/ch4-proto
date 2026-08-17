#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationRecordProvider.h"

struct FNKMTextBindingProfile;

struct FNKMTextBindingReconcileReport
{
	int32 RecordCount = 0;
	TArray<FString> AddedKeys;
	TArray<FString> UpdatedMetadataKeys;
	TArray<FString> OrphanKeys;
};

class NKMLOCALIZATIONEDITOR_API FNKMTextBindingReconciler
{
public:
	/** Adds missing generated entries in memory. Existing source strings are never replaced and orphans are never deleted. */
	static bool Reconcile(
		const FNKMTextBindingProfile& Profile,
		const INKMLocalizationRecordProvider& Provider,
		FNKMLocalizationDocument& InOutDocument,
		FNKMTextBindingReconcileReport& OutReport,
		FNKMLocalizationResult& OutResult);
};
