#include "DataForgeRuleSetSemanticDiff.h"

#include "DataForgeRuleSet.h"

namespace DataForgeSemanticDiff
{
	void AddValue(
		TArray<FDataForgeSemanticDiffEntry>& Entries,
		const FString& Path,
		const FString& Before,
		const FString& After)
	{
		if (Before == After)
		{
			return;
		}
		FDataForgeSemanticDiffEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Path = Path;
		Entry.BeforeValue = Before;
		Entry.AfterValue = After;
		Entry.Kind = Before == TEXT("<missing>")
			? EDataForgeSemanticDiffKind::Added
			: After == TEXT("<missing>")
				? EDataForgeSemanticDiffKind::Removed
				: EDataForgeSemanticDiffKind::Modified;
	}

	FString Bool(bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString Names(const TArray<FName>& Values)
	{
		TArray<FName> Sorted = Values;
		Sorted.Sort(FNameLexicalLess());
		TArray<FString> Strings;
		for (const FName Value : Sorted)
		{
			Strings.Add(Value.ToString());
		}
		return FString::Join(Strings, TEXT(","));
	}

	template <typename ValueType>
	TArray<FName> NameUnion(const TMap<FName, ValueType>& Before, const TMap<FName, ValueType>& After)
	{
		TSet<FName> Keys;
		for (const TPair<FName, ValueType>& Pair : Before) Keys.Add(Pair.Key);
		for (const TPair<FName, ValueType>& Pair : After) Keys.Add(Pair.Key);
		TArray<FName> Result = Keys.Array();
		Result.Sort(FNameLexicalLess());
		return Result;
	}

	FString BindingKey(const FDataForgeBindingRule& Binding)
	{
		const FString Owner = Binding.Target == EDataForgeBindingTarget::DataTableRow
			? TEXT("row")
			: Binding.TargetOutput.ToString();
		return Owner + TEXT(".") + Binding.TargetProperty;
	}

	FString BindingSource(const FDataForgeBindingRule& Binding)
	{
		if (Binding.Source == EDataForgeBindingSource::GeneratedOutput)
		{
			return TEXT("output:") + Binding.SourceOutput.ToString();
		}
		const FString Prefix = Binding.Source == EDataForgeBindingSource::ResolvedAsset ? TEXT("asset:") : TEXT("field:");
		return Prefix + Binding.SourceColumn.ToString();
	}

	FString SourceInputs(const TArray<FDataForgeSourceInput>& Inputs)
	{
		TArray<FString> Values;
		for (int32 Index = 0; Index < Inputs.Num(); ++Index)
		{
			const FDataForgeSourceInput& Input = Inputs[Index];
			FString Value = FString::Printf(TEXT("%d:%s:%s:%s:%s:%s"), Index, *Input.AdapterId.ToString(), *Input.File.FilePath,
				*Input.SourceAsset.ToSoftObjectPath().ToString(), *Input.JoinColumn.ToString(), *Input.ColumnPrefix);
			TArray<FName> Keys;
			Input.Parameters.GetKeys(Keys);
			Keys.Sort(FNameLexicalLess());
			for (const FName Key : Keys) Value += FString::Printf(TEXT(":%s=%s"), *Key.ToString(), *Input.Parameters.FindChecked(Key));
			Values.Add(MoveTemp(Value));
		}
		return FString::Join(Values, TEXT(";"));
	}

	FString AssociationSources(const TArray<FDataForgeAssociationSourceRule>& Sources)
	{
		TArray<FString> Values;
		for (const FDataForgeAssociationSourceRule& Source : Sources)
		{
			FString Value = FString::Printf(TEXT("%s:%s:%s:%s:%s:%s:%s:%s"), *Source.SourceId.ToString(), *Source.Source.AdapterId.ToString(),
				*Source.Source.File.FilePath, *Source.Source.SourceAsset.ToSoftObjectPath().ToString(), *Source.MatchColumn.ToString(),
				*Source.AssetPathColumn.ToString(), *Source.AssetKindColumn.ToString(), *Source.RoleColumn.ToString());
			TArray<FName> Keys;
			Source.Source.Parameters.GetKeys(Keys);
			Keys.Sort(FNameLexicalLess());
			for (const FName Key : Keys) Value += FString::Printf(TEXT(":%s=%s"), *Key.ToString(), *Source.Source.Parameters.FindChecked(Key));
			Values.Add(MoveTemp(Value));
		}
		Values.Sort();
		return FString::Join(Values, TEXT(";"));
	}
}

FString FDataForgeSemanticDiffEntry::ToDisplayString() const
{
	const TCHAR* KindText = Kind == EDataForgeSemanticDiffKind::Added
		? TEXT("+")
		: Kind == EDataForgeSemanticDiffKind::Removed ? TEXT("-") : TEXT("~");
	return FString::Printf(TEXT("%s %s\n    %s  ->  %s"), KindText, *Path, *BeforeValue, *AfterValue);
}

TArray<FDataForgeSemanticDiffEntry> FDataForgeRuleSetSemanticDiff::Compare(
	const UDataForgeRuleSet& Before,
	const UDataForgeRuleSet& After)
{
	TArray<FDataForgeSemanticDiffEntry> Entries;
	using namespace DataForgeSemanticDiff;

	AddValue(Entries, TEXT("RuleVersion"), FString::FromInt(Before.RuleVersion), FString::FromInt(After.RuleVersion));
	AddValue(Entries, TEXT("Source.AdapterId"), Before.Source.AdapterId.ToString(), After.Source.AdapterId.ToString());
	AddValue(Entries, TEXT("Source.File"), Before.Source.File.FilePath, After.Source.File.FilePath);
	AddValue(Entries, TEXT("Source.Asset"), Before.Source.SourceAsset.ToSoftObjectPath().ToString(), After.Source.SourceAsset.ToSoftObjectPath().ToString());
	AddValue(Entries, TEXT("Source.Inputs"), SourceInputs(Before.Source.Inputs), SourceInputs(After.Source.Inputs));
	AddValue(Entries, TEXT("Source.ProbeRowLimit"), FString::FromInt(Before.Source.ProbeRowLimit), FString::FromInt(After.Source.ProbeRowLimit));

	TSet<FName> ParameterKeys;
	for (const TPair<FName, FString>& Pair : Before.Source.Parameters) ParameterKeys.Add(Pair.Key);
	for (const TPair<FName, FString>& Pair : After.Source.Parameters) ParameterKeys.Add(Pair.Key);
	TArray<FName> SortedParameterKeys = ParameterKeys.Array();
	SortedParameterKeys.Sort(FNameLexicalLess());
	for (const FName Key : SortedParameterKeys)
	{
		const FString* BeforeValue = Before.Source.Parameters.Find(Key);
		const FString* AfterValue = After.Source.Parameters.Find(Key);
		AddValue(Entries, FString::Printf(TEXT("Source.Parameters[%s]"), *Key.ToString()), BeforeValue ? *BeforeValue : TEXT("<missing>"), AfterValue ? *AfterValue : TEXT("<missing>"));
	}

	AddValue(Entries, TEXT("Schema.PrimaryKey"), Before.Schema.PrimaryKey.ToString(), After.Schema.PrimaryKey.ToString());
	AddValue(Entries, TEXT("Schema.RequiredColumns"), Names(Before.Schema.RequiredColumns), Names(After.Schema.RequiredColumns));
	AddValue(Entries, TEXT("Schema.WarnOnUnmappedColumns"), Bool(Before.Schema.bWarnOnUnmappedColumns), Bool(After.Schema.bWarnOnUnmappedColumns));
	AddValue(Entries, TEXT("Output.RowStruct"), Before.Output.RowStruct ? Before.Output.RowStruct->GetPathName() : TEXT("None"), After.Output.RowStruct ? After.Output.RowStruct->GetPathName() : TEXT("None"));
	AddValue(Entries, TEXT("Output.AssetPath"), Before.Output.AssetPath, After.Output.AssetPath);
	AddValue(Entries, TEXT("Output.CreateIfMissing"), Bool(Before.Output.bCreateIfMissing), Bool(After.Output.bCreateIfMissing));
	AddValue(Entries, TEXT("Output.RemoveRowsMissingFromSource"), Bool(Before.Output.bRemoveRowsMissingFromSource), Bool(After.Output.bRemoveRowsMissingFromSource));
	AddValue(Entries, TEXT("Output.SaveAfterApply"), Bool(Before.Output.bSaveAfterApply), Bool(After.Output.bSaveAfterApply));
	AddValue(Entries, TEXT("BindingPreset"), Before.BindingPreset.ToSoftObjectPath().ToString(), After.BindingPreset.ToSoftObjectPath().ToString());
	AddValue(Entries, TEXT("AssociationSources"), AssociationSources(Before.AssociationSources), AssociationSources(After.AssociationSources));

	TMap<FName, const FDataForgeAssetRule*> BeforeAssetRules;
	TMap<FName, const FDataForgeAssetRule*> AfterAssetRules;
	for (const FDataForgeAssetRule& Rule : Before.AssetRules) BeforeAssetRules.Add(Rule.RuleId, &Rule);
	for (const FDataForgeAssetRule& Rule : After.AssetRules) AfterAssetRules.Add(Rule.RuleId, &Rule);
	for (const FName Id : NameUnion(BeforeAssetRules, AfterAssetRules))
	{
		const FDataForgeAssetRule* const* BeforeRule = BeforeAssetRules.Find(Id);
		const FDataForgeAssetRule* const* AfterRule = AfterAssetRules.Find(Id);
		const FString Prefix = FString::Printf(TEXT("AssetRules[%s]"), *Id.ToString());
		if (!BeforeRule || !AfterRule)
		{
			AddValue(Entries, Prefix, BeforeRule ? TEXT("present") : TEXT("<missing>"), AfterRule ? TEXT("present") : TEXT("<missing>"));
			continue;
		}
		AddValue(Entries, Prefix + TEXT(".Ownership"), FString::FromInt(static_cast<int32>((*BeforeRule)->Ownership)), FString::FromInt(static_cast<int32>((*AfterRule)->Ownership)));
		AddValue(Entries, Prefix + TEXT(".BaseFolder"), (*BeforeRule)->BaseFolder, (*AfterRule)->BaseFolder);
		AddValue(Entries, Prefix + TEXT(".SubfolderPattern"), (*BeforeRule)->SubfolderPattern, (*AfterRule)->SubfolderPattern);
		AddValue(Entries, Prefix + TEXT(".AssetNamePattern"), (*BeforeRule)->AssetNamePattern, (*AfterRule)->AssetNamePattern);
	}

	TMap<FName, const FDataForgeGeneratedAssetOutputRule*> BeforeOutputs;
	TMap<FName, const FDataForgeGeneratedAssetOutputRule*> AfterOutputs;
	for (const FDataForgeGeneratedAssetOutputRule& Output : Before.GeneratedOutputs) BeforeOutputs.Add(Output.OutputName, &Output);
	for (const FDataForgeGeneratedAssetOutputRule& Output : After.GeneratedOutputs) AfterOutputs.Add(Output.OutputName, &Output);
	for (const FName Name : NameUnion(BeforeOutputs, AfterOutputs))
	{
		const FDataForgeGeneratedAssetOutputRule* const* BeforeOutput = BeforeOutputs.Find(Name);
		const FDataForgeGeneratedAssetOutputRule* const* AfterOutput = AfterOutputs.Find(Name);
		const FString Prefix = FString::Printf(TEXT("GeneratedOutputs[%s]"), *Name.ToString());
		if (!BeforeOutput || !AfterOutput)
		{
			AddValue(Entries, Prefix, BeforeOutput ? TEXT("present") : TEXT("<missing>"), AfterOutput ? TEXT("present") : TEXT("<missing>"));
			continue;
		}
		AddValue(Entries, Prefix + TEXT(".Type"), FString::FromInt(static_cast<int32>((*BeforeOutput)->Type)), FString::FromInt(static_cast<int32>((*AfterOutput)->Type)));
		AddValue(Entries, Prefix + TEXT(".AssetClass"), (*BeforeOutput)->AssetClass ? (*BeforeOutput)->AssetClass->GetPathName() : TEXT("None"), (*AfterOutput)->AssetClass ? (*AfterOutput)->AssetClass->GetPathName() : TEXT("None"));
		AddValue(Entries, Prefix + TEXT(".AssetRuleId"), (*BeforeOutput)->AssetRuleId.ToString(), (*AfterOutput)->AssetRuleId.ToString());
		AddValue(Entries, Prefix + TEXT(".AdoptCompatibleUnownedAsset"), (*BeforeOutput)->bAdoptCompatibleUnownedAsset ? TEXT("true") : TEXT("false"), (*AfterOutput)->bAdoptCompatibleUnownedAsset ? TEXT("true") : TEXT("false"));
	}

	TMap<FString, const FDataForgeBindingRule*> BeforeBindings;
	TMap<FString, const FDataForgeBindingRule*> AfterBindings;
	for (const FDataForgeBindingRule& Binding : Before.Bindings) BeforeBindings.Add(BindingKey(Binding), &Binding);
	for (const FDataForgeBindingRule& Binding : After.Bindings) AfterBindings.Add(BindingKey(Binding), &Binding);
	TSet<FString> BindingKeys;
	for (const TPair<FString, const FDataForgeBindingRule*>& Pair : BeforeBindings) BindingKeys.Add(Pair.Key);
	for (const TPair<FString, const FDataForgeBindingRule*>& Pair : AfterBindings) BindingKeys.Add(Pair.Key);
	TArray<FString> SortedBindingKeys = BindingKeys.Array();
	SortedBindingKeys.Sort();
	for (const FString& Key : SortedBindingKeys)
	{
		const FDataForgeBindingRule* const* BeforeBinding = BeforeBindings.Find(Key);
		const FDataForgeBindingRule* const* AfterBinding = AfterBindings.Find(Key);
		const FString Prefix = FString::Printf(TEXT("Bindings[%s]"), *Key);
		if (!BeforeBinding || !AfterBinding)
		{
			AddValue(Entries, Prefix, BeforeBinding ? TEXT("present") : TEXT("<missing>"), AfterBinding ? TEXT("present") : TEXT("<missing>"));
			continue;
		}
		AddValue(Entries, Prefix + TEXT(".Source"), BindingSource(**BeforeBinding), BindingSource(**AfterBinding));
		AddValue(Entries, Prefix + TEXT(".AssetRuleId"), (*BeforeBinding)->AssetRuleId.ToString(), (*AfterBinding)->AssetRuleId.ToString());
		AddValue(Entries, Prefix + TEXT(".Required"), Bool((*BeforeBinding)->bRequired), Bool((*AfterBinding)->bRequired));
	}

	TSet<FString> BeforeDependencies;
	TSet<FString> AfterDependencies;
	for (const FDataForgeDependencyRule& Dependency : Before.Dependencies)
	{
		BeforeDependencies.Add(Dependency.RuleSet.ToSoftObjectPath().ToString());
	}
	for (const FDataForgeDependencyRule& Dependency : After.Dependencies)
	{
		AfterDependencies.Add(Dependency.RuleSet.ToSoftObjectPath().ToString());
	}
	TSet<FString> DependencyPaths = BeforeDependencies;
	DependencyPaths.Append(AfterDependencies);
	TArray<FString> SortedDependencyPaths = DependencyPaths.Array();
	SortedDependencyPaths.Sort();
	for (const FString& DependencyPath : SortedDependencyPaths)
	{
		AddValue(
			Entries,
			FString::Printf(TEXT("Dependencies[%s]"), *DependencyPath),
			BeforeDependencies.Contains(DependencyPath) ? TEXT("present") : TEXT("<missing>"),
			AfterDependencies.Contains(DependencyPath) ? TEXT("present") : TEXT("<missing>"));
	}

	Entries.Sort([](const FDataForgeSemanticDiffEntry& Left, const FDataForgeSemanticDiffEntry& Right)
	{
		return Left.Path < Right.Path;
	});
	return Entries;
}
