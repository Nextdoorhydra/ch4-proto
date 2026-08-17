#pragma once

#include "CoreMinimal.h"
#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeTypes.h"

class UDataForgeAssetLayoutProfile;
class UDataForgeRuleSet;

enum class EDataForgeAssetLayoutStatus : uint8
{
	Manual,
	InSync,
	Outdated,
	Modified,
	MissingProfile
};

enum class EDataForgeAssetLayoutRuleStatus : uint8
{
	Profile,
	Override,
	Custom,
	Missing
};

struct DATAFORGEEDITOR_API FDataForgeAssetLayoutRuleAnalysis
{
	FName RuleId = NAME_None;
	EDataForgeAssetLayoutRuleStatus Status = EDataForgeAssetLayoutRuleStatus::Profile;
	int32 OverrideFieldCount = 0;
};

struct DATAFORGEEDITOR_API FDataForgeAssetLayoutAnalysis
{
	EDataForgeAssetLayoutStatus Status = EDataForgeAssetLayoutStatus::Manual;
	int32 OverrideFieldCount = 0;
	int32 CustomRuleCount = 0;
	int32 MissingProfileRuleCount = 0;
	FString CurrentProfileHash;
	TArray<FDataForgeAssetLayoutRuleAnalysis> Rules;

	FString MakeSummary() const;
};

enum class EDataForgeAssetLayoutRebaseChange : uint8
{
	Added,
	Updated,
	Removed,
	CustomPreserved
};

struct DATAFORGEEDITOR_API FDataForgeAssetLayoutRebaseRuleChange
{
	FGuid RuleTemplateId;
	FName PreviousRuleId = NAME_None;
	FName CandidateRuleId = NAME_None;
	EDataForgeAssetLayoutRebaseChange Change = EDataForgeAssetLayoutRebaseChange::Updated;
	int32 ChangedFieldCount = 0;
};

/** Immutable result of a Profile rebase calculation. Apply rejects it if the RuleSet changed afterwards. */
struct DATAFORGEEDITOR_API FDataForgeAssetLayoutRebaseCandidate
{
	bool bSuccess = false;
	FString SourceStateHash;
	TArray<FDataForgeAssetRule> AssetRules;
	TArray<FDataForgeGeneratedAssetOutputRule> GeneratedOutputs;
	TArray<FDataForgeBindingRule> Bindings;
	FDataForgeProfileOrigin ProfileOrigin;
	TArray<FDataForgeAssetLayoutRebaseRuleChange> Changes;
	TArray<FDataForgeDiagnostic> Diagnostics;

	FString MakeSummary() const;
};

/** Editor mutation boundary for applying a pure Profile materialization to a RuleSet. */
class DATAFORGEEDITOR_API FDataForgeAssetLayoutAuthoring
{
public:
	static FDataForgeAssetLayoutAnalysis Analyze(const UDataForgeRuleSet& RuleSet);

	/** Calculate the complete rebase without changing the RuleSet. */
	static FDataForgeAssetLayoutRebaseCandidate PreviewRebase(
		const UDataForgeRuleSet& RuleSet,
		UDataForgeAssetLayoutProfile& Profile,
		const TMap<FName, FString>& ParameterValues,
		const TArray<FName>& SourceColumns);

	/** Apply a previously calculated candidate only when its source RuleSet is unchanged. */
	static FDataForgeResult ApplyRebase(UDataForgeRuleSet& RuleSet, const FDataForgeAssetLayoutRebaseCandidate& Candidate);
	static bool IsRebaseCandidateCurrent(const UDataForgeRuleSet& RuleSet, const FDataForgeAssetLayoutRebaseCandidate& Candidate);

	/** First use preserves manual rules as Custom. Re-materialization preserves local field overrides. */
	static FDataForgeResult Materialize(
		UDataForgeRuleSet& RuleSet,
		UDataForgeAssetLayoutProfile& Profile,
		const TMap<FName, FString>& ParameterValues,
		const TArray<FName>& SourceColumns);

	/** Remove Profile provenance while retaining every concrete Asset Rule. */
	static void Detach(UDataForgeRuleSet& RuleSet);

	/** Refresh transient Profile status for all loaded project RuleSets. Never materializes or applies content. */
	static int32 RefreshDependentStatuses(const UDataForgeAssetLayoutProfile* ChangedProfile = nullptr);
};
