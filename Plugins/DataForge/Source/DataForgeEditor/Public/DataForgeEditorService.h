#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"

class UDataForgeRuleSet;

enum class EDataForgeBindingCompatibility : uint8
{
	Unknown,
	Direct,
	Convertible,
	Risky,
	Unsupported
};

struct DATAFORGEEDITOR_API FDataForgeBindingSuggestion
{
	EDataForgeBindingCompatibility Compatibility = EDataForgeBindingCompatibility::Unknown;
	FString SourceType;
	FString TargetType;
	FString Message;

	FString ToDisplayString() const;
};

class DATAFORGEEDITOR_API FDataForgeEditorService
{
public:
	static FDataForgeResult Probe(UDataForgeRuleSet& RuleSet, FDataForgeDataSet* OutDataSet = nullptr);
	static FDataForgeResult Preview(UDataForgeRuleSet& RuleSet, FDataForgeApplyPlan* OutPlan = nullptr);
	static FDataForgeResult PreviewDependencyGraph(UDataForgeRuleSet& RootRuleSet, FDataForgeApplyPlan* OutRootPlan = nullptr);
	static FDataForgeResult Apply(UDataForgeRuleSet& RuleSet);
	static FDataForgeResult ApplyDependencyGraph(UDataForgeRuleSet& RootRuleSet);
	static FDataForgeResult CleanupOrphans(UDataForgeRuleSet& RuleSet);
	static int32 AutoMapExactNames(UDataForgeRuleSet& RuleSet, const TArray<FName>& SourceColumns);
	static FDataForgeBindingSuggestion AnalyzeBinding(
		const UDataForgeRuleSet& RuleSet,
		const FDataForgeBindingRule& Binding,
		const FDataForgeDataSet& DataSet);
	static FDataForgeBindingSuggestion GetCachedBindingSuggestion(
		const UDataForgeRuleSet& RuleSet,
		const FDataForgeBindingRule& Binding);
	static void InvalidateProbeCache(UDataForgeRuleSet& RuleSet);
	static void LogResult(const UDataForgeRuleSet& RuleSet, const FDataForgeResult& Result, bool bOpenMessageLog);
};
