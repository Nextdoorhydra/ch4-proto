#include "DataForgeBindingGraph.h"

#include "DataForgeEditorService.h"
#include "DataForgeRuleSet.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "DataForgeBindingGraph"

namespace DataForgeBindingGraph
{
	void CollectTargets(
		UStruct* Struct,
		EDataForgeBindingTarget Target,
		FName TargetOutput,
		const FString& Prefix,
		int32 Depth,
		TArray<FDataForgeBindingGraphTarget>& OutTargets)
	{
		if (!Struct || Depth > 4)
		{
			return;
		}
		for (TFieldIterator<FProperty> It(Struct, EFieldIterationFlags::IncludeSuper); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
			{
				continue;
			}
			const FString Path = Prefix.IsEmpty() ? Property->GetName() : Prefix + TEXT(".") + Property->GetName();
			FDataForgeBindingGraphTarget& GraphTarget = OutTargets.AddDefaulted_GetRef();
			GraphTarget.Target = Target;
			GraphTarget.TargetOutput = TargetOutput;
			GraphTarget.PropertyPath = Path;
			GraphTarget.PropertyType = Property->GetCPPType();
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				CollectTargets(StructProperty->Struct, Target, TargetOutput, Path, Depth + 1, OutTargets);
			}
		}
	}

	FString TargetKey(const FDataForgeBindingGraphTarget& Target)
	{
		return FString::Printf(TEXT("%d:%s:%s"), static_cast<int32>(Target.Target), *Target.TargetOutput.ToString(), *Target.PropertyPath);
	}

	FString BindingTargetKey(const FDataForgeBindingRule& Binding)
	{
		return FString::Printf(TEXT("%d:%s:%s"), static_cast<int32>(Binding.Target), *Binding.TargetOutput.ToString(), *Binding.TargetProperty);
	}
}

TArray<FDataForgeBindingGraphSource> FDataForgeBindingGraphModel::BuildSources(
	const UDataForgeRuleSet& RuleSet,
	const FDataForgeDataSet& DataSet)
{
	TArray<FDataForgeBindingGraphSource> Sources;
	for (const FName Column : DataSet.Columns)
	{
		FDataForgeBindingGraphSource& Source = Sources.AddDefaulted_GetRef();
		Source.Kind = EDataForgeGraphSourceKind::SourceColumn;
		Source.Name = Column;
		Source.DisplayName = FString::Printf(TEXT("Field  %s"), *Column.ToString());
	}
	TArray<FName> OutputNames;
	for (const FDataForgeGeneratedAssetOutputRule& Output : RuleSet.GeneratedOutputs)
	{
		OutputNames.Add(Output.OutputName);
	}
	OutputNames.Sort(FNameLexicalLess());
	for (const FName OutputName : OutputNames)
	{
		FDataForgeBindingGraphSource& Source = Sources.AddDefaulted_GetRef();
		Source.Kind = EDataForgeGraphSourceKind::GeneratedOutput;
		Source.Name = OutputName;
		Source.DisplayName = FString::Printf(TEXT("Output  %s"), *OutputName.ToString());
	}
	return Sources;
}

TArray<FDataForgeBindingGraphTarget> FDataForgeBindingGraphModel::BuildTargets(const UDataForgeRuleSet& RuleSet)
{
	TArray<FDataForgeBindingGraphTarget> Targets;
	DataForgeBindingGraph::CollectTargets(RuleSet.Output.RowStruct, EDataForgeBindingTarget::DataTableRow, NAME_None, FString(), 0, Targets);
	for (const FDataForgeGeneratedAssetOutputRule& Output : RuleSet.GeneratedOutputs)
	{
		DataForgeBindingGraph::CollectTargets(Output.AssetClass.Get(), EDataForgeBindingTarget::GeneratedOutput, Output.OutputName, FString(), 0, Targets);
	}
	Targets.Sort([](const FDataForgeBindingGraphTarget& Left, const FDataForgeBindingGraphTarget& Right)
	{
		return DataForgeBindingGraph::TargetKey(Left) < DataForgeBindingGraph::TargetKey(Right);
	});
	return Targets;
}

bool FDataForgeBindingGraphModel::Connect(
	UDataForgeRuleSet& RuleSet,
	const FDataForgeDataSet& DataSet,
	const FDataForgeBindingGraphSource& Source,
	const FDataForgeBindingGraphTarget& Target,
	FString& OutMessage)
{
	OutMessage.Reset();
	const FString DesiredTargetKey = DataForgeBindingGraph::TargetKey(Target);
	if (RuleSet.Bindings.ContainsByPredicate([&DesiredTargetKey](const FDataForgeBindingRule& Binding)
	{
		return DataForgeBindingGraph::BindingTargetKey(Binding).Equals(DesiredTargetKey, ESearchCase::IgnoreCase);
	}))
	{
		OutMessage = TEXT("Target is already bound. Remove or edit the existing binding first.");
		return false;
	}

	FDataForgeBindingRule Candidate;
	Candidate.Target = Target.Target;
	Candidate.TargetOutput = Target.TargetOutput;
	Candidate.TargetProperty = Target.PropertyPath;
	if (Source.Kind == EDataForgeGraphSourceKind::GeneratedOutput)
	{
		if (Target.Target != EDataForgeBindingTarget::DataTableRow)
		{
			OutMessage = TEXT("Generated outputs can only connect back to DataTable row properties.");
			return false;
		}
		Candidate.Source = EDataForgeBindingSource::GeneratedOutput;
		Candidate.SourceOutput = Source.Name;
	}
	else
	{
		if (!DataSet.Columns.Contains(Source.Name))
		{
			OutMessage = TEXT("Source field is not present in the latest Probe.");
			return false;
		}
		Candidate.Source = EDataForgeBindingSource::SourceValue;
		Candidate.SourceColumn = Source.Name;
	}

	const FDataForgeBindingSuggestion Suggestion = FDataForgeEditorService::AnalyzeBinding(RuleSet, Candidate, DataSet);
	if (Suggestion.Compatibility != EDataForgeBindingCompatibility::Direct
		&& Suggestion.Compatibility != EDataForgeBindingCompatibility::Convertible)
	{
		OutMessage = TEXT("Connection blocked: ") + Suggestion.ToDisplayString();
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("ConnectBinding", "Connect DataForge Binding Graph Nodes"));
	RuleSet.Modify();
	RuleSet.Bindings.Add(MoveTemp(Candidate));
#if WITH_EDITORONLY_DATA
	RuleSet.LastStatus = TEXT("Draft");
	RuleSet.LastSummary = TEXT("Binding graph changed. Run Preview before Apply.");
#endif
	if (RuleSet.GetPackage() != GetTransientPackage())
	{
		RuleSet.MarkPackageDirty();
	}
	OutMessage = TEXT("Binding created: ") + Suggestion.ToDisplayString();
	return true;
}

#undef LOCTEXT_NAMESPACE
