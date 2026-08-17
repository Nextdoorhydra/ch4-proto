#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationTypes.h"

class UStringTable;

struct FNKMStringTableDiff
{
	FString TableId;
	FString AssetPath;
	int32 Added = 0;
	int32 Updated = 0;
	int32 Removed = 0;
	bool bNamespaceChanged = false;

	bool HasChanges() const;
};

class NKMLOCALIZATIONEDITOR_API FNKMStringTableSynchronizer
{
public:
	static FNKMStringTableDiff BuildDiff(const UStringTable* ExistingAsset, const FNKMLocalizationTable& DesiredTable);
	static void ApplyToAsset(UStringTable& Asset, const FNKMLocalizationTable& DesiredTable);
	static bool Preview(
		const FNKMLocalizationDocument& Document,
		TArray<FNKMStringTableDiff>& OutDiffs,
		FNKMLocalizationResult& OutResult);

	static bool Sync(
		const FNKMLocalizationDocument& Document,
		TArray<FNKMStringTableDiff>& OutDiffs,
		FNKMLocalizationResult& OutResult);
};
