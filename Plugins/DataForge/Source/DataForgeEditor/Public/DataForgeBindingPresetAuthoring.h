#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"

class UDataForgeBindingPreset;
class UDataForgeRuleSet;

struct DATAFORGEEDITOR_API FDataForgeBindingPresetMaterialization
{
	bool bSuccess = false;
	int32 ResolvedSlotCount = 0;
	int32 AmbiguousSlotCount = 0;
	FName ManagedRuleId = NAME_None;
	TArray<FDataForgeDiagnostic> Diagnostics;

	FString MakeSummary() const;
};

class DATAFORGEEDITOR_API FDataForgeBindingPresetAuthoring
{
public:
	static FDataForgeBindingPresetMaterialization Validate(const UDataForgeBindingPreset& Preset);
	static FDataForgeBindingPresetMaterialization Materialize(
		UDataForgeRuleSet& RuleSet,
		const UDataForgeBindingPreset& Preset,
		const FString& OutputFolder);
};
