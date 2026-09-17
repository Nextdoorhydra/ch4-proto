#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"

class UDataForgeAssetLayoutProfile;
class UDataForgeRuleSet;

struct DATAFORGEEDITOR_API FDataForgeProfileValidationReport
{
	bool bSuccess = false;
	int32 ProfileCount = 0;
	int32 ProfiledRuleSetCount = 0;
	int32 OutdatedRuleSetCount = 0;
	TArray<FDataForgeDiagnostic> Diagnostics;

	FString MakeSummary() const;
};

class DATAFORGEEDITOR_API FDataForgeProfileValidation
{
public:
	static FDataForgeProfileValidationReport ValidateProject();
	static FDataForgeProfileValidationReport Validate(
		TConstArrayView<UDataForgeAssetLayoutProfile*> Profiles,
		TConstArrayView<const UDataForgeRuleSet*> RuleSets);
};
