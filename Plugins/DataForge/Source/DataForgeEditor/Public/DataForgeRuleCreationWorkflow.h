#pragma once

#include "CoreMinimal.h"
#include "DataForgeRuleSet.h"
#include "DataForgeTypes.h"
#include "UObject/StrongObjectPtr.h"

enum class EDataForgeWizardStep : uint8
{
	Source,
	Probe,
	Schema,
	Output,
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

	FDataForgeResult Probe();
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
	bool bPreviewSucceeded = false;
	int32 RemoveInvalidGeneratedOutputBindings();
};
