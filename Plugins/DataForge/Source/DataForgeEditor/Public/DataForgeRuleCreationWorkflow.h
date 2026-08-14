#pragma once

#include "CoreMinimal.h"
#include "DataForgeAuthoringMaterializer.h"
#include "DataForgeRuleSet.h"
#include "DataForgeTypes.h"
#include "UObject/StrongObjectPtr.h"

class UDataForgeAssetLayoutProfile;
class UDataForgeBindingPreset;
class UDataForgeNamingPolicy;

struct DATAFORGEEDITOR_API FDataForgeAutomaticSetupDefaults
{
	FString AssetSearchRoot = TEXT("/Game");
	FString GeneratedOutputFolder;
	FString DefinitionFolder;
	TWeakObjectPtr<UDataForgeNamingPolicy> NamingPolicy;
	TWeakObjectPtr<UClass> GeneratedOutputClass;
};

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
	bool HasAutomaticSetup() const;
	bool HasSuccessfulPreview() const;
	bool IsAutomaticReviewApproved() const;
	void SetAutomaticReviewApproved(bool bApproved);
	FDataForgeAutomaticSetupDefaults GetAutomaticSetupDefaults() const;
	FString GetAutomaticSetupInspection() const;

	FDataForgeResult Probe();
	void SelectAssetLayoutProfile(UDataForgeAssetLayoutProfile* Profile);
	void SetAssetLayoutParameter(FName Name, const FString& Value);
	FDataForgeResult MaterializeAssetLayout();
	void SelectBindingPreset(UDataForgeBindingPreset* Preset);
	void SetBindingPresetOutputFolder(const FString& OutputFolder);
	FDataForgeResult MaterializeBindingPreset();
	int32 AutoMapExactNames();
	FDataForgeResult Preview();
	FDataForgeResult ConfigureAutomatically(
		const FString& AssetSearchRoot,
		UDataForgeNamingPolicy* NamingPolicy,
		UClass* GeneratedOutputClass,
		const FString& GeneratedOutputFolder,
		const FString& DefinitionFolder);
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
	bool bAutomaticReviewApproved = false;
	TWeakObjectPtr<UDataForgeAssetLayoutProfile> SelectedAssetLayoutProfile;
	TWeakObjectPtr<UDataForgeBindingPreset> SelectedBindingPreset;
	FString BindingPresetOutputFolder = TEXT("/Game/DataForgeGenerated");
	TMap<FName, FString> AssetLayoutParameterValues;
	TUniquePtr<FDataForgeAuthoringDraft> AutomaticDraft;
	TUniquePtr<FDataForgeAuthoringPlannerResult> AutomaticPlan;
	FString AutomaticDefinitionFolder;
	int32 RemoveInvalidGeneratedOutputBindings();
};
