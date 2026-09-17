#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationTypes.h"

class NKMLOCALIZATIONEDITOR_API FNKMLocalizationSourceWriter
{
public:
	/** Saves canonical structured JSON. Tables, entries, and metadata are sorted for stable diffs. */
	static bool SaveStructuredJson(
		const FString& Filename,
		const FNKMLocalizationDocument& Document,
		FNKMLocalizationResult& OutResult);
};
