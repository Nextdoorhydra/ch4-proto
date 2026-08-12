#include "DataForgeRuleCreationWorkflow.h"

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

FDataForgeResult FDataForgeRuleCreationWorkflow::Probe()
{
	FDataForgeResult Result = FDataForgeEditorService::Probe(GetDraft(), &ProbedDataSet);
	bProbeSucceeded = Result.bSuccess;
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
		ProbedDataSet = FDataForgeDataSet();
	}
	LastMessage = TEXT("Draft changed. Re-run the current validation step.");
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
	TargetRuleSet->GeneratedOutputs = DraftRuleSet.GeneratedOutputs;
	TargetRuleSet->Bindings = DraftRuleSet.Bindings;
	TargetRuleSet->Dependencies = DraftRuleSet.Dependencies;
#if WITH_EDITORONLY_DATA
	TargetRuleSet->LastStatus = TEXT("Draft");
	TargetRuleSet->LastSummary = TEXT("Wizard configuration committed. Run Preview in the RuleSet editor before Apply.");
	TargetRuleSet->LastDetectedColumns = ProbedDataSet.Columns;
	TargetRuleSet->LastDiagnostics.Reset();
#endif
	if (TargetRuleSet->GetPackage() != GetTransientPackage())
	{
		TargetRuleSet->MarkPackageDirty();
	}
	OutReason.Reset();
	return true;
}

#undef LOCTEXT_NAMESPACE
