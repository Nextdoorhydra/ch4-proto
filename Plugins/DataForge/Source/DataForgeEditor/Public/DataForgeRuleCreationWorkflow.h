#pragma once

#include "CoreMinimal.h"
#include "DataForgeRuleSet.h"
#include "DataForgeTypes.h"
#include "UObject/StrongObjectPtr.h"

class UDataForgeAssetLayoutProfile;

enum class EDataForgeWizardStep : uint8
{
	Source,
	Probe,
	Schema,
	Output,
	AssetLayout,
	AssetRules,
	GeneratedOutputs,
	Bindings,
	Preview
};

/** Staged RuleSet authoring model. Only Finish copies the draft into the real asset. */
class DATAFORGEEDITOR_API FDataForgeRuleCreationWorkflow
{
public:
	explicit FDataForgeRuleCreationWorkflow(UDataForgeRuleSet& InTarget);

	UDataForgeRuleSet& GetDraft() const;
	EDataForgeWizardStep GetStep() const;
	const FDataForgeDataSet& GetProbedDataSet() const;
	const FDataForgeApplyPlan& GetPreviewPlan() const;
	const FString& GetLastMessage() const;
	UDataForgeAssetLayoutProfile* GetSelectedAssetLayoutProfile() const;
	const TMap<FName, FString>& GetAssetLayoutParameterValues() const;
	bool IsManualAssetLayout() const;

	FDataForgeResult Probe();
	void SelectAssetLayoutProfile(UDataForgeAssetLayoutProfile* Profile);
	void SetAssetLayoutParameter(FName Name, const FString& Value);
	FDataForgeResult MaterializeAssetLayout();
	int32 AutoMapExactNames();
	FDataForgeResult Preview();
	void NotifyDraftChanged(FName MemberPropertyName);

	bool CanAdvance(FString& OutReason) const;
	bool Next(FString& OutReason);
	void Back();
	bool Finish(FString& OutReason);

private:
	TWeakObjectPtr<UDataForgeRuleSet> Target;
	TStrongObjectPtr<UDataForgeRuleSet> Draft;
	EDataForgeWizardStep Step = EDataForgeWizardStep::Source;
	FDataForgeDataSet ProbedDataSet;
	FDataForgeApplyPlan PreviewPlan;
	FString LastMessage;
	bool bProbeSucceeded = false;
	bool bAssetLayoutReady = true;
	bool bManualAssetLayout = true;
	bool bPreviewSucceeded = false;
	TWeakObjectPtr<UDataForgeAssetLayoutProfile> SelectedAssetLayoutProfile;
	TMap<FName, FString> AssetLayoutParameterValues;
	int32 RemoveInvalidGeneratedOutputBindings();
};
