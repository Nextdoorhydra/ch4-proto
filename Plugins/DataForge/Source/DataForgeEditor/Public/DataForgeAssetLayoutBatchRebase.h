#pragma once

#include "CoreMinimal.h"
#include "DataForgeAssetLayoutAuthoring.h"

class UDataForgeAssetLayoutProfile;
class UDataForgeRuleSet;

struct DATAFORGEEDITOR_API FDataForgeAssetLayoutBatchEntry
{
	TWeakObjectPtr<UDataForgeRuleSet> RuleSet;
	FDataForgeAssetLayoutRebaseCandidate Candidate;
	FDataForgeApplyPlan ContentPlan;
};

/** Mutation-free, project-level rebase preview. */
struct DATAFORGEEDITOR_API FDataForgeAssetLayoutBatchPlan
{
	bool bSuccess = false;
	TWeakObjectPtr<UDataForgeAssetLayoutProfile> Profile;
	FString ProfileRevisionHash;
	TArray<FDataForgeAssetLayoutBatchEntry> Entries;
	TArray<FDataForgeDiagnostic> Diagnostics;

	FString MakeSummary() const;
};

class DATAFORGEEDITOR_API FDataForgeAssetLayoutBatchRebase
{
public:
	static TArray<UDataForgeRuleSet*> FindDependents(const UDataForgeAssetLayoutProfile& Profile);
	static FDataForgeAssetLayoutBatchPlan Preview(UDataForgeAssetLayoutProfile& Profile);
	static FDataForgeAssetLayoutBatchPlan Preview(
		UDataForgeAssetLayoutProfile& Profile,
		TConstArrayView<UDataForgeRuleSet*> RuleSets);
	static FDataForgeResult Apply(FDataForgeAssetLayoutBatchPlan& Plan);

	/** Public for deterministic automation coverage and commandlet reuse. */
	static void ValidateGlobalCollisions(FDataForgeAssetLayoutBatchPlan& Plan);
};
