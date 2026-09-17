#include "DataForgeBindingPresetAuthoring.h"

#include "DataForgeBindingPreset.h"
#include "DataForgeRuleSet.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "DataForgeBindingPresetAuthoring"

namespace DataForgeBindingPresetAuthoring
{
	void AddDiagnostic(TArray<FDataForgeDiagnostic>& Diagnostics, EDataForgeSeverity Severity, const TCHAR* Code, const FString& Message, FName Field = NAME_None)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Diagnostic.Field = Field;
	}

	bool HasErrors(const TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		return Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == EDataForgeSeverity::Error;
		});
	}

	FProperty* ResolveProperty(UStruct* Owner, const FString& Path)
	{
		if (!Owner || Path.IsEmpty()) return nullptr;
		TArray<FString> Segments;
		Path.ParseIntoArray(Segments, TEXT("."), true);
		FProperty* Property = nullptr;
		for (int32 Index = 0; Index < Segments.Num(); ++Index)
		{
			Property = FindFProperty<FProperty>(Owner, *Segments[Index]);
			if (!Property) return nullptr;
			if (Index + 1 < Segments.Num())
			{
				const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
				if (!StructProperty) return nullptr;
				Owner = StructProperty->Struct;
			}
		}
		return Property;
	}

	FProperty* ReferenceProperty(FProperty* Property, EDataForgeBindingCardinality Cardinality)
	{
		if (Cardinality == EDataForgeBindingCardinality::Many)
		{
			const FArrayProperty* Array = CastField<FArrayProperty>(Property);
			return Array ? Array->Inner : nullptr;
		}
		return CastField<FArrayProperty>(Property) ? nullptr : Property;
	}

	bool IsCompatibleReference(FProperty* Property, EDataForgeBindingCardinality Cardinality, UClass* ExpectedClass)
	{
		FProperty* Reference = ReferenceProperty(Property, Cardinality);
		if (const FSoftObjectProperty* SoftObject = CastField<FSoftObjectProperty>(Reference))
		{
			return !ExpectedClass || ExpectedClass->IsChildOf(SoftObject->PropertyClass);
		}
		if (const FSoftClassProperty* SoftClass = CastField<FSoftClassProperty>(Reference))
		{
			return !ExpectedClass || ExpectedClass->IsChildOf(SoftClass->MetaClass);
		}
		return false;
	}

	TArray<FProperty*> CompatibleProperties(UClass* TargetClass, const FDataForgeBindingPresetSlot& Slot)
	{
		TArray<FProperty*> Compatible;
		UClass* ExpectedClass = Slot.ExpectedAssetClass.LoadSynchronous();
		for (TFieldIterator<FProperty> It(TargetClass, EFieldIterationFlags::IncludeSuper); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
			if (IsCompatibleReference(Property, Slot.Cardinality, ExpectedClass)) Compatible.Add(Property);
		}

		TArray<FProperty*> MetadataMatches = Compatible.FilterByPredicate([&Slot](const FProperty* Property)
		{
			const FString Kind = Property->GetMetaData(TEXT("DataForgeKind"));
			const FString Role = Property->GetMetaData(TEXT("DataForgeRole"));
			return (!Slot.AssetKind.IsNone() && Kind == Slot.AssetKind.ToString())
				&& (Slot.Role.IsNone() || Role.IsEmpty() || Role == Slot.Role.ToString());
		});
		return MetadataMatches.IsEmpty() ? Compatible : MetadataMatches;
	}

	FString ResolveSlot(const UDataForgeBindingPreset& Preset, const FDataForgeBindingPresetSlot& Slot, TArray<FDataForgeDiagnostic>& Diagnostics, int32& AmbiguousCount)
	{
		UClass* TargetClass = Preset.TargetClass.Get();
		if (!Slot.TargetProperty.IsEmpty())
		{
			FProperty* Property = ResolveProperty(TargetClass, Slot.TargetProperty);
			if (!Property || !IsCompatibleReference(Property, Slot.Cardinality, Slot.ExpectedAssetClass.LoadSynchronous()))
			{
				AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF1906"), FString::Printf(TEXT("Binding Preset slot '%s' property '%s' is missing or incompatible with its cardinality/class."), *Slot.SlotId.ToString(), *Slot.TargetProperty), Slot.SlotId);
				return FString();
			}
			return Slot.TargetProperty;
		}

		const TArray<FProperty*> Candidates = CompatibleProperties(TargetClass, Slot);
		if (Candidates.Num() == 1) return Candidates[0]->GetName();
		if (Candidates.IsEmpty())
		{
			AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF1907"), FString::Printf(TEXT("Binding Preset slot '%s' has no compatible property on '%s'."), *Slot.SlotId.ToString(), *TargetClass->GetName()), Slot.SlotId);
			return FString();
		}
		++AmbiguousCount;
		AddDiagnostic(Diagnostics, EDataForgeSeverity::Warning, TEXT("DF1908"), FString::Printf(TEXT("Binding Preset slot '%s' matches %d properties on '%s'. Select Target Property explicitly."), *Slot.SlotId.ToString(), Candidates.Num(), *TargetClass->GetName()), Slot.SlotId);
		return FString();
	}

	FString ResolveRowReference(const UDataForgeRuleSet& RuleSet, const UDataForgeBindingPreset& Preset, TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		if (!RuleSet.Output.RowStruct) return FString();
		UClass* TargetClass = Preset.TargetClass.Get();
		if (!Preset.RowReferenceProperty.IsEmpty())
		{
			FProperty* Property = ResolveProperty(RuleSet.Output.RowStruct, Preset.RowReferenceProperty);
			if (!Property || !IsCompatibleReference(Property, EDataForgeBindingCardinality::One, TargetClass))
			{
				AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF1910"), FString::Printf(TEXT("Row Reference Property '%s' cannot receive '%s'."), *Preset.RowReferenceProperty, *TargetClass->GetName()));
				return FString();
			}
			return Preset.RowReferenceProperty;
		}

		TArray<FProperty*> Candidates;
		for (TFieldIterator<FProperty> It(RuleSet.Output.RowStruct, EFieldIterationFlags::IncludeSuper); It; ++It)
		{
			FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
				&& IsCompatibleReference(Property, EDataForgeBindingCardinality::One, TargetClass))
			{
				Candidates.Add(Property);
			}
		}
		if (FProperty** Exact = Candidates.FindByPredicate([&Preset](const FProperty* Property)
		{
			return Property->GetFName() == Preset.OutputName;
		}))
		{
			return (*Exact)->GetName();
		}
		if (Candidates.Num() == 1) return Candidates[0]->GetName();
		if (Candidates.Num() > 1)
		{
			AddDiagnostic(Diagnostics, EDataForgeSeverity::Warning, TEXT("DF1911"), FString::Printf(TEXT("Generated Output '%s' matches %d DataTable Row properties. Set Row Reference Property explicitly."), *Preset.OutputName.ToString(), Candidates.Num()));
		}
		return FString();
	}
}

FString FDataForgeBindingPresetMaterialization::MakeSummary() const
{
	return FString::Printf(TEXT("Binding Preset %s: %d slot(s) resolved, %d ambiguous."), bSuccess ? TEXT("ready") : TEXT("invalid"), ResolvedSlotCount, AmbiguousSlotCount);
}

FDataForgeBindingPresetMaterialization FDataForgeBindingPresetAuthoring::Validate(const UDataForgeBindingPreset& Preset)
{
	FDataForgeBindingPresetMaterialization Result;
	UClass* TargetClass = Preset.TargetClass.Get();
	if (!Preset.PresetId.IsValid()) DataForgeBindingPresetAuthoring::AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1901"), TEXT("Binding Preset requires a stable Preset Id."));
	if (Preset.OutputName.IsNone()) DataForgeBindingPresetAuthoring::AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1902"), TEXT("Binding Preset requires an Output Name."));
	if (!TargetClass || !TargetClass->IsChildOf(UDataAsset::StaticClass()) || TargetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		DataForgeBindingPresetAuthoring::AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1903"), TEXT("Binding Preset Target Class must be a concrete DataAsset class."));
	}
	if (Preset.AssetNamePrefix.IsEmpty() || Preset.AssetNamePrefix.Contains(TEXT("/")) || Preset.AssetNamePrefix.Contains(TEXT("{")))
	{
		DataForgeBindingPresetAuthoring::AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1904"), TEXT("Binding Preset Asset Name Prefix must be a non-empty literal such as DA or PDA."));
	}

	TSet<FName> SlotIds;
	if (TargetClass)
	{
		for (const FDataForgeBindingPresetSlot& Slot : Preset.Slots)
		{
			if (Slot.SlotId.IsNone() || SlotIds.Contains(Slot.SlotId))
			{
				DataForgeBindingPresetAuthoring::AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1905"), TEXT("Binding Preset Slot Ids must be non-empty and unique."), Slot.SlotId);
				continue;
			}
			SlotIds.Add(Slot.SlotId);
			if (Slot.Cardinality == EDataForgeBindingCardinality::Many && Slot.Reconcile == EDataForgeBindingReconcileMode::Assign)
			{
				DataForgeBindingPresetAuthoring::AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1909"), FString::Printf(TEXT("Many slot '%s' must use Replace Managed, Merge By Key, or Manual reconciliation."), *Slot.SlotId.ToString()), Slot.SlotId);
				continue;
			}
			if (Slot.Cardinality != EDataForgeBindingCardinality::Many
				&& Slot.Reconcile != EDataForgeBindingReconcileMode::Assign
				&& Slot.Reconcile != EDataForgeBindingReconcileMode::Manual)
			{
				DataForgeBindingPresetAuthoring::AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1909"), FString::Printf(TEXT("Scalar slot '%s' must use Assign or Manual reconciliation."), *Slot.SlotId.ToString()), Slot.SlotId);
				continue;
			}
			if (!DataForgeBindingPresetAuthoring::ResolveSlot(Preset, Slot, Result.Diagnostics, Result.AmbiguousSlotCount).IsEmpty()) ++Result.ResolvedSlotCount;
		}
	}
	Result.bSuccess = !DataForgeBindingPresetAuthoring::HasErrors(Result.Diagnostics);
	return Result;
}

FDataForgeBindingPresetMaterialization FDataForgeBindingPresetAuthoring::Materialize(
	UDataForgeRuleSet& RuleSet,
	const UDataForgeBindingPreset& Preset,
	const FString& OutputFolder)
{
	FDataForgeBindingPresetMaterialization Result = Validate(Preset);
	if (!FPackageName::IsValidLongPackageName(OutputFolder))
	{
		DataForgeBindingPresetAuthoring::AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1912"), FString::Printf(TEXT("Generated Output Folder '%s' is not a valid Content Browser folder."), *OutputFolder));
	}
	if (RuleSet.Schema.PrimaryKey.IsNone())
	{
		DataForgeBindingPresetAuthoring::AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1913"), TEXT("Probe and select a Primary Key before applying a Binding Preset."));
	}
	if (DataForgeBindingPresetAuthoring::HasErrors(Result.Diagnostics))
	{
		Result.bSuccess = false;
		return Result;
	}

	const FName ManagedRuleId(*(Preset.OutputName.ToString() + TEXT("_Managed")));
	if (const FDataForgeAssetRule* Existing = RuleSet.AssetRules.FindByPredicate([ManagedRuleId](const FDataForgeAssetRule& Rule) { return Rule.RuleId == ManagedRuleId; });
		Existing && Existing->Ownership != EDataForgeAssetOwnership::Managed)
	{
		DataForgeBindingPresetAuthoring::AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1914"), FString::Printf(TEXT("Asset Rule '%s' already exists and is not Managed."), *ManagedRuleId.ToString()));
		Result.bSuccess = false;
		return Result;
	}
	const FString RowReference = DataForgeBindingPresetAuthoring::ResolveRowReference(RuleSet, Preset, Result.Diagnostics);
	if (DataForgeBindingPresetAuthoring::HasErrors(Result.Diagnostics))
	{
		Result.bSuccess = false;
		return Result;
	}

	const FScopedTransaction Transaction(LOCTEXT("ApplyBindingPreset", "Apply DataForge Binding Preset"));
	RuleSet.Modify();
	FDataForgeAssetRule* ManagedRule = RuleSet.AssetRules.FindByPredicate([ManagedRuleId](const FDataForgeAssetRule& Rule) { return Rule.RuleId == ManagedRuleId; });
	if (!ManagedRule)
	{
		ManagedRule = &RuleSet.AssetRules.AddDefaulted_GetRef();
		ManagedRule->RuleId = ManagedRuleId;
	}
	ManagedRule->Ownership = EDataForgeAssetOwnership::Managed;
	ManagedRule->BaseFolder = OutputFolder;
	ManagedRule->SubfolderPattern.Reset();
	ManagedRule->AssetNamePattern = Preset.AssetNamePrefix + TEXT("_{") + RuleSet.Schema.PrimaryKey.ToString() + TEXT("}");

	FDataForgeGeneratedAssetOutputRule* Output = RuleSet.GeneratedOutputs.FindByPredicate([&Preset](const FDataForgeGeneratedAssetOutputRule& Candidate)
	{
		return Candidate.OutputName == Preset.OutputName;
	});
	if (!Output)
	{
		Output = &RuleSet.GeneratedOutputs.AddDefaulted_GetRef();
		Output->OutputName = Preset.OutputName;
	}
	Output->Type = Preset.TargetClass->IsChildOf(UPrimaryDataAsset::StaticClass())
		? EDataForgeGeneratedAssetType::PrimaryDataAsset
		: EDataForgeGeneratedAssetType::DataAsset;
	Output->AssetClass = Preset.TargetClass;
	Output->AssetRuleId = ManagedRuleId;
	RuleSet.BindingPreset = const_cast<UDataForgeBindingPreset*>(&Preset);
	Result.ManagedRuleId = ManagedRuleId;

	if (!RowReference.IsEmpty())
	{
		const bool bAlreadyBound = RuleSet.Bindings.ContainsByPredicate([&Preset, &RowReference](const FDataForgeBindingRule& Binding)
		{
			return Binding.Source == EDataForgeBindingSource::GeneratedOutput
				&& Binding.SourceOutput == Preset.OutputName
				&& Binding.Target == EDataForgeBindingTarget::DataTableRow
				&& Binding.TargetProperty.Equals(RowReference, ESearchCase::IgnoreCase);
		});
		if (!bAlreadyBound)
		{
			FDataForgeBindingRule& Binding = RuleSet.Bindings.AddDefaulted_GetRef();
			Binding.Source = EDataForgeBindingSource::GeneratedOutput;
			Binding.SourceOutput = Preset.OutputName;
			Binding.Target = EDataForgeBindingTarget::DataTableRow;
			Binding.TargetProperty = RowReference;
		}
	}

#if WITH_EDITORONLY_DATA
	RuleSet.LastStatus = TEXT("Draft");
	RuleSet.LastSummary = Result.MakeSummary() + TEXT(" Association slots will resolve from the RuleSet's Parsed Data association sources during Preview.");
#endif
	if (RuleSet.GetPackage() != GetTransientPackage()) RuleSet.MarkPackageDirty();
	Result.bSuccess = !DataForgeBindingPresetAuthoring::HasErrors(Result.Diagnostics);
	return Result;
}

#undef LOCTEXT_NAMESPACE
