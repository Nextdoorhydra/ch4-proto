#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"

class UDataForgeRuleSet;

enum class EDataForgeGraphSourceKind : uint8
{
	SourceColumn,
	GeneratedOutput
};

struct DATAFORGEEDITOR_API FDataForgeBindingGraphSource
{
	EDataForgeGraphSourceKind Kind = EDataForgeGraphSourceKind::SourceColumn;
	FName Name = NAME_None;
	FString DisplayName;
};

struct DATAFORGEEDITOR_API FDataForgeBindingGraphTarget
{
	EDataForgeBindingTarget Target = EDataForgeBindingTarget::DataTableRow;
	FName TargetOutput = NAME_None;
	FString PropertyPath;
	FString PropertyType;
};

class DATAFORGEEDITOR_API FDataForgeBindingGraphModel
{
public:
	static TArray<FDataForgeBindingGraphSource> BuildSources(
		const UDataForgeRuleSet& RuleSet,
		const FDataForgeDataSet& DataSet);

	static TArray<FDataForgeBindingGraphTarget> BuildTargets(const UDataForgeRuleSet& RuleSet);

	static bool Connect(
		UDataForgeRuleSet& RuleSet,
		const FDataForgeDataSet& DataSet,
		const FDataForgeBindingGraphSource& Source,
		const FDataForgeBindingGraphTarget& Target,
		FString& OutMessage);
};
