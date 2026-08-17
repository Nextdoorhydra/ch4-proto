#pragma once

#include "AssetRegistry/AssetData.h"
#include "DataForgeTypes.h"

class UDataForgeRuleSet;

struct FDataForgeRenameCandidate
{
	FSoftObjectPath AssetPath;
	FSoftObjectPath RuleSetPath;
	TWeakObjectPtr<const UDataForgeRuleSet> RuleSet;
	FName SlotId = NAME_None;
	FName RecordId = NAME_None;
	int32 MatchScore = 0;
	bool bRecommended = false;
	TArray<FString> MatchEvidence;
	FString SuggestedPackageName;
	FString SuggestedAssetName;
	FString Reason;
	TArray<FDataForgeDiagnostic> Diagnostics;

	FString GetSuggestedObjectPath() const;
	bool HasErrors() const;
	bool IsChange() const;
};

class FDataForgeRenameAdvisor
{
public:
	/** Finds every deterministic proposal for an asset across persisted RuleSets. */
	static TArray<FDataForgeRenameCandidate> BuildCandidates(const FAssetData& AssetData);
	/** Aggregates candidates and marks destinations claimed by more than one source asset. */
	static TArray<FDataForgeRenameCandidate> BuildCandidates(const TArray<FAssetData>& Assets);

	/** Deterministic overload used by the UI and automation tests. */
	static TArray<FDataForgeRenameCandidate> BuildCandidatesForRuleSet(
		const FAssetData& AssetData,
		const UDataForgeRuleSet& RuleSet);

	/** Revalidates stale/collision conditions immediately before the explicit rename. */
	static FDataForgeResult ValidateForApply(const FDataForgeRenameCandidate& Candidate);
	static FDataForgeResult Apply(const FDataForgeRenameCandidate& Candidate);

	/** All-or-nothing preflight for the candidates selected in the audit window. */
	static FDataForgeResult ValidateBatchForApply(const TArray<FDataForgeRenameCandidate>& Candidates);
	static FDataForgeResult ApplyBatch(const TArray<FDataForgeRenameCandidate>& Candidates);

private:
	static TArray<FDataForgeRenameCandidate> BuildCandidatesForRuleSetWithData(
		const FAssetData& AssetData,
		const UDataForgeRuleSet& RuleSet,
		const FDataForgeDataSet& PrimaryData,
		const TSet<FString>* ManifestMatches);
};
