#pragma once

#include "CoreMinimal.h"
#include "DataForgeRuleSet.h"
#include "DataForgeTypes.h"
#include "UObject/StrongObjectPtr.h"

class UDataForgeAssetLayoutProfile;
class UDataForgeBindingPreset;

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
	UDataForgeBindingPreset* GetSelectedBindingPreset() const;
	const FString& GetBindingPresetOutputFolder() const;
	const TMap<FName, FString>& GetAssetLayoutParameterValues() const;
	bool IsManualAssetLayout() const;

	FDataForgeResult Probe();
	void SelectAssetLayoutProfile(UDataForgeAssetLayoutProfile* Profile);
	void SetAssetLayoutParameter(FName Name, const FString& Value);
	FDataForgeResult MaterializeAssetLayout();
	void SelectBindingPreset(UDataForgeBindingPreset* Preset);
	void SetBindingPresetOutputFolder(const FString& OutputFolder);
	FDataForgeResult MaterializeBindingPreset();
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
	TWeakObjectPtr<UDataForgeBindingPreset> SelectedBindingPreset;
	FString BindingPresetOutputFolder = TEXT("/Game/DataForgeGenerated");
	TMap<FName, FString> AssetLayoutParameterValues;
	int32 RemoveInvalidGeneratedOutputBindings();
};
