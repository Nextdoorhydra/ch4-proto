#pragma once

#include "CoreMinimal.h"

struct FNKMTranslationStatus
{
	int32 Total = 0;
	int32 Translated = 0;
	int32 Stale = 0;
	int32 Missing = 0;
	int32 Invalid = 0;
};

class NKMLOCALIZATIONEDITOR_API FNKMTranslationStatusAnalyzer
{
public:
	static bool AnalyzeFile(const FString& Filename, FNKMTranslationStatus& OutStatus, FString& OutError);
	static bool AnalyzeString(const FString& PortableObjectText, FNKMTranslationStatus& OutStatus, FString& OutError);
};
