#include "DataForgeRuleCreationWorkflow.h"

#include "DataForgeAssetLayoutAuthoring.h"
#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeBindingPreset.h"
#include "DataForgeBindingPresetAuthoring.h"
#include "DataForgeEditorService.h"
#include "DataForgePipeline.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "DataForgeRuleCreationWorkflow"

FDataForgeRuleCreationWorkflow::FDataForgeRuleCreationWorkflow(UDataForgeRuleSet& InTarget)
	: Target(&InTarget)
{
	const FName DraftName = MakeUniqueObjectName(GetTransientPackage(), UDataForgeRuleSet::StaticClass(), TEXT("DataForgeRuleWizardDraft"));
	Draft.Reset(DuplicateObject<UDataForgeRuleSet>(&InTarget, GetTransientPackage(), DraftName));
	bManualAssetLayout = !GetDraft().ProfileOrigin.IsSet();
	SelectedAssetLayoutProfile = GetDraft().ProfileOrigin.Profile.LoadSynchronous();
	AssetLayoutParameterValues = GetDraft().ProfileOrigin.ParameterValues;
	SelectedBindingPreset = GetDraft().BindingPreset.LoadSynchronous();
	if (const UDataForgeBindingPreset* Preset = SelectedBindingPreset.Get())
	{
		const FName RuleId(*(Preset->OutputName.ToString() + TEXT("_Managed")));
		if (const FDataForgeAssetRule* Rule = GetDraft().AssetRules.FindByPredicate([RuleId](const FDataForgeAssetRule& Candidate) { return Candidate.RuleId == RuleId; }))
		{
			BindingPresetOutputFolder = Rule->BaseFolder;
		}
	}
	bAssetLayoutReady = bManualAssetLayout;
}

UDataForgeRuleSet& FDataForgeRuleCreationWorkflow::GetDraft() const
{
	check(Draft.IsValid());
	return *Draft.Get();
}

EDataForgeWizardStep FDataForgeRuleCreationWorkflow::GetStep() const
{
	return Step;
}

const FDataForgeDataSet& FDataForgeRuleCreationWorkflow::GetProbedDataSet() const
{
	return ProbedDataSet;
}

const FDataForgeApplyPlan& FDataForgeRuleCreationWorkflow::GetPreviewPlan() const
{
	return PreviewPlan;
}

const FString& FDataForgeRuleCreationWorkflow::GetLastMessage() const
{
	return LastMessage;
}

UDataForgeAssetLayoutProfile* FDataForgeRuleCreationWorkflow::GetSelectedAssetLayoutProfile() const
{
	return SelectedAssetLayoutProfile.Get();
}

UDataForgeBindingPreset* FDataForgeRuleCreationWorkflow::GetSelectedBindingPreset() const
{
	return SelectedBindingPreset.Get();
}

const FString& FDataForgeRuleCreationWorkflow::GetBindingPresetOutputFolder() const
{
	return BindingPresetOutputFolder;
}

const TMap<FName, FString>& FDataForgeRuleCreationWorkflow::GetAssetLayoutParameterValues() const
{
	return AssetLayoutParameterValues;
}

bool FDataForgeRuleCreationWorkflow::IsManualAssetLayout() const
{
	return bManualAssetLayout;
}

FDataForgeResult FDataForgeRuleCreationWorkflow::Probe()
{
	FDataForgeResult Result = FDataForgeEditorService::Probe(GetDraft(), &ProbedDataSet);
	bProbeSucceeded = Result.bSuccess;
	bAssetLayoutReady = bManualAssetLayout;
	if (bProbeSucceeded)
	{
		FDataForgeSchemaRule& Schema = GetDraft().Schema;
		if (Schema.RequiredColumns.IsEmpty())
		{
			Schema.RequiredColumns = ProbedDataSet.Columns;
		}
		if (Schema.PrimaryKey.IsNone() || !ProbedDataSet.Columns.Contains(Schema.PrimaryKey))
		{
			const auto FindColumn = [this](const TCHAR* Name) -> FName
			{
				const FName Candidate(Name);
				return ProbedDataSet.Columns.Contains(Candidate) ? Candidate : NAME_None;
			};
			Schema.PrimaryKey = FindColumn(TEXT("RowName"));
			if (Schema.PrimaryKey.IsNone()) Schema.PrimaryKey = FindColumn(TEXT("Id"));
			if (Schema.PrimaryKey.IsNone())
			{
				for (const FName Column : ProbedDataSet.Columns)
				{
					if (Column.ToString().EndsWith(TEXT("Id"), ESearchCase::IgnoreCase))
					{
						Schema.PrimaryKey = Column;
						break;
					}
				}
			}
			if (Schema.PrimaryKey.IsNone() && !ProbedDataSet.Columns.IsEmpty())
			{
				Schema.PrimaryKey = ProbedDataSet.Columns[0];
			}
		}
	}
	bPreviewSucceeded = false;
	PreviewPlan = FDataForgeApplyPlan();
	LastMessage = bProbeSucceeded
		? FString::Printf(TEXT("%s Schema inferred: %d required column(s), primary key '%s'."), *Result.Summary, GetDraft().Schema.RequiredColumns.Num(), *GetDraft().Schema.PrimaryKey.ToString())
		: Result.Summary;
	return Result;
}

void FDataForgeRuleCreationWorkflow::SelectAssetLayoutProfile(UDataForgeAssetLayoutProfile* Profile)
{
	SelectedAssetLayoutProfile = Profile;
	AssetLayoutParameterValues.Reset();
	bManualAssetLayout = Profile == nullptr;
	bAssetLayoutReady = bManualAssetLayout;
	bPreviewSucceeded = false;
	PreviewPlan = FDataForgeApplyPlan();

	if (!Profile)
	{
		FDataForgeAssetLayoutAuthoring::Detach(GetDraft());
		LastMessage = TEXT("Manual Asset Layout selected. Existing concrete rules remain editable in the next step.");
		return;
	}

	const bool bSameProfile = GetDraft().ProfileOrigin.ProfileId == Profile->ProfileId;
	for (const FDataForgeProfileParameter& Parameter : Profile->Parameters)
	{
		const FString* Existing = bSameProfile ? GetDraft().ProfileOrigin.ParameterValues.Find(Parameter.Name) : nullptr;
		AssetLayoutParameterValues.Add(Parameter.Name, Existing ? *Existing : Parameter.DefaultValue);
	}
	LastMessage = TEXT("Profile selected. Configure parameters, then Materialize Profile before continuing.");
}

void FDataForgeRuleCreationWorkflow::SetAssetLayoutParameter(FName Name, const FString& Value)
{
	if (!SelectedAssetLayoutProfile.IsValid()) return;
	AssetLayoutParameterValues.Add(Name, Value);
	bAssetLayoutReady = false;
	bPreviewSucceeded = false;
	PreviewPlan = FDataForgeApplyPlan();
	LastMessage = TEXT("Profile parameter changed. Materialize Profile again before continuing.");
}

FDataForgeResult FDataForgeRuleCreationWorkflow::MaterializeAssetLayout()
{
	UDataForgeAssetLayoutProfile* Profile = SelectedAssetLayoutProfile.Get();
	if (!Profile)
	{
		FDataForgeResult Result;
		Result.bSuccess = bManualAssetLayout;
		Result.Summary = bManualAssetLayout
			? TEXT("Manual Asset Layout is ready.")
			: TEXT("Select an available Asset Layout Profile or choose Manual.");
		LastMessage = Result.Summary;
		return Result;
	}
	if (!bProbeSucceeded)
	{
		FDataForgeResult Result;
		FDataForgeDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF1622");
		Diagnostic.Message = TEXT("Run a successful source Probe before materializing an Asset Layout Profile.");
		Result.Summary = Diagnostic.Message;
		LastMessage = Result.Summary;
		return Result;
	}

	FDataForgeResult Result = FDataForgeAssetLayoutAuthoring::Materialize(
		GetDraft(),
		*Profile,
		AssetLayoutParameterValues,
		ProbedDataSet.Columns);
	bAssetLayoutReady = Result.bSuccess;
	bPreviewSucceeded = false;
	PreviewPlan = FDataForgeApplyPlan();
	LastMessage = Result.Summary;
	return Result;
}

void FDataForgeRuleCreationWorkflow::SelectBindingPreset(UDataForgeBindingPreset* Preset)
{
	SelectedBindingPreset = Preset;
	bPreviewSucceeded = false;
	PreviewPlan = FDataForgeApplyPlan();
	LastMessage = Preset
		? TEXT("Binding Preset selected. Choose the generated output folder, then Apply Preset.")
		: TEXT("Binding Preset cleared. Existing concrete output rules remain unchanged.");
}

void FDataForgeRuleCreationWorkflow::SetBindingPresetOutputFolder(const FString& OutputFolder)
{
	BindingPresetOutputFolder = OutputFolder;
	bPreviewSucceeded = false;
	PreviewPlan = FDataForgeApplyPlan();
	LastMessage = TEXT("Binding Preset output folder changed. Apply Preset again before continuing.");
}

FDataForgeResult FDataForgeRuleCreationWorkflow::MaterializeBindingPreset()
{
	FDataForgeResult Result;
	UDataForgeBindingPreset* Preset = SelectedBindingPreset.Get();
	if (!Preset)
	{
		FDataForgeDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF1915");
		Diagnostic.Message = TEXT("Select a Binding Preset before applying it.");
		Result.Summary = Diagnostic.Message;
		LastMessage = Result.Summary;
		return Result;
	}
	if (!bProbeSucceeded)
	{
		FDataForgeDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF1913");
		Diagnostic.Message = TEXT("Run Probe and confirm the Primary Key before applying a Binding Preset.");
		Result.Summary = Diagnostic.Message;
		LastMessage = Result.Summary;
		return Result;
	}

	const FDataForgeBindingPresetMaterialization Materialized = FDataForgeBindingPresetAuthoring::Materialize(GetDraft(), *Preset, BindingPresetOutputFolder);
	Result.bSuccess = Materialized.bSuccess;
	Result.Diagnostics = Materialized.Diagnostics;
	if (Result.bSuccess)
	{
		const int32 AddedBindings = FDataForgeEditorService::AutoMapExactNames(GetDraft(), ProbedDataSet.Columns);
		Result.Summary = Materialized.MakeSummary() + FString::Printf(TEXT(" %d exact-name binding(s) added."), AddedBindings);
	}
	else
	{
		Result.Summary = Materialized.MakeSummary();
	}
	bPreviewSucceeded = false;
	PreviewPlan = FDataForgeApplyPlan();
	LastMessage = Result.Summary;
	return Result;
}

int32 FDataForgeRuleCreationWorkflow::AutoMapExactNames()
{
	const int32 AddedCount = bProbeSucceeded
		? FDataForgeEditorService::AutoMapExactNames(GetDraft(), ProbedDataSet.Columns)
		: 0;
	bPreviewSucceeded = false;
	LastMessage = FString::Printf(TEXT("Auto Map added %d exact-name bindings."), AddedCount);
	return AddedCount;
}

FDataForgeResult FDataForgeRuleCreationWorkflow::Preview()
{
	FDataForgeResult Result = FDataForgeEditorService::Preview(GetDraft(), &PreviewPlan);
	bPreviewSucceeded = Result.bSuccess;
	LastMessage = Result.Summary;
	return Result;
}

void FDataForgeRuleCreationWorkflow::NotifyDraftChanged(FName MemberPropertyName)
{
	bPreviewSucceeded = false;
	PreviewPlan = FDataForgeApplyPlan();
	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Source))
	{
		bProbeSucceeded = false;
		bAssetLayoutReady = bManualAssetLayout;
		ProbedDataSet = FDataForgeDataSet();
	}
	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, GeneratedOutputs))
	{
		const int32 RemovedCount = RemoveInvalidGeneratedOutputBindings();
		const int32 AddedCount = AutoMapExactNames();
		LastMessage = FString::Printf(TEXT("Generated Outputs synchronized: %d binding(s) added, %d invalid binding(s) removed."), AddedCount, RemovedCount);
		return;
	}
	LastMessage = TEXT("Draft changed. Re-run the current validation step.");
}

int32 FDataForgeRuleCreationWorkflow::RemoveInvalidGeneratedOutputBindings()
{
	UDataForgeRuleSet& RuleSet = GetDraft();
	const auto FindOutput = [&RuleSet](FName OutputName)
	{
		return RuleSet.GeneratedOutputs.FindByPredicate([OutputName](const FDataForgeGeneratedAssetOutputRule& Output)
		{
			return !OutputName.IsNone() && Output.OutputName == OutputName;
		});
	};
	return RuleSet.Bindings.RemoveAll([&FindOutput](const FDataForgeBindingRule& Binding)
	{
		if (Binding.Source == EDataForgeBindingSource::GeneratedOutput && !FindOutput(Binding.SourceOutput))
		{
			return true;
		}
		if (Binding.Target != EDataForgeBindingTarget::GeneratedOutput)
		{
			return false;
		}
		return !FindOutput(Binding.TargetOutput);
	});
}

bool FDataForgeRuleCreationWorkflow::CanAdvance(FString& OutReason) const
{
	OutReason.Reset();
	const UDataForgeRuleSet& RuleSet = GetDraft();
	switch (Step)
	{
	case EDataForgeWizardStep::Source:
		if (!FDataForgeSourceAdapterRegistry::Get().Find(RuleSet.Source.AdapterId))
		{
			OutReason = FString::Printf(TEXT("Select a registered source adapter. '%s' is unavailable."), *RuleSet.Source.AdapterId.ToString());
			return false;
		}
		return true;
	case EDataForgeWizardStep::Probe:
		if (!bProbeSucceeded)
		{
			OutReason = TEXT("Run a successful source probe before continuing.");
			return false;
		}
		return true;
	case EDataForgeWizardStep::Schema:
		if (RuleSet.Schema.PrimaryKey.IsNone() || !ProbedDataSet.Columns.Contains(RuleSet.Schema.PrimaryKey))
		{
			OutReason = TEXT("Select a detected column as the primary key.");
			return false;
		}
		return true;
	case EDataForgeWizardStep::Output:
		if (!RuleSet.Output.RowStruct || !RuleSet.Output.RowStruct->IsChildOf(FTableRowBase::StaticStruct()))
		{
			OutReason = TEXT("Select a DataTable row struct derived from FTableRowBase.");
			return false;
		}
		if (!FPackageName::IsValidLongPackageName(RuleSet.Output.AssetPath))
		{
			OutReason = TEXT("Enter a valid DataTable package path such as /Game/Data/DT_Items.");
			return false;
		}
		return true;
	case EDataForgeWizardStep::AssetLayout:
		if (bManualAssetLayout) return true;
		if (!SelectedAssetLayoutProfile.IsValid())
		{
			OutReason = TEXT("The selected Asset Layout Profile is unavailable. Select another Profile or choose Manual.");
			return false;
		}
		if (!bAssetLayoutReady)
		{
			OutReason = TEXT("Materialize the selected Profile with the current parameters before continuing.");
			return false;
		}
		return true;
	case EDataForgeWizardStep::AssetRules:
	{
		TSet<FName> RuleIds;
		for (const FDataForgeAssetRule& AssetRule : RuleSet.AssetRules)
		{
			if (AssetRule.RuleId.IsNone() || RuleIds.Contains(AssetRule.RuleId))
			{
				OutReason = TEXT("Every Asset Rule needs a unique Rule Id.");
				return false;
			}
			RuleIds.Add(AssetRule.RuleId);
			if (!FPackageName::IsValidLongPackageName(AssetRule.BaseFolder) || AssetRule.AssetNamePattern.IsEmpty())
			{
				OutReason = FString::Printf(TEXT("Asset Rule '%s' needs a valid /Game folder and asset name pattern."), *AssetRule.RuleId.ToString());
				return false;
			}
		}
		return true;
	}
	case EDataForgeWizardStep::GeneratedOutputs:
	{
		TSet<FName> OutputNames;
		for (const FDataForgeGeneratedAssetOutputRule& Output : RuleSet.GeneratedOutputs)
		{
			if (Output.OutputName.IsNone() || OutputNames.Contains(Output.OutputName) || !Output.AssetClass.Get())
			{
				OutReason = TEXT("Every Generated Output needs a unique Output Name and an Asset Class.");
				return false;
			}
			OutputNames.Add(Output.OutputName);
			const FDataForgeAssetRule* AssetRule = RuleSet.AssetRules.FindByPredicate([&Output](const FDataForgeAssetRule& Candidate)
			{
				return Candidate.RuleId == Output.AssetRuleId;
			});
			if (!AssetRule || AssetRule->Ownership != EDataForgeAssetOwnership::Managed)
			{
				OutReason = FString::Printf(TEXT("Generated Output '%s' must select a Managed Asset Rule."), *Output.OutputName.ToString());
				return false;
			}
		}
		return true;
	}
	case EDataForgeWizardStep::Bindings:
		return true;
	case EDataForgeWizardStep::Preview:
		if (!bPreviewSucceeded)
		{
			OutReason = TEXT("Run a successful mutation-free Preview before finishing.");
			return false;
		}
		return true;
	default:
		return false;
	}
}

bool FDataForgeRuleCreationWorkflow::Next(FString& OutReason)
{
	if (!CanAdvance(OutReason) || Step == EDataForgeWizardStep::Preview)
	{
		return false;
	}
	Step = static_cast<EDataForgeWizardStep>(static_cast<uint8>(Step) + 1);
	if (Step == EDataForgeWizardStep::Bindings)
	{
		AutoMapExactNames();
	}
	return true;
}

void FDataForgeRuleCreationWorkflow::Back()
{
	if (Step != EDataForgeWizardStep::Source)
	{
		Step = static_cast<EDataForgeWizardStep>(static_cast<uint8>(Step) - 1);
	}
}

bool FDataForgeRuleCreationWorkflow::Finish(FString& OutReason)
{
	if (Step != EDataForgeWizardStep::Preview || !CanAdvance(OutReason))
	{
		if (OutReason.IsEmpty())
		{
			OutReason = TEXT("Complete every wizard step before finishing.");
		}
		return false;
	}

	UDataForgeRuleSet* TargetRuleSet = Target.Get();
	if (!TargetRuleSet)
	{
		OutReason = TEXT("The target RuleSet is no longer available.");
		return false;
	}

	const UDataForgeRuleSet& DraftRuleSet = GetDraft();
	const FScopedTransaction Transaction(LOCTEXT("CommitWizard", "Commit DataForge Rule Creation Wizard"));
	TargetRuleSet->Modify();
	TargetRuleSet->RuleVersion = DraftRuleSet.RuleVersion;
	TargetRuleSet->Source = DraftRuleSet.Source;
	TargetRuleSet->Schema = DraftRuleSet.Schema;
	TargetRuleSet->Output = DraftRuleSet.Output;
	TargetRuleSet->AssetRules = DraftRuleSet.AssetRules;
	TargetRuleSet->ProfileOrigin = DraftRuleSet.ProfileOrigin;
	TargetRuleSet->BindingPreset = DraftRuleSet.BindingPreset;
	TargetRuleSet->AssociationSources = DraftRuleSet.AssociationSources;
	TargetRuleSet->GeneratedOutputs = DraftRuleSet.GeneratedOutputs;
	TargetRuleSet->Bindings = DraftRuleSet.Bindings;
	TargetRuleSet->Dependencies = DraftRuleSet.Dependencies;
#if WITH_EDITORONLY_DATA
	TargetRuleSet->LastStatus = TEXT("Wizard Committed");
	TargetRuleSet->LastSummary = TEXT("Wizard configuration committed. Preparing automatic Apply.");
	TargetRuleSet->LastDetectedColumns = ProbedDataSet.Columns;
	TargetRuleSet->LastDiagnostics.Reset();
#endif
	if (TargetRuleSet->GetPackage() != GetTransientPackage())
	{
		TargetRuleSet->MarkPackageDirty();
	}
	const FDataForgeResult PreviewResult = FDataForgeEditorService::Preview(*TargetRuleSet);
	if (!PreviewResult.bSuccess)
	{
		OutReason = TEXT("Configuration was saved, but automatic Apply was blocked: ") + PreviewResult.Summary;
		return false;
	}

	const FDataForgeResult ApplyResult = FDataForgeEditorService::Apply(*TargetRuleSet);
	if (!ApplyResult.bSuccess)
	{
		OutReason = TEXT("Configuration was saved, but automatic Apply failed: ") + ApplyResult.Summary;
		return false;
	}

	OutReason = ApplyResult.Summary;
	return true;
}

#undef LOCTEXT_NAMESPACE
