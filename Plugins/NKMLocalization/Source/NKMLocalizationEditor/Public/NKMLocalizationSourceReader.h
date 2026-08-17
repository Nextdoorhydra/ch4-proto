#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationTypes.h"

class NKMLOCALIZATIONEDITOR_API FNKMLocalizationSourceReader
{
public:
	static bool LoadFile(
		const FString& Filename,
		const FNKMLocalizationSourceContext& Context,
		FNKMLocalizationDocument& OutDocument,
		FNKMLocalizationResult& OutResult);

	static bool ParseJson(
		const FString& Source,
		const FNKMLocalizationSourceContext& Context,
		FNKMLocalizationDocument& OutDocument,
		FNKMLocalizationResult& OutResult);

	static bool ParseCsv(
		const FString& Source,
		const FNKMLocalizationSourceContext& Context,
		FNKMLocalizationDocument& OutDocument,
		FNKMLocalizationResult& OutResult);
};
