#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationTypes.h"

class NKMLOCALIZATIONEDITOR_API FNKMLocalizationValidator
{
public:
	static bool Validate(const FNKMLocalizationDocument& Document, FNKMLocalizationResult& OutResult);
};
