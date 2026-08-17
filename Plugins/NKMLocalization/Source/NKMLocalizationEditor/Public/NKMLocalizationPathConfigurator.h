#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationTypes.h"

/** Propagates the Project Settings asset root to tracked cook/gather configuration. */
class NKMLOCALIZATIONEDITOR_API FNKMLocalizationPathConfigurator
{
public:
	static bool Apply(FNKMLocalizationResult& OutResult);
	static FString ToProjectContentPath(const FString& LongPackageRoot);
};
