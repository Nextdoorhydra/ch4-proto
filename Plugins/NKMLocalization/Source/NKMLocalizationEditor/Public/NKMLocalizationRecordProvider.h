#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationTypes.h"

struct FNKMTextBindingProfile;

struct FNKMLocalizationRecord
{
	FName Id;
	FString SourceLocation;
};

/** Project-agnostic source of stable gameplay record IDs. */
class NKMLOCALIZATIONEDITOR_API INKMLocalizationRecordProvider
{
public:
	virtual ~INKMLocalizationRecordProvider() = default;
	virtual bool EnumerateRecords(TArray<FNKMLocalizationRecord>& OutRecords, FNKMLocalizationResult& OutResult) const = 0;
};

/** Creates the built-in CSV or reflected-map provider configured for a binding profile. */
class NKMLOCALIZATIONEDITOR_API FNKMLocalizationRecordProviderFactory
{
public:
	static TUniquePtr<INKMLocalizationRecordProvider> Create(
		const FNKMTextBindingProfile& Profile,
		const FString& CsvOverride,
		FNKMLocalizationResult& OutResult);
};
