#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationRecordProvider.h"

struct FNKMTextBindingProfile;

struct FNKMTextBindingCoverageReport
{
	int32 RecordCount = 0;
	int32 ExpectedKeyCount = 0;
	TArray<FString> MissingKeys;
	TArray<FString> OrphanKeys;
	TArray<FString> InvalidMetadataKeys;
};

class NKMLOCALIZATIONEDITOR_API FNKMTextBindingCoverageValidator
{
public:
	static bool Validate(
		const FNKMTextBindingProfile& Profile,
		const INKMLocalizationRecordProvider& Provider,
		const FNKMLocalizationDocument& Document,
		FNKMTextBindingCoverageReport& OutReport,
		FNKMLocalizationResult& OutResult);

	/** Validates every configured profile that defines an authoring source and record provider. */
	static bool ValidateConfiguredProfiles(FNKMLocalizationResult& OutResult);
};
