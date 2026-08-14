#include "DataForgePipeline.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeCore.h"
#include "DataForgeBindingPreset.h"
#include "DataForgeDependencyGraph.h"
#include "DataForgeRuleSet.h"
#include "Dom/JsonObject.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/StringOutputDevice.h"
#include "Modules/ModuleManager.h"
#include "Serialization/Csv/CsvParser.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StructOnScope.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/UnrealType.h"

namespace DataForgePipeline
{
	void AddDiagnostic(
		TArray<FDataForgeDiagnostic>& Diagnostics,
		EDataForgeSeverity Severity,
		const TCHAR* Code,
		const FString& Message,
		FName RecordId = NAME_None,
		FName Field = NAME_None,
		int32 SourceRow = INDEX_NONE)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Diagnostic.RecordId = RecordId;
		Diagnostic.Field = Field;
		Diagnostic.SourceRow = SourceRow;
	}

	bool HasErrors(const TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		return Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == EDataForgeSeverity::Error;
		});
	}

	bool JsonScalarToString(const TSharedPtr<FJsonValue>& Value, FString& OutValue)
	{
		if (!Value.IsValid() || Value->IsNull())
		{
			OutValue.Reset();
			return true;
		}
		switch (Value->Type)
		{
		case EJson::String:
			OutValue = Value->AsString();
			return true;
		case EJson::Number:
			OutValue = FString::SanitizeFloat(Value->AsNumber(), 0);
			return true;
		case EJson::Boolean:
			OutValue = Value->AsBool() ? TEXT("true") : TEXT("false");
			return true;
		default:
			return false;
		}
	}

	bool AddColumns(
		const TArray<FString>& Headers,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics,
		const TCHAR* SourceLabel)
	{
		TSet<FName> HeaderSet;
		for (FString Header : Headers)
		{
			Header.TrimStartAndEndInline();
			const FName HeaderName(*Header);
			if (HeaderName.IsNone())
			{
				AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1005"), FString::Printf(TEXT("%s contains an empty column name."), SourceLabel));
				continue;
			}
			if (HeaderSet.Contains(HeaderName))
			{
				AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1006"), FString::Printf(TEXT("%s contains duplicate column '%s'."), SourceLabel, *Header));
				continue;
			}
			HeaderSet.Add(HeaderName);
			OutDataSet.Columns.Add(HeaderName);
		}
		return !HasErrors(OutDiagnostics);
	}

	FString ResolveSourceFilename(const FString& ConfiguredPath)
	{
		FString Filename = ConfiguredPath;
		FPaths::NormalizeFilename(Filename);
		if (FPaths::IsRelative(Filename))
		{
			const FString FullProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
			const FString UnrealRelative = FPaths::ConvertRelativePathToFull(Filename);
			Filename = FPaths::IsUnderDirectory(UnrealRelative, FullProjectDir)
				? UnrealRelative
				: FPaths::ConvertRelativePathToFull(FullProjectDir, Filename);
		}
		FPaths::NormalizeFilename(Filename);
		return Filename;
	}

	FString HashSource(const FString& Source)
	{
		FSHA1 Sha;
		Sha.UpdateWithString(*Source, Source.Len());
		Sha.Final();
		uint8 Hash[FSHA1::DigestSize];
		Sha.GetHash(Hash);
		return BytesToHex(Hash, UE_ARRAY_COUNT(Hash));
	}

	bool ResolvePropertyChain(
		UStruct* RootStruct,
		const FString& ConfiguredPath,
		TArray<FProperty*>& OutChain,
		FString& OutError)
	{
		OutChain.Reset();
		FString Path = ConfiguredPath;
		Path.TrimStartAndEndInline();
		if (Path.StartsWith(TEXT("row."), ESearchCase::IgnoreCase))
		{
			Path.RightChopInline(4);
		}

		TArray<FString> Segments;
		Path.ParseIntoArray(Segments, TEXT("."), true);
		if (Segments.IsEmpty())
		{
			OutError = TEXT("Target property path is empty.");
			return false;
		}

		UStruct* OwnerStruct = RootStruct;
		for (int32 Index = 0; Index < Segments.Num(); ++Index)
		{
			FProperty* Property = FindFProperty<FProperty>(OwnerStruct, FName(*Segments[Index]));
			if (!Property)
			{
				OutError = FString::Printf(TEXT("Property '%s' does not exist on '%s'."), *Segments[Index], *OwnerStruct->GetName());
				return false;
			}

			OutChain.Add(Property);
			if (Index + 1 < Segments.Num())
			{
				const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
				if (!StructProperty)
				{
					OutError = FString::Printf(TEXT("Property '%s' is not a struct and cannot contain nested properties."), *Segments[Index]);
					return false;
				}
				OwnerStruct = StructProperty->Struct;
			}
		}

		return true;
	}

	FString ExpandPattern(
		const FString& Pattern,
		const FDataForgeRow& Row,
		TArray<FDataForgeDiagnostic>& Diagnostics,
		FName RecordId,
		FName Field);

	bool BuildAssetObjectPath(
		const FDataForgeAssetRule& AssetRule,
		const FDataForgeRow& Row,
		FName RecordId,
		FName Field,
		FString& OutPackageName,
		FString& OutObjectPath,
		TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		const FString Subfolder = ExpandPattern(AssetRule.SubfolderPattern, Row, Diagnostics, RecordId, Field);
		const FString AssetName = ExpandPattern(AssetRule.AssetNamePattern, Row, Diagnostics, RecordId, Field);
		if (AssetName.IsEmpty())
		{
			AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF2011"), TEXT("Asset rule produced an empty asset name."), RecordId, Field, Row.SourceRow);
			return false;
		}

		OutPackageName = AssetRule.BaseFolder / Subfolder / AssetName;
		while (OutPackageName.ReplaceInline(TEXT("//"), TEXT("/")) > 0)
		{
		}
		if (!FPackageName::IsValidLongPackageName(OutPackageName))
		{
			AddDiagnostic(
				Diagnostics,
				EDataForgeSeverity::Error,
				TEXT("DF2012"),
				FString::Printf(TEXT("Asset rule produced invalid package path '%s'."), *OutPackageName),
				RecordId,
				Field,
				Row.SourceRow);
			return false;
		}

		OutObjectPath = OutPackageName + TEXT(".") + AssetName;
		return true;
	}

	void* ResolvePropertyAddress(void* RootMemory, const TArray<FProperty*>& Chain)
	{
		if (!RootMemory || Chain.IsEmpty())
		{
			return nullptr;
		}
		void* Container = RootMemory;
		for (int32 Index = 0; Index + 1 < Chain.Num(); ++Index)
		{
			if (!Container || !Chain[Index])
			{
				return nullptr;
			}
			Container = Chain[Index]->ContainerPtrToValuePtr<void>(Container);
		}
		return Container && Chain.Last()
			? Chain.Last()->ContainerPtrToValuePtr<void>(Container)
			: nullptr;
	}

	FString ExpandPattern(
		const FString& Pattern,
		const FDataForgeRow& Row,
		TArray<FDataForgeDiagnostic>& Diagnostics,
		FName RecordId,
		FName Field)
	{
		FString Result = Pattern;
		for (const TPair<FName, FString>& Pair : Row.Values)
		{
			Result.ReplaceInline(*FString::Printf(TEXT("{%s}"), *Pair.Key.ToString()), *Pair.Value, ESearchCase::CaseSensitive);
		}

		if (Result.Contains(TEXT("{")) || Result.Contains(TEXT("}")))
		{
			AddDiagnostic(
				Diagnostics,
				EDataForgeSeverity::Error,
				TEXT("DF2010"),
				FString::Printf(TEXT("Pattern '%s' contains an unknown or incomplete token."), *Pattern),
				RecordId,
				Field,
				Row.SourceRow);
			return FString();
		}
		return Result;
	}

	bool ResolveAssetValue(
		const FDataForgeAssetRule& AssetRule,
		const FDataForgeRow& Row,
		FName RecordId,
		FName Field,
		FString& OutValue,
		TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		FString PackageName;
		FString ObjectPathString;
		if (!BuildAssetObjectPath(AssetRule, Row, RecordId, Field, PackageName, ObjectPathString, Diagnostics))
		{
			return false;
		}

		const FSoftObjectPath ObjectPath(ObjectPathString);
		const FAssetData AssetData = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"))
			.Get()
			.GetAssetByObjectPath(ObjectPath);
		if (!AssetData.IsValid())
		{
			AddDiagnostic(
				Diagnostics,
				EDataForgeSeverity::Error,
				TEXT("DF2004"),
				FString::Printf(TEXT("Asset '%s' was not found for rule '%s'."), *ObjectPath.ToString(), *AssetRule.RuleId.ToString()),
				RecordId,
				Field,
				Row.SourceRow);
			return false;
		}

		OutValue = AssetData.GetSoftObjectPath().ToString();
		return true;
	}

	bool ResolveAssetArrayValue(
		const FDataForgeCompiledBinding& Binding,
		const FDataForgeAssetRule& AssetRule,
		const FDataForgeRow& Row,
		FName RecordId,
		const FString& SourceValue,
		FString& OutValue,
		TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Binding.TargetProperty);
		if (!ArrayProperty || !CastField<FSoftObjectProperty>(ArrayProperty->Inner)) return false;

		TArray<FString> AssetIds;
		SourceValue.ParseIntoArray(AssetIds, TEXT(";"), true);
		TArray<FString> ResolvedPaths;
		for (FString AssetId : AssetIds)
		{
			AssetId.TrimStartAndEndInline();
			if (AssetId.IsEmpty()) continue;
			FDataForgeRow ElementRow = Row;
			ElementRow.Values.FindOrAdd(Binding.Rule.SourceColumn) = AssetId;
			FString ResolvedPath;
			if (!ResolveAssetValue(AssetRule, ElementRow, RecordId, Binding.Rule.SourceColumn, ResolvedPath, Diagnostics))
			{
				return false;
			}
			ResolvedPaths.Add(FString::Printf(TEXT("\"%s\""), *ResolvedPath));
		}
		OutValue = TEXT("(") + FString::Join(ResolvedPaths, TEXT(",")) + TEXT(")");
		return true;
	}

	bool ImportBindingValue(
		const FDataForgeCompiledBinding& Binding,
		const FString& Value,
		void* RootMemory,
		UObject* OwnerObject,
		FName RecordId,
		int32 SourceRow,
		TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		void* PropertyAddress = ResolvePropertyAddress(RootMemory, Binding.PropertyChain);
		if (!Binding.TargetProperty || !PropertyAddress)
		{
			AddDiagnostic(
				Diagnostics,
				EDataForgeSeverity::Error,
				TEXT("DF3002"),
				FString::Printf(TEXT("Target property storage is unavailable for '%s'. Recompile the RuleSet after its target schema changes."), *Binding.Rule.TargetProperty),
				RecordId,
				Binding.Rule.SourceColumn,
				SourceRow);
			return false;
		}
		FStringOutputDevice ErrorOutput;
		const TCHAR* Result = Binding.TargetProperty->ImportText_Direct(
			*Value,
			PropertyAddress,
			OwnerObject,
			PPF_None,
			&ErrorOutput);
		if (!Result)
		{
			AddDiagnostic(
				Diagnostics,
				EDataForgeSeverity::Error,
				TEXT("DF3001"),
				FString::Printf(
					TEXT("Value '%s' cannot be assigned to '%s' (%s). %s"),
					*Value,
					*Binding.Rule.TargetProperty,
					*Binding.TargetProperty->GetCPPType(),
					*ErrorOutput),
				RecordId,
				Binding.Rule.SourceColumn,
				SourceRow);
			return false;
		}
		return true;
	}

	bool ExportBindingValue(
		const FDataForgeCompiledBinding& Binding,
		UObject& OwnerObject,
		FString& OutValue)
	{
		const void* PropertyAddress = ResolvePropertyAddress(&OwnerObject, Binding.PropertyChain);
		return Binding.TargetProperty && PropertyAddress && Binding.TargetProperty->ExportText_Direct(
			OutValue,
			PropertyAddress,
			PropertyAddress,
			&OwnerObject,
			PPF_None);
	}

	bool IsBindingIdentical(
		const FDataForgeCompiledBinding& Binding,
		const UObject& A,
		const UObject& B)
	{
		const void* AddressA = ResolvePropertyAddress(const_cast<UObject*>(&A), Binding.PropertyChain);
		const void* AddressB = ResolvePropertyAddress(const_cast<UObject*>(&B), Binding.PropertyChain);
		return Binding.TargetProperty && AddressA && AddressB
			&& Binding.TargetProperty->Identical(AddressA, AddressB, PPF_DeepComparison);
	}

	bool IsOwnedByRuleSet(
		const UObject& Asset,
		const UDataForgeRuleSet& RuleSet,
		FName OutputName,
		FName RecordId)
	{
		FMetaData& MetaData = Asset.GetPackage()->GetMetaData();
		return MetaData.GetValue(&Asset, TEXT("DataForge.Managed")) == TEXT("true")
			&& MetaData.GetValue(&Asset, TEXT("DataForge.RuleSetId")) == RuleSet.RuleSetId.ToString(EGuidFormats::Digits)
			&& MetaData.GetValue(&Asset, TEXT("DataForge.Role")) == OutputName.ToString()
			&& MetaData.GetValue(&Asset, TEXT("DataForge.RecordId")) == RecordId.ToString();
	}

	FString GetOwnershipState(const UObject& Asset)
	{
		FMetaData& MetaData = Asset.GetPackage()->GetMetaData();
		return FString::Printf(
			TEXT("%s|%s|%s|%s|%s"),
			*MetaData.GetValue(&Asset, TEXT("DataForge.Managed")),
			*MetaData.GetValue(&Asset, TEXT("DataForge.RuleSetId")),
			*MetaData.GetValue(&Asset, TEXT("DataForge.RecordId")),
			*MetaData.GetValue(&Asset, TEXT("DataForge.Role")),
			*MetaData.GetValue(&Asset, TEXT("DataForge.RuleVersion")));
	}

	bool ResolveBindingValue(
		const FDataForgeCompiledBinding& Binding,
		const FDataForgeRow& SourceRow,
		const TMap<FName, FString>& GeneratedOutputPaths,
		const TMap<FName, FDataForgeAssetRule>& AssetRules,
		FName RecordId,
		FString& OutValue,
		TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		if (Binding.Rule.Source == EDataForgeBindingSource::GeneratedOutput)
		{
			const FString* GeneratedPath = GeneratedOutputPaths.Find(Binding.Rule.SourceOutput);
			if (!GeneratedPath)
			{
				AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF2020"), FString::Printf(TEXT("Generated output '%s' is unavailable."), *Binding.Rule.SourceOutput.ToString()), RecordId, NAME_None, SourceRow.SourceRow);
				return false;
			}
			OutValue = *GeneratedPath;
			return true;
		}

		const FString* SourceValue = SourceRow.Values.Find(Binding.Rule.SourceColumn);
		if (!SourceValue || (Binding.Rule.bRequired && SourceValue->IsEmpty()))
		{
			if (Binding.Rule.bRequired)
			{
				AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF1205"), TEXT("Required binding source value is empty."), RecordId, Binding.Rule.SourceColumn, SourceRow.SourceRow);
			}
			return false;
		}

		OutValue = SourceValue ? *SourceValue : FString();
		if (Binding.Rule.Source == EDataForgeBindingSource::ResolvedAsset)
		{
			const FDataForgeAssetRule* AssetRule = AssetRules.Find(Binding.Rule.AssetRuleId);
			if (!AssetRule) return false;
			if (CastField<FArrayProperty>(Binding.TargetProperty))
			{
				const FString SourceListValue = OutValue;
				return ResolveAssetArrayValue(Binding, *AssetRule, SourceRow, RecordId, SourceListValue, OutValue, Diagnostics);
			}
			return ResolveAssetValue(*AssetRule, SourceRow, RecordId, Binding.Rule.SourceColumn, OutValue, Diagnostics);
		}
		return true;
	}

	FProperty* AssociationReferenceProperty(FProperty* Property, EDataForgeBindingCardinality Cardinality)
	{
		if (Cardinality == EDataForgeBindingCardinality::Many)
		{
			const FArrayProperty* Array = CastField<FArrayProperty>(Property);
			return Array ? Array->Inner : nullptr;
		}
		return CastField<FArrayProperty>(Property) ? nullptr : Property;
	}

	bool IsAssociationPropertyCompatible(FProperty* Property, EDataForgeBindingCardinality Cardinality, UClass* ExpectedClass)
	{
		const FSoftObjectProperty* SoftObject = CastField<FSoftObjectProperty>(AssociationReferenceProperty(Property, Cardinality));
		return SoftObject && (!ExpectedClass || ExpectedClass->IsChildOf(SoftObject->PropertyClass));
	}

	FString ResolveAssociationPropertyPath(UClass* TargetClass, const FDataForgeBindingPresetSlot& Slot, TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		if (!Slot.TargetProperty.IsEmpty()) return Slot.TargetProperty;
		TArray<FProperty*> Candidates;
		UClass* ExpectedClass = Slot.ExpectedAssetClass.LoadSynchronous();
		for (TFieldIterator<FProperty> It(TargetClass, EFieldIterationFlags::IncludeSuper); It; ++It)
		{
			FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Edit)
				&& !Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
				&& IsAssociationPropertyCompatible(Property, Slot.Cardinality, ExpectedClass))
			{
				Candidates.Add(Property);
			}
		}
		if (Candidates.Num() == 1) return Candidates[0]->GetName();
		AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF1920"), FString::Printf(
			TEXT("Association slot '%s' requires an explicit Target Property because %d compatible properties were found."),
			*Slot.SlotId.ToString(), Candidates.Num()), NAME_None, Slot.SlotId);
		return FString();
	}

	TArray<FString> ReadSoftObjectPaths(const TArray<FProperty*>& PropertyChain, const UObject& Object)
	{
		TArray<FString> Paths;
		const void* Address = ResolvePropertyAddress(const_cast<UObject*>(&Object), PropertyChain);
		FProperty* Property = PropertyChain.IsEmpty() ? nullptr : PropertyChain.Last();
		if (!Address || !Property) return Paths;
		if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
		{
			const FString Path = Soft->GetPropertyValue(Address).ToSoftObjectPath().ToString();
			if (!Path.IsEmpty()) Paths.Add(Path);
		}
		else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			const FSoftObjectProperty* Inner = CastField<FSoftObjectProperty>(Array->Inner);
			if (!Inner) return Paths;
			FScriptArrayHelper Helper(Array, Address);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				const FString Path = Inner->GetPropertyValue(Helper.GetRawPtr(Index)).ToSoftObjectPath().ToString();
				if (!Path.IsEmpty()) Paths.AddUnique(Path);
			}
		}
		return Paths;
	}

	bool WriteSoftObjectPaths(const FDataForgeCompiledAssociationSlot& Slot, const TArray<FString>& Paths, UObject& Target)
	{
		void* Address = ResolvePropertyAddress(&Target, Slot.PropertyChain);
		if (!Address || !Slot.TargetProperty) return false;
		if (FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Slot.TargetProperty))
		{
			Soft->SetPropertyValue(Address, FSoftObjectPtr(Paths.IsEmpty() ? FSoftObjectPath() : FSoftObjectPath(Paths[0])));
			return true;
		}
		FArrayProperty* Array = CastField<FArrayProperty>(Slot.TargetProperty);
		FSoftObjectProperty* Inner = Array ? CastField<FSoftObjectProperty>(Array->Inner) : nullptr;
		if (!Array || !Inner) return false;
		FScriptArrayHelper Helper(Array, Address);
		Helper.EmptyValues();
		for (const FString& Path : Paths)
		{
			const int32 Index = Helper.AddValue();
			Inner->SetPropertyValue(Helper.GetRawPtr(Index), FSoftObjectPtr(FSoftObjectPath(Path)));
		}
		return true;
	}

	bool ResolveAssociationMatches(
		const FCompiledDataForgeRuleSet& Compiled,
		const FDataForgeCompiledAssociationSlot& Slot,
		const FDataForgeRow& SourceRow,
		FName RecordId,
		TArray<FString>& OutPaths,
		TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		const FDataForgeAssociationSourceRule* SourceRule = Compiled.AssociationSourceRules.Find(Slot.AssociationSourceId);
		const FDataForgeDataSet* DataSet = Compiled.AssociationDataSets.Find(Slot.AssociationSourceId);
		const FString* MatchValue = SourceRow.Values.Find(Slot.SourceKeyColumn);
		if (!SourceRule || !DataSet || !MatchValue || MatchValue->IsEmpty())
		{
			AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF1921"), TEXT("Association source or source match value is unavailable."), RecordId, Slot.SlotId, SourceRow.SourceRow);
			return false;
		}
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		for (const FDataForgeRow& Candidate : DataSet->Rows)
		{
			if (Candidate.Values.FindRef(SourceRule->MatchColumn) != *MatchValue) continue;
			if (!Slot.AssetKind.IsNone() && Candidate.Values.FindRef(SourceRule->AssetKindColumn) != Slot.AssetKind.ToString()) continue;
			if (!Slot.Role.IsNone() && Candidate.Values.FindRef(SourceRule->RoleColumn) != Slot.Role.ToString()) continue;
			const FString Path = Candidate.Values.FindRef(SourceRule->AssetPathColumn);
			const FAssetData AssetData = Registry.GetAssetByObjectPath(FSoftObjectPath(Path));
			FProperty* Reference = AssociationReferenceProperty(Slot.TargetProperty, Slot.Cardinality);
			const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Reference);
			UClass* AssetClass = AssetData.IsValid() ? AssetData.GetClass(EResolveClass::Yes) : nullptr;
			if (!AssetData.IsValid() || !Soft || !AssetClass || !AssetClass->IsChildOf(Soft->PropertyClass))
			{
				AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF1922"), FString::Printf(TEXT("Association candidate '%s' is missing or incompatible with slot '%s'."), *Path, *Slot.SlotId.ToString()), RecordId, Slot.SlotId, Candidate.SourceRow);
				continue;
			}
			OutPaths.AddUnique(AssetData.GetSoftObjectPath().ToString());
		}
		OutPaths.Sort();
		const bool bTooMany = Slot.Cardinality != EDataForgeBindingCardinality::Many && OutPaths.Num() > 1;
		const bool bMissingRequired = Slot.bRequired && OutPaths.IsEmpty();
		if (bTooMany || bMissingRequired)
		{
			AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF1923"), FString::Printf(TEXT("Association slot '%s' resolved %d candidates; its cardinality is not satisfied."), *Slot.SlotId.ToString(), OutPaths.Num()), RecordId, Slot.SlotId, SourceRow.SourceRow);
			return false;
		}
		return !HasErrors(Diagnostics);
	}
}

FDataForgeSourceAdapterRegistry& FDataForgeSourceAdapterRegistry::Get()
{
	static FDataForgeSourceAdapterRegistry Registry;
	return Registry;
}

bool FDataForgeSourceAdapterRegistry::Register(const TSharedRef<IDataForgeSourceAdapter>& Adapter)
{
	const FDataForgeSourceDescriptor Descriptor = Adapter->Describe();
	if (Descriptor.AdapterId.IsNone() || Adapters.Contains(Descriptor.AdapterId))
	{
		return false;
	}
	Adapters.Add(Descriptor.AdapterId, Adapter);
	return true;
}

void FDataForgeSourceAdapterRegistry::Unregister(FName AdapterId)
{
	Adapters.Remove(AdapterId);
}

TSharedPtr<const IDataForgeSourceAdapter> FDataForgeSourceAdapterRegistry::Find(FName AdapterId) const
{
	const TSharedPtr<IDataForgeSourceAdapter>* Adapter = Adapters.Find(AdapterId);
	return Adapter ? *Adapter : nullptr;
}

TArray<FDataForgeSourceDescriptor> FDataForgeSourceAdapterRegistry::DescribeAll() const
{
	TArray<FDataForgeSourceDescriptor> Descriptors;
	Descriptors.Reserve(Adapters.Num());
	for (const TPair<FName, TSharedPtr<IDataForgeSourceAdapter>>& Pair : Adapters)
	{
		Descriptors.Add(Pair.Value->Describe());
	}
	Descriptors.Sort([](const FDataForgeSourceDescriptor& Left, const FDataForgeSourceDescriptor& Right)
	{
		return Left.DisplayName.ToString() < Right.DisplayName.ToString();
	});
	return Descriptors;
}

FDataForgeSourceDescriptor FDataForgeCsvSourceAdapter::Describe() const
{
	return { TEXT("Csv"), NSLOCTEXT("DataForge", "CsvAdapter", "CSV"), TEXT("Parse a CSV file into canonical DataForge rows."), TEXT("csv") };
}

bool FDataForgeCsvSourceAdapter::Probe(
	const FDataForgeSourceConfig& Source,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics) const
{
	return Read(Source, FMath::Max(1, Source.ProbeRowLimit), OutDataSet, OutDiagnostics);
}

bool FDataForgeCsvSourceAdapter::Fetch(
	const FDataForgeSourceConfig& Source,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics) const
{
	return Read(Source, INDEX_NONE, OutDataSet, OutDiagnostics);
}

bool FDataForgeCsvSourceAdapter::Read(
	const FDataForgeSourceConfig& Source,
	int32 RowLimit,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics)
{
	const FString Filename = DataForgePipeline::ResolveSourceFilename(Source.File.FilePath);
	if (Filename.IsEmpty() || !IFileManager::Get().FileExists(*Filename))
	{
		DataForgePipeline::AddDiagnostic(
			OutDiagnostics,
			EDataForgeSeverity::Error,
			TEXT("DF1001"),
			FString::Printf(TEXT("CSV source file does not exist: %s"), *Filename));
		return false;
	}

	FString CsvText;
	if (!FFileHelper::LoadFileToString(CsvText, *Filename, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite))
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1002"), FString::Printf(TEXT("Could not read CSV source: %s"), *Filename));
		return false;
	}

	return Parse(CsvText, RowLimit, OutDataSet, OutDiagnostics);
}

bool FDataForgeCsvSourceAdapter::Parse(
	const FString& CsvText,
	int32 RowLimit,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics)
{
	OutDataSet = FDataForgeDataSet();
	if (CsvText.TrimStartAndEnd().IsEmpty())
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1003"), TEXT("CSV source is empty."));
		return false;
	}

	FCsvParser Parser(CsvText);
	const FCsvParser::FRows& CsvRows = Parser.GetRows();
	if (CsvRows.IsEmpty())
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1004"), TEXT("CSV source has no header row."));
		return false;
	}

	TArray<FString> Headers;
	for (const TCHAR* HeaderValue : CsvRows[0])
	{
		Headers.Add(HeaderValue);
	}
	DataForgePipeline::AddColumns(Headers, OutDataSet, OutDiagnostics, TEXT("CSV"));

	const int32 LastRowExclusive = RowLimit == INDEX_NONE
		? CsvRows.Num()
		: FMath::Min(CsvRows.Num(), RowLimit + 1);
	for (int32 CsvRowIndex = 1; CsvRowIndex < LastRowExclusive; ++CsvRowIndex)
	{
		const TArray<const TCHAR*>& CsvRow = CsvRows[CsvRowIndex];
		bool bHasAnyValue = false;
		FDataForgeRow Row;
		Row.SourceRow = CsvRowIndex + 1;
		for (int32 ColumnIndex = 0; ColumnIndex < OutDataSet.Columns.Num(); ++ColumnIndex)
		{
			const FString Value = CsvRow.IsValidIndex(ColumnIndex) ? FString(CsvRow[ColumnIndex]) : FString();
			bHasAnyValue |= !Value.IsEmpty();
			Row.Values.Add(OutDataSet.Columns[ColumnIndex], Value);
		}
		if (CsvRow.Num() > OutDataSet.Columns.Num())
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Warning, TEXT("DF1007"), TEXT("CSV row has more values than the header."), NAME_None, NAME_None, Row.SourceRow);
		}
		if (bHasAnyValue)
		{
			OutDataSet.Rows.Add(MoveTemp(Row));
		}
	}

	OutDataSet.SourceRevision = DataForgePipeline::HashSource(CsvText);
	return !DataForgePipeline::HasErrors(OutDiagnostics);
}

FDataForgeSourceDescriptor FDataForgeJsonSourceAdapter::Describe() const
{
	return { TEXT("Json"), NSLOCTEXT("DataForge", "JsonAdapter", "JSON"), TEXT("Parse a flat object array or normalized headers/rows JSON file."), TEXT("json") };
}

bool FDataForgeJsonSourceAdapter::Probe(
	const FDataForgeSourceConfig& Source,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics) const
{
	return Read(Source, FMath::Max(1, Source.ProbeRowLimit), OutDataSet, OutDiagnostics);
}

bool FDataForgeJsonSourceAdapter::Fetch(
	const FDataForgeSourceConfig& Source,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics) const
{
	return Read(Source, INDEX_NONE, OutDataSet, OutDiagnostics);
}

bool FDataForgeJsonSourceAdapter::Read(
	const FDataForgeSourceConfig& Source,
	int32 RowLimit,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics)
{
	const FString Filename = DataForgePipeline::ResolveSourceFilename(Source.File.FilePath);
	if (Filename.IsEmpty() || !IFileManager::Get().FileExists(*Filename))
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1010"), FString::Printf(TEXT("JSON source file does not exist: %s"), *Filename));
		return false;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Filename, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite))
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1011"), FString::Printf(TEXT("Could not read JSON source: %s"), *Filename));
		return false;
	}
	return Parse(JsonText, RowLimit, OutDataSet, OutDiagnostics);
}

bool FDataForgeJsonSourceAdapter::Parse(
	const FString& JsonText,
	int32 RowLimit,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics)
{
	OutDataSet = FDataForgeDataSet();
	TSharedPtr<FJsonValue> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1012"), TEXT("JSON source is invalid."));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> RowValues;
	if (Root->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> Object = Root->AsObject();
		const TArray<TSharedPtr<FJsonValue>>* HeaderValues = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* NormalizedRows = nullptr;
		if (!Object.IsValid()
			|| !Object->TryGetArrayField(TEXT("headers"), HeaderValues)
			|| !Object->TryGetArrayField(TEXT("rows"), NormalizedRows))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1013"), TEXT("JSON object sources require 'headers' and 'rows' arrays."));
			return false;
		}
		TArray<FString> Headers;
		for (const TSharedPtr<FJsonValue>& HeaderValue : *HeaderValues)
		{
			FString Header;
			if (!HeaderValue.IsValid() || !HeaderValue->TryGetString(Header))
			{
				DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1014"), TEXT("JSON headers must be strings."));
				return false;
			}
			Headers.Add(MoveTemp(Header));
		}
		if (!DataForgePipeline::AddColumns(Headers, OutDataSet, OutDiagnostics, TEXT("JSON")))
		{
			return false;
		}
		RowValues = *NormalizedRows;
	}
	else if (Root->Type == EJson::Array)
	{
		RowValues = Root->AsArray();
		TSet<FString> HeaderSet;
		for (const TSharedPtr<FJsonValue>& RowValue : RowValues)
		{
			const TSharedPtr<FJsonObject> RowObject = RowValue.IsValid() && RowValue->Type == EJson::Object
				? RowValue->AsObject()
				: nullptr;
			if (!RowObject.IsValid())
			{
				DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1015"), TEXT("JSON array sources must contain only objects."));
				return false;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : RowObject->Values)
			{
				HeaderSet.Add(Pair.Key);
			}
		}
		TArray<FString> Headers = HeaderSet.Array();
		Headers.Sort();
		if (!DataForgePipeline::AddColumns(Headers, OutDataSet, OutDiagnostics, TEXT("JSON")))
		{
			return false;
		}
	}
	else
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1016"), TEXT("JSON root must be an object or array."));
		return false;
	}

	const int32 RowCount = RowLimit == INDEX_NONE ? RowValues.Num() : FMath::Min(RowValues.Num(), RowLimit);
	for (int32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
	{
		FDataForgeRow Row;
		Row.SourceRow = RowIndex + 1;
		if (Root->Type == EJson::Object)
		{
			const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
			if (!RowValues[RowIndex].IsValid() || !RowValues[RowIndex]->TryGetArray(Cells))
			{
				DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1017"), TEXT("Normalized JSON rows must be arrays."), NAME_None, NAME_None, Row.SourceRow);
				continue;
			}
			for (int32 ColumnIndex = 0; ColumnIndex < OutDataSet.Columns.Num(); ++ColumnIndex)
			{
				FString Value;
				if (Cells->IsValidIndex(ColumnIndex) && !DataForgePipeline::JsonScalarToString((*Cells)[ColumnIndex], Value))
				{
					DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1018"), TEXT("Nested JSON values are not supported as cells."), NAME_None, OutDataSet.Columns[ColumnIndex], Row.SourceRow);
				}
				Row.Values.Add(OutDataSet.Columns[ColumnIndex], MoveTemp(Value));
			}
		}
		else
		{
			const TSharedPtr<FJsonObject> RowObject = RowValues[RowIndex]->AsObject();
			for (const FName Column : OutDataSet.Columns)
			{
				FString Value;
				const TSharedPtr<FJsonValue>* JsonValue = RowObject->Values.Find(Column.ToString());
				if (JsonValue && !DataForgePipeline::JsonScalarToString(*JsonValue, Value))
				{
					DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1018"), TEXT("Nested JSON values are not supported as cells."), NAME_None, Column, Row.SourceRow);
				}
				Row.Values.Add(Column, MoveTemp(Value));
			}
		}
		OutDataSet.Rows.Add(MoveTemp(Row));
	}

	OutDataSet.SourceRevision = DataForgePipeline::HashSource(JsonText);
	return !DataForgePipeline::HasErrors(OutDiagnostics);
}

bool FDataForgeCompiler::Probe(
	const UDataForgeRuleSet& RuleSet,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics)
{
	const TSharedPtr<const IDataForgeSourceAdapter> Adapter = FDataForgeSourceAdapterRegistry::Get().Find(RuleSet.Source.AdapterId);
	if (!Adapter)
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1000"), FString::Printf(TEXT("Source adapter '%s' is not registered."), *RuleSet.Source.AdapterId.ToString()));
		return false;
	}
	return Adapter->Probe(RuleSet.Source, OutDataSet, OutDiagnostics);
}

bool FDataForgeCompiler::Compile(
	const UDataForgeRuleSet& RuleSet,
	FCompiledDataForgeRuleSet& OutCompiled,
	TArray<FDataForgeDiagnostic>& OutDiagnostics)
{
	OutCompiled = FCompiledDataForgeRuleSet();
	OutCompiled.RuleSet = &RuleSet;
	TArray<const UDataForgeRuleSet*> DependencyRoots = { &RuleSet };
	TArray<const UDataForgeRuleSet*> DependencyOrder;
	if (!FDataForgeDependencyGraph::BuildExecutionOrder(DependencyRoots, DependencyOrder, OutDiagnostics))
	{
		return false;
	}

	if (!RuleSet.RuleSetId.IsValid())
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1100"), TEXT("RuleSetId is invalid. Recreate the RuleSet asset or assign a valid id."));
	}
	if (RuleSet.Schema.PrimaryKey.IsNone())
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1101"), TEXT("Primary key must be selected explicitly."));
	}
	if (!RuleSet.Output.RowStruct)
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1102"), TEXT("DataTable row struct is not configured."));
	}
	else if (!RuleSet.Output.RowStruct->IsChildOf(FTableRowBase::StaticStruct()))
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1103"), TEXT("Configured row struct must derive from FTableRowBase."));
	}
	else
	{
		OutCompiled.RowStruct = RuleSet.Output.RowStruct;
	}

	if (!FPackageName::IsValidLongPackageName(RuleSet.Output.AssetPath))
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1104"), FString::Printf(TEXT("Output path is not a valid long package name: %s"), *RuleSet.Output.AssetPath));
	}

	const TSharedPtr<const IDataForgeSourceAdapter> SourceAdapter = FDataForgeSourceAdapterRegistry::Get().Find(RuleSet.Source.AdapterId);
	if (!SourceAdapter)
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1000"), FString::Printf(TEXT("Source adapter '%s' is not registered."), *RuleSet.Source.AdapterId.ToString()));
		return false;
	}
	if (!SourceAdapter->Fetch(RuleSet.Source, OutCompiled.DataSet, OutDiagnostics))
	{
		return false;
	}

	const TSet<FName> SourceColumns(OutCompiled.DataSet.Columns);
	if (!SourceColumns.Contains(RuleSet.Schema.PrimaryKey))
	{
		DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1105"), FString::Printf(TEXT("Primary key column '%s' was not found."), *RuleSet.Schema.PrimaryKey.ToString()), NAME_None, RuleSet.Schema.PrimaryKey);
	}
	for (const FName RequiredColumn : RuleSet.Schema.RequiredColumns)
	{
		if (!SourceColumns.Contains(RequiredColumn))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1106"), FString::Printf(TEXT("Required column '%s' was not found."), *RequiredColumn.ToString()), NAME_None, RequiredColumn);
		}
	}

	for (const FDataForgeAssociationSourceRule& AssociationSource : RuleSet.AssociationSources)
	{
		if (AssociationSource.SourceId.IsNone() || OutCompiled.AssociationSourceRules.Contains(AssociationSource.SourceId))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1916"), TEXT("Association Source Ids must be non-empty and unique."));
			continue;
		}
		const TSharedPtr<const IDataForgeSourceAdapter> Adapter = FDataForgeSourceAdapterRegistry::Get().Find(AssociationSource.Source.AdapterId);
		FDataForgeDataSet AssociationData;
		if (!Adapter || !Adapter->Fetch(AssociationSource.Source, AssociationData, OutDiagnostics))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1917"), FString::Printf(TEXT("Association Source '%s' could not be fetched."), *AssociationSource.SourceId.ToString()));
			continue;
		}
		const TSet<FName> AssociationColumns(AssociationData.Columns);
		const TArray<FName> RequiredAssociationColumns = {
			AssociationSource.MatchColumn, AssociationSource.AssetPathColumn,
			AssociationSource.AssetKindColumn, AssociationSource.RoleColumn };
		if (RequiredAssociationColumns.Contains(NAME_None)
			|| RequiredAssociationColumns.ContainsByPredicate([&AssociationColumns](FName Column) { return !AssociationColumns.Contains(Column); }))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1918"), FString::Printf(TEXT("Association Source '%s' is missing one or more configured columns."), *AssociationSource.SourceId.ToString()));
			continue;
		}
		OutCompiled.AssociationSourceRules.Add(AssociationSource.SourceId, AssociationSource);
		OutCompiled.AssociationDataSets.Add(AssociationSource.SourceId, MoveTemp(AssociationData));
	}

	for (const FDataForgeAssetRule& AssetRule : RuleSet.AssetRules)
	{
		if (AssetRule.RuleId.IsNone())
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1107"), TEXT("Asset rule id cannot be empty."));
			continue;
		}
		if (OutCompiled.AssetRules.Contains(AssetRule.RuleId))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1108"), FString::Printf(TEXT("Asset rule id '%s' is duplicated."), *AssetRule.RuleId.ToString()));
			continue;
		}
		if (!FPackageName::IsValidLongPackageName(AssetRule.BaseFolder))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1109"), FString::Printf(TEXT("Asset rule '%s' has invalid base folder '%s'."), *AssetRule.RuleId.ToString(), *AssetRule.BaseFolder));
		}
		OutCompiled.AssetRules.Add(AssetRule.RuleId, AssetRule);
	}

	for (const FDataForgeGeneratedAssetOutputRule& OutputRule : RuleSet.GeneratedOutputs)
	{
		if (OutputRule.OutputName.IsNone())
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1114"), TEXT("Generated output name cannot be empty."));
			continue;
		}
		if (OutCompiled.GeneratedOutputs.Contains(OutputRule.OutputName))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1115"), FString::Printf(TEXT("Generated output '%s' is duplicated."), *OutputRule.OutputName.ToString()));
			continue;
		}

		UClass* AssetClass = OutputRule.AssetClass.Get();
		if (!AssetClass || !AssetClass->IsChildOf(UDataAsset::StaticClass()) || AssetClass->HasAnyClassFlags(CLASS_Abstract))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1116"), FString::Printf(TEXT("Generated output '%s' requires a concrete UDataAsset class."), *OutputRule.OutputName.ToString()));
			continue;
		}
		if (OutputRule.Type == EDataForgeGeneratedAssetType::PrimaryDataAsset && !AssetClass->IsChildOf(UPrimaryDataAsset::StaticClass()))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1117"), FString::Printf(TEXT("Generated output '%s' is PrimaryDataAsset but its class does not derive from UPrimaryDataAsset."), *OutputRule.OutputName.ToString()));
			continue;
		}

		const FDataForgeAssetRule* AssetRule = OutCompiled.AssetRules.Find(OutputRule.AssetRuleId);
		if (!AssetRule || AssetRule->Ownership != EDataForgeAssetOwnership::Managed)
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1118"), FString::Printf(TEXT("Generated output '%s' must reference a Managed Asset Rule."), *OutputRule.OutputName.ToString()));
			continue;
		}

		FDataForgeCompiledOutput CompiledOutput;
		CompiledOutput.Rule = OutputRule;
		CompiledOutput.AssetClass = AssetClass;
		OutCompiled.GeneratedOutputs.Add(OutputRule.OutputName, MoveTemp(CompiledOutput));
	}

	TSet<FName> MappedColumns;
	TSet<FString> BoundTargets;
	for (const FDataForgeBindingRule& BindingRule : RuleSet.Bindings)
	{
		if (BindingRule.Source == EDataForgeBindingSource::GeneratedOutput)
		{
			if (!OutCompiled.GeneratedOutputs.Contains(BindingRule.SourceOutput))
			{
				DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1119"), FString::Printf(TEXT("Binding references unknown generated output '%s'."), *BindingRule.SourceOutput.ToString()));
			}
			if (BindingRule.Target != EDataForgeBindingTarget::DataTableRow)
			{
				DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1120"), TEXT("GeneratedOutput sources currently bind only to DataTable row properties."));
			}
		}
		else
		{
			MappedColumns.Add(BindingRule.SourceColumn);
			if (!SourceColumns.Contains(BindingRule.SourceColumn))
			{
				DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1110"), FString::Printf(TEXT("Binding source column '%s' was not found."), *BindingRule.SourceColumn.ToString()), NAME_None, BindingRule.SourceColumn);
			}
			if (BindingRule.Source == EDataForgeBindingSource::ResolvedAsset && !OutCompiled.AssetRules.Contains(BindingRule.AssetRuleId))
			{
				DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1111"), FString::Printf(TEXT("Binding references unknown asset rule '%s'."), *BindingRule.AssetRuleId.ToString()), NAME_None, BindingRule.SourceColumn);
			}
		}

		UStruct* TargetStruct = nullptr;
		FString TargetPath = BindingRule.TargetProperty;
		if (BindingRule.Target == EDataForgeBindingTarget::DataTableRow)
		{
			TargetStruct = RuleSet.Output.RowStruct;
		}
		else if (const FDataForgeCompiledOutput* Output = OutCompiled.GeneratedOutputs.Find(BindingRule.TargetOutput))
		{
			TargetStruct = Output->AssetClass.Get();
			const FString Prefix = BindingRule.TargetOutput.ToString() + TEXT(".");
			if (TargetPath.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				TargetPath.RightChopInline(Prefix.Len());
			}
		}
		else
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1121"), FString::Printf(TEXT("Binding target output '%s' does not exist."), *BindingRule.TargetOutput.ToString()));
		}

		if (!TargetStruct)
		{
			continue;
		}
		const FString TargetKey = FString::Printf(TEXT("%d:%s:%s"), static_cast<int32>(BindingRule.Target), *BindingRule.TargetOutput.ToString(), *TargetPath);
		if (BoundTargets.Contains(TargetKey))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1122"), FString::Printf(TEXT("Target property '%s' is bound more than once."), *BindingRule.TargetProperty));
			continue;
		}
		BoundTargets.Add(TargetKey);

		FDataForgeCompiledBinding CompiledBinding;
		CompiledBinding.Rule = BindingRule;
		CompiledBinding.Rule.TargetProperty = TargetPath;
		FString PropertyError;
		if (DataForgePipeline::ResolvePropertyChain(TargetStruct, TargetPath, CompiledBinding.PropertyChain, PropertyError))
		{
			CompiledBinding.TargetProperty = CompiledBinding.PropertyChain.Last();
			if (BindingRule.Source == EDataForgeBindingSource::GeneratedOutput && !CastField<FSoftObjectProperty>(CompiledBinding.TargetProperty))
			{
				DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1123"), TEXT("Generated output references require a soft object property so Preview does not load or create assets."));
			}
			OutCompiled.Bindings.Add(MoveTemp(CompiledBinding));
		}
		else
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1112"), PropertyError, NAME_None, BindingRule.SourceColumn);
		}
	}

	if (UDataForgeBindingPreset* Preset = RuleSet.BindingPreset.LoadSynchronous())
	{
		const FDataForgeCompiledOutput* PresetOutput = OutCompiled.GeneratedOutputs.Find(Preset->OutputName);
		UClass* TargetClass = Preset->TargetClass.Get();
		if (!PresetOutput || !TargetClass || PresetOutput->AssetClass.Get() != TargetClass)
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1919"), TEXT("Binding Preset output is missing or its generated class no longer matches."));
		}
		else
		{
			TSet<FName> AssociationSlotIds;
			for (const FDataForgeBindingPresetSlot& Slot : Preset->Slots)
			{
				if (Slot.SlotId.IsNone() || AssociationSlotIds.Contains(Slot.SlotId))
				{
					DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1905"), TEXT("Binding Preset Slot Ids must be non-empty and unique."), NAME_None, Slot.SlotId);
					continue;
				}
				AssociationSlotIds.Add(Slot.SlotId);
				if ((Slot.Cardinality == EDataForgeBindingCardinality::Many && Slot.Reconcile == EDataForgeBindingReconcileMode::Assign)
					|| (Slot.Cardinality != EDataForgeBindingCardinality::Many
						&& Slot.Reconcile != EDataForgeBindingReconcileMode::Assign
						&& Slot.Reconcile != EDataForgeBindingReconcileMode::Manual))
				{
					DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1909"), FString::Printf(TEXT("Association slot '%s' has incompatible cardinality and reconciliation modes."), *Slot.SlotId.ToString()), NAME_None, Slot.SlotId);
					continue;
				}
				if (Slot.Reconcile == EDataForgeBindingReconcileMode::Manual) continue;
				FName AssociationSourceId = Slot.AssociationSourceId;
				if (AssociationSourceId.IsNone())
				{
					if (OutCompiled.AssociationSourceRules.IsEmpty()) continue;
					if (OutCompiled.AssociationSourceRules.Num() == 1) AssociationSourceId = OutCompiled.AssociationSourceRules.CreateConstIterator().Key();
					else
					{
						DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1925"), FString::Printf(TEXT("Association slot '%s' must select a source because the RuleSet has multiple Association Sources."), *Slot.SlotId.ToString()), NAME_None, Slot.SlotId);
						continue;
					}
				}
				const FDataForgeAssociationSourceRule* SourceRule = OutCompiled.AssociationSourceRules.Find(AssociationSourceId);
				const FName SourceKeyColumn = Slot.SourceKeyColumn.IsNone() ? RuleSet.Schema.PrimaryKey : Slot.SourceKeyColumn;
				if (!SourceRule || !SourceColumns.Contains(SourceKeyColumn))
				{
					DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1921"), FString::Printf(TEXT("Association slot '%s' references an unavailable source or key column."), *Slot.SlotId.ToString()), NAME_None, Slot.SlotId);
					continue;
				}
				FDataForgeCompiledAssociationSlot CompiledSlot;
				CompiledSlot.OutputName = Preset->OutputName;
				CompiledSlot.SlotId = Slot.SlotId;
				CompiledSlot.AssociationSourceId = AssociationSourceId;
				CompiledSlot.SourceKeyColumn = SourceKeyColumn;
				CompiledSlot.AssetKind = Slot.AssetKind;
				CompiledSlot.Role = Slot.Role;
				CompiledSlot.Cardinality = Slot.Cardinality;
				CompiledSlot.Reconcile = Slot.Reconcile;
				CompiledSlot.bRequired = Slot.bRequired;
				CompiledSlot.TargetPropertyPath = DataForgePipeline::ResolveAssociationPropertyPath(TargetClass, Slot, OutDiagnostics);
				FString PropertyError;
				if (CompiledSlot.TargetPropertyPath.IsEmpty()
					|| !DataForgePipeline::ResolvePropertyChain(TargetClass, CompiledSlot.TargetPropertyPath, CompiledSlot.PropertyChain, PropertyError))
				{
					if (!CompiledSlot.TargetPropertyPath.IsEmpty()) DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1906"), PropertyError, NAME_None, Slot.SlotId);
					continue;
				}
				CompiledSlot.TargetProperty = CompiledSlot.PropertyChain.Last();
				if (!DataForgePipeline::IsAssociationPropertyCompatible(CompiledSlot.TargetProperty, Slot.Cardinality, Slot.ExpectedAssetClass.LoadSynchronous()))
				{
					DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1906"), FString::Printf(TEXT("Association slot '%s' target property is incompatible."), *Slot.SlotId.ToString()), NAME_None, Slot.SlotId);
					continue;
				}
				const FString TargetKey = FString::Printf(TEXT("%d:%s:%s"), static_cast<int32>(EDataForgeBindingTarget::GeneratedOutput), *Preset->OutputName.ToString(), *CompiledSlot.TargetPropertyPath);
				if (BoundTargets.Contains(TargetKey))
				{
					DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1122"), FString::Printf(TEXT("Association target property '%s' is bound more than once."), *CompiledSlot.TargetPropertyPath));
					continue;
				}
				BoundTargets.Add(TargetKey);
				OutCompiled.AssociationSlots.Add(MoveTemp(CompiledSlot));
			}
		}
	}

	if (RuleSet.Schema.bWarnOnUnmappedColumns)
	{
		for (const FName Column : OutCompiled.DataSet.Columns)
		{
			if (Column != RuleSet.Schema.PrimaryKey && !MappedColumns.Contains(Column))
			{
				DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Warning, TEXT("DF1113"), FString::Printf(TEXT("Source column '%s' is not mapped."), *Column.ToString()), NAME_None, Column);
			}
		}
	}
	if (!OutCompiled.AssociationDataSets.IsEmpty())
	{
		TArray<FName> SourceIds;
		OutCompiled.AssociationDataSets.GetKeys(SourceIds);
		SourceIds.Sort(FNameLexicalLess());
		FString CombinedRevision = OutCompiled.DataSet.SourceRevision;
		for (const FName SourceId : SourceIds)
		{
			CombinedRevision += TEXT("|") + SourceId.ToString() + TEXT(":") + OutCompiled.AssociationDataSets.FindChecked(SourceId).SourceRevision;
		}
		OutCompiled.DataSet.SourceRevision = DataForgePipeline::HashSource(CombinedRevision);
	}

	return !DataForgePipeline::HasErrors(OutDiagnostics);
}

FDataForgeApplyPlan FDataForgeCompiler::BuildPlan(const FCompiledDataForgeRuleSet& Compiled)
{
	FDataForgeApplyPlan Plan;
	Plan.RuleSet = Compiled.RuleSet;
	Plan.SourceRevision = Compiled.DataSet.SourceRevision;

	const UDataForgeRuleSet* RuleSet = Compiled.RuleSet.Get();
	UScriptStruct* RowStruct = Compiled.RowStruct.Get();
	if (!RuleSet || !RowStruct)
	{
		DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1200"), TEXT("Compiled RuleSet is stale or invalid."));
		return Plan;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(RuleSet->Output.AssetPath);
	const FString ObjectPath = RuleSet->Output.AssetPath + TEXT(".") + AssetName;
	UDataTable* ExistingTable = LoadObject<UDataTable>(nullptr, *ObjectPath);
	Plan.ExistingTable = ExistingTable;
	FString TargetState = TEXT("<missing-table>");
	if (!ExistingTable && !RuleSet->Output.bCreateIfMissing)
	{
		DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1201"), FString::Printf(TEXT("Output DataTable does not exist: %s"), *ObjectPath));
		Plan.TargetRevision = DataForgePipeline::HashSource(TargetState);
		return Plan;
	}
	if (ExistingTable && ExistingTable->GetRowStruct() != RowStruct)
	{
		DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1202"), TEXT("Existing DataTable row struct does not match the RuleSet output."));
		Plan.TargetRevision = DataForgePipeline::HashSource(TEXT("<row-struct-mismatch>") + ObjectPath);
		return Plan;
	}
	if (ExistingTable)
	{
		TargetState = ExistingTable->GetTableAsJSON();
	}

	TSet<FName> DesiredRowNames;
	TSet<FString> DesiredManagedObjectPaths;
	TSet<FString> PlannedManagedObjectPaths;
	TSet<FString> ConsumedManagedObjectPaths;
	TArray<FName> GeneratedOutputNames;
	Compiled.GeneratedOutputs.GetKeys(GeneratedOutputNames);
	GeneratedOutputNames.Sort([](const FName A, const FName B)
	{
		return A.ToString() < B.ToString();
	});

	struct FOwnedManagedAsset
	{
		FName RecordId = NAME_None;
		FName OutputName = NAME_None;
		FString PackageName;
		FString ObjectPath;
		TWeakObjectPtr<UDataAsset> Asset;
	};
	TArray<FOwnedManagedAsset> OwnedManagedAssets;
	TMap<FString, int32> OwnedAssetByIdentity;
	TSet<FString> DuplicateOwnedIdentities;
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> DataAssets;
	AssetRegistry.GetAssetsByClass(UDataAsset::StaticClass()->GetClassPathName(), DataAssets, true);
	DataAssets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString();
	});
	for (const FAssetData& AssetData : DataAssets)
	{
		UDataAsset* Asset = Cast<UDataAsset>(AssetData.GetAsset());
		if (!Asset)
		{
			continue;
		}
		FMetaData& MetaData = Asset->GetPackage()->GetMetaData();
		if (MetaData.GetValue(Asset, TEXT("DataForge.Managed")) != TEXT("true")
			|| MetaData.GetValue(Asset, TEXT("DataForge.RuleSetId")) != RuleSet->RuleSetId.ToString(EGuidFormats::Digits))
		{
			continue;
		}

		FOwnedManagedAsset& Owned = OwnedManagedAssets.AddDefaulted_GetRef();
		Owned.RecordId = FName(*MetaData.GetValue(Asset, TEXT("DataForge.RecordId")));
		Owned.OutputName = FName(*MetaData.GetValue(Asset, TEXT("DataForge.Role")));
		Owned.PackageName = AssetData.PackageName.ToString();
		Owned.ObjectPath = AssetData.GetSoftObjectPath().ToString();
		Owned.Asset = Asset;
		const FString Identity = Owned.OutputName.ToString() + TEXT("|") + Owned.RecordId.ToString();
		if (Owned.OutputName.IsNone() || Owned.RecordId.IsNone())
		{
			DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF1217"), FString::Printf(TEXT("Managed asset has incomplete ownership identity and can only be treated as an orphan: %s"), *Owned.ObjectPath));
			continue;
		}
		if (OwnedAssetByIdentity.Contains(Identity))
		{
			DuplicateOwnedIdentities.Add(Identity);
			DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1216"), FString::Printf(TEXT("Multiple managed assets share ownership identity '%s': %s"), *Identity, *Owned.ObjectPath));
			continue;
		}
		OwnedAssetByIdentity.Add(Identity, OwnedManagedAssets.Num() - 1);
	}

	for (const FDataForgeRow& SourceRow : Compiled.DataSet.Rows)
	{
		const FString* PrimaryKeyValue = SourceRow.Values.Find(RuleSet->Schema.PrimaryKey);
		FString PrimaryKey = PrimaryKeyValue ? *PrimaryKeyValue : FString();
		PrimaryKey.TrimStartAndEndInline();
		const FName RowName(*PrimaryKey);
		if (RowName.IsNone())
		{
			DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1203"), TEXT("Primary key is empty."), NAME_None, RuleSet->Schema.PrimaryKey, SourceRow.SourceRow);
			continue;
		}
		if (DesiredRowNames.Contains(RowName))
		{
			DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1204"), TEXT("Primary key is duplicated."), RowName, RuleSet->Schema.PrimaryKey, SourceRow.SourceRow);
			continue;
		}
		DesiredRowNames.Add(RowName);

		TMap<FName, FString> GeneratedOutputPaths;
		for (const FName OutputName : GeneratedOutputNames)
		{
			const FDataForgeCompiledOutput& Output = Compiled.GeneratedOutputs.FindChecked(OutputName);
			const FDataForgeAssetRule* AssetRule = Compiled.AssetRules.Find(Output.Rule.AssetRuleId);
			UClass* AssetClass = Output.AssetClass.Get();
			if (!AssetRule || !AssetClass)
			{
				DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1210"), TEXT("Compiled generated output is stale."), RowName, NAME_None, SourceRow.SourceRow);
				continue;
			}

			FString PackageName;
			FString GeneratedObjectPath;
			if (!DataForgePipeline::BuildAssetObjectPath(*AssetRule, SourceRow, RowName, OutputName, PackageName, GeneratedObjectPath, Plan.Diagnostics))
			{
				continue;
			}
			if (PlannedManagedObjectPaths.Contains(GeneratedObjectPath))
			{
				DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1211"), FString::Printf(TEXT("Generated asset path collision: %s"), *GeneratedObjectPath), RowName, OutputName, SourceRow.SourceRow);
				continue;
			}
			PlannedManagedObjectPaths.Add(GeneratedObjectPath);
			DesiredManagedObjectPaths.Add(GeneratedObjectPath);
			GeneratedOutputPaths.Add(OutputName, GeneratedObjectPath);

			FDataForgePlannedAsset PlannedAsset;
			PlannedAsset.RecordId = RowName;
			PlannedAsset.OutputName = OutputName;
			PlannedAsset.PackageName = PackageName;
			PlannedAsset.ObjectPath = GeneratedObjectPath;
			PlannedAsset.AssetClass = AssetClass;

			UObject* ExistingObject = LoadObject<UObject>(nullptr, *GeneratedObjectPath);
			UDataAsset* ExistingAsset = Cast<UDataAsset>(ExistingObject);
			if (ExistingObject && (!ExistingAsset || !ExistingObject->IsA(AssetClass)))
			{
				DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1212"), FString::Printf(TEXT("Generated path collides with an asset of another class: %s"), *GeneratedObjectPath), RowName, OutputName, SourceRow.SourceRow);
				continue;
			}
			if (ExistingAsset && !DataForgePipeline::IsOwnedByRuleSet(*ExistingAsset, *RuleSet, OutputName, RowName))
			{
				DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1213"), FString::Printf(TEXT("Refusing to overwrite asset without matching DataForge ownership: %s"), *GeneratedObjectPath), RowName, OutputName, SourceRow.SourceRow);
				continue;
			}
			const FString Identity = OutputName.ToString() + TEXT("|") + RowName.ToString();
			if (!ExistingAsset && !DuplicateOwnedIdentities.Contains(Identity))
			{
				if (const int32* OwnedIndex = OwnedAssetByIdentity.Find(Identity))
				{
					ExistingAsset = OwnedManagedAssets[*OwnedIndex].Asset.Get();
					if (ExistingAsset && !ExistingAsset->IsA(AssetClass))
					{
						DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1218"), FString::Printf(TEXT("Managed asset '%s' cannot move to '%s' because its class no longer matches output '%s'."), *OwnedManagedAssets[*OwnedIndex].ObjectPath, *GeneratedObjectPath, *OutputName.ToString()), RowName, OutputName, SourceRow.SourceRow);
						continue;
					}
				}
			}
			PlannedAsset.ExistingAsset = ExistingAsset;
			if (ExistingAsset)
			{
				PlannedAsset.PreviousPackageName = ExistingAsset->GetPackage()->GetName();
				PlannedAsset.PreviousObjectPath = ExistingAsset->GetPathName();
				ConsumedManagedObjectPaths.Add(PlannedAsset.PreviousObjectPath);
			}

			UDataAsset* DesiredAsset = NewObject<UDataAsset>(GetTransientPackage(), AssetClass, NAME_None, RF_Transient);
			bool bPropertiesIdentical = ExistingAsset != nullptr;
			for (const FDataForgeCompiledBinding& Binding : Compiled.Bindings)
			{
				if (Binding.Rule.Target != EDataForgeBindingTarget::GeneratedOutput || Binding.Rule.TargetOutput != OutputName)
				{
					continue;
				}

				FString BindingValue;
				if (!DataForgePipeline::ResolveBindingValue(Binding, SourceRow, GeneratedOutputPaths, Compiled.AssetRules, RowName, BindingValue, Plan.Diagnostics))
				{
					continue;
				}
				if (!DataForgePipeline::ImportBindingValue(Binding, BindingValue, DesiredAsset, DesiredAsset, RowName, SourceRow.SourceRow, Plan.Diagnostics))
				{
					continue;
				}

				FDataForgePlannedPropertyWrite& PropertyWrite = PlannedAsset.PropertyWrites.AddDefaulted_GetRef();
				PropertyWrite.PropertyPath = Binding.Rule.TargetProperty;
				if (!DataForgePipeline::ExportBindingValue(Binding, *DesiredAsset, PropertyWrite.ExportedValue))
				{
					DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1214"), FString::Printf(TEXT("Could not export desired property '%s'."), *Binding.Rule.TargetProperty), RowName, Binding.Rule.SourceColumn, SourceRow.SourceRow);
				}
				if (ExistingAsset)
				{
					bPropertiesIdentical &= DataForgePipeline::IsBindingIdentical(Binding, *ExistingAsset, *DesiredAsset);
					DataForgePipeline::ExportBindingValue(Binding, *ExistingAsset, PropertyWrite.PreviousValue);
					TargetState += FString::Printf(TEXT("|%s:%s=%s"), *GeneratedObjectPath, *Binding.Rule.TargetProperty, *PropertyWrite.PreviousValue);
				}
			}

			for (const FDataForgeCompiledAssociationSlot& Slot : Compiled.AssociationSlots)
			{
				if (Slot.OutputName != OutputName) continue;
				TArray<FString> MatchedPaths;
				if (!DataForgePipeline::ResolveAssociationMatches(Compiled, Slot, SourceRow, RowName, MatchedPaths, Plan.Diagnostics)) continue;

				TArray<FString> DesiredPaths = MatchedPaths;
				const FString ManifestKey = TEXT("DataForge.Association.") + Slot.TargetPropertyPath;
				if (Slot.Cardinality == EDataForgeBindingCardinality::Many
					&& Slot.Reconcile == EDataForgeBindingReconcileMode::MergeByKey
					&& ExistingAsset)
				{
					DesiredPaths = DataForgePipeline::ReadSoftObjectPaths(Slot.PropertyChain, *ExistingAsset);
					TArray<FString> PreviouslyManaged;
					ExistingAsset->GetPackage()->GetMetaData().GetValue(ExistingAsset, *ManifestKey).ParseIntoArrayLines(PreviouslyManaged, true);
					DesiredPaths.RemoveAll([&PreviouslyManaged](const FString& Path) { return PreviouslyManaged.Contains(Path); });
					for (const FString& Path : MatchedPaths) DesiredPaths.AddUnique(Path);
					DesiredPaths.Sort();
				}

				if (!DataForgePipeline::WriteSoftObjectPaths(Slot, DesiredPaths, *DesiredAsset))
				{
					DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1924"), FString::Printf(TEXT("Association slot '%s' could not write property '%s'."), *Slot.SlotId.ToString(), *Slot.TargetPropertyPath), RowName, Slot.SlotId, SourceRow.SourceRow);
					continue;
				}
				PlannedAsset.ManagedAssociations.Add(Slot.TargetPropertyPath, MatchedPaths);
				FDataForgePlannedPropertyWrite& PropertyWrite = PlannedAsset.PropertyWrites.AddDefaulted_GetRef();
				PropertyWrite.PropertyPath = Slot.TargetPropertyPath;
				void* DesiredAddress = DataForgePipeline::ResolvePropertyAddress(DesiredAsset, Slot.PropertyChain);
				if (!DesiredAddress || !Slot.TargetProperty->ExportText_Direct(PropertyWrite.ExportedValue, DesiredAddress, DesiredAddress, DesiredAsset, PPF_None))
				{
					DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1924"), FString::Printf(TEXT("Association slot '%s' could not export its desired value."), *Slot.SlotId.ToString()), RowName, Slot.SlotId, SourceRow.SourceRow);
					continue;
				}
				if (ExistingAsset)
				{
					TArray<FString> CurrentManaged;
					ExistingAsset->GetPackage()->GetMetaData().GetValue(ExistingAsset, *ManifestKey).ParseIntoArrayLines(CurrentManaged, true);
					CurrentManaged.Sort();
					bPropertiesIdentical &= CurrentManaged == MatchedPaths;
					void* ExistingAddress = DataForgePipeline::ResolvePropertyAddress(ExistingAsset, Slot.PropertyChain);
					bPropertiesIdentical &= ExistingAddress && Slot.TargetProperty->Identical(ExistingAddress, DesiredAddress, PPF_DeepComparison);
					if (ExistingAddress) Slot.TargetProperty->ExportText_Direct(PropertyWrite.PreviousValue, ExistingAddress, ExistingAddress, ExistingAsset, PPF_None);
					TargetState += FString::Printf(TEXT("|%s:%s=%s|%s=%s"), *GeneratedObjectPath, *Slot.TargetPropertyPath, *PropertyWrite.PreviousValue, *ManifestKey, *ExistingAsset->GetPackage()->GetMetaData().GetValue(ExistingAsset, *ManifestKey));
				}
			}
			if (ExistingAsset)
			{
				TArray<FString> PreviousKeys;
				ExistingAsset->GetPackage()->GetMetaData().GetValue(ExistingAsset, TEXT("DataForge.Association.Keys")).ParseIntoArrayLines(PreviousKeys, true);
				for (const FString& PreviousKey : PreviousKeys)
				{
					if (!PlannedAsset.ManagedAssociations.Contains(PreviousKey)) PlannedAsset.RemovedAssociationKeys.AddUnique(PreviousKey);
				}
				PlannedAsset.RemovedAssociationKeys.Sort();
				bPropertiesIdentical &= PlannedAsset.RemovedAssociationKeys.IsEmpty();
			}

			if (!ExistingAsset)
			{
				PlannedAsset.Change = EDataForgeManagedAssetChange::Create;
				++Plan.AssetCreateCount;
				TargetState += TEXT("|") + GeneratedObjectPath + TEXT(":<missing>");
			}
			else if (PlannedAsset.PreviousObjectPath != PlannedAsset.ObjectPath)
			{
				PlannedAsset.Change = EDataForgeManagedAssetChange::Move;
				++Plan.AssetMoveCount;
				TargetState += TEXT("|") + PlannedAsset.PreviousObjectPath + TEXT(":") + DataForgePipeline::GetOwnershipState(*ExistingAsset);
				TargetState += TEXT("|") + GeneratedObjectPath + TEXT(":<move-destination>");
			}
			else
			{
				TargetState += TEXT("|") + GeneratedObjectPath + TEXT(":") + DataForgePipeline::GetOwnershipState(*ExistingAsset);
				const FString Version = ExistingAsset->GetPackage()->GetMetaData().GetValue(ExistingAsset, TEXT("DataForge.RuleVersion"));
				if (bPropertiesIdentical && Version == FString::FromInt(RuleSet->RuleVersion))
				{
					PlannedAsset.Change = EDataForgeManagedAssetChange::Unchanged;
					++Plan.AssetUnchangedCount;
				}
				else
				{
					PlannedAsset.Change = EDataForgeManagedAssetChange::Update;
					++Plan.AssetUpdateCount;
				}
			}
			Plan.ManagedAssets.Add(MoveTemp(PlannedAsset));
		}

		FDataForgePlannedRow PlannedRow;
		PlannedRow.RowName = RowName;
		PlannedRow.DesiredData = MakeShared<FStructOnScope>(RowStruct);
		for (const FDataForgeCompiledBinding& Binding : Compiled.Bindings)
		{
			if (Binding.Rule.Target != EDataForgeBindingTarget::DataTableRow)
			{
				continue;
			}

			FString BindingValue;
			if (!DataForgePipeline::ResolveBindingValue(Binding, SourceRow, GeneratedOutputPaths, Compiled.AssetRules, RowName, BindingValue, Plan.Diagnostics))
			{
				continue;
			}

			DataForgePipeline::ImportBindingValue(
				Binding,
				BindingValue,
				PlannedRow.DesiredData->GetStructMemory(),
				nullptr,
				RowName,
				SourceRow.SourceRow,
				Plan.Diagnostics);
		}

		const uint8* ExistingRow = ExistingTable ? ExistingTable->FindRowUnchecked(RowName) : nullptr;
		if (!ExistingRow)
		{
			PlannedRow.Change = EDataForgeRowChange::Create;
			++Plan.CreateCount;
		}
		else if (RowStruct->CompareScriptStruct(ExistingRow, PlannedRow.DesiredData->GetStructMemory(), PPF_DeepComparison))
		{
			PlannedRow.Change = EDataForgeRowChange::Unchanged;
			++Plan.UnchangedCount;
		}
		else
		{
			PlannedRow.Change = EDataForgeRowChange::Update;
			++Plan.UpdateCount;
		}
		Plan.Rows.Add(MoveTemp(PlannedRow));
	}

	for (const FOwnedManagedAsset& Owned : OwnedManagedAssets)
	{
		if (ConsumedManagedObjectPaths.Contains(Owned.ObjectPath)
			|| DesiredManagedObjectPaths.Contains(Owned.ObjectPath))
		{
			continue;
		}
		UDataAsset* Asset = Owned.Asset.Get();
		if (!Asset)
		{
			continue;
		}

		FDataForgePlannedAsset& Orphan = Plan.ManagedAssets.AddDefaulted_GetRef();
		Orphan.RecordId = Owned.RecordId;
		Orphan.OutputName = Owned.OutputName;
		Orphan.PreviousPackageName = Owned.PackageName;
		Orphan.PreviousObjectPath = Owned.ObjectPath;
		Orphan.PackageName = Owned.PackageName;
		Orphan.ObjectPath = Owned.ObjectPath;
		Orphan.AssetClass = Asset->GetClass();
		Orphan.ExistingAsset = Asset;
		Orphan.Change = EDataForgeManagedAssetChange::Orphan;
		++Plan.AssetOrphanCount;
		TargetState += TEXT("|") + Owned.ObjectPath + TEXT(":orphan:") + DataForgePipeline::GetOwnershipState(*Asset);
	}

	if (Plan.AssetOrphanCount > 0)
	{
		DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF1215"), TEXT("Managed assets outside the desired state are preserved by Apply. Use the explicit Cleanup Orphans action to delete them."));
	}

	if (ExistingTable)
	{
		for (const TPair<FName, uint8*>& ExistingPair : ExistingTable->GetRowMap())
		{
			if (!DesiredRowNames.Contains(ExistingPair.Key))
			{
				FDataForgePlannedRow& Orphan = Plan.Rows.AddDefaulted_GetRef();
				Orphan.RowName = ExistingPair.Key;
				Orphan.Change = EDataForgeRowChange::Orphan;
				++Plan.OrphanCount;
			}
		}
	}

	if (Plan.OrphanCount > 0 && !RuleSet->Output.bRemoveRowsMissingFromSource)
	{
		DataForgePipeline::AddDiagnostic(Plan.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF1206"), TEXT("Rows missing from the source are preserved. Enable explicit orphan removal to delete them during Apply."));
	}

	Plan.Rows.Sort([](const FDataForgePlannedRow& A, const FDataForgePlannedRow& B)
	{
		return A.RowName.ToString() < B.RowName.ToString();
	});
	Plan.ManagedAssets.Sort([](const FDataForgePlannedAsset& A, const FDataForgePlannedAsset& B)
	{
		return A.ObjectPath < B.ObjectPath;
	});
	Plan.TargetRevision = DataForgePipeline::HashSource(TargetState);

	return Plan;
}

bool FDataForgeCompiler::IsManagedAssetOwnedBy(
	const UObject& Asset,
	const UDataForgeRuleSet& RuleSet,
	FName OutputName,
	FName RecordId)
{
	return DataForgePipeline::IsOwnedByRuleSet(Asset, RuleSet, OutputName, RecordId);
}

bool FDataForgeCompiler::ApplyPlannedProperties(
	UObject& Target,
	const FDataForgePlannedAsset& PlannedAsset,
	TArray<FDataForgeDiagnostic>& OutDiagnostics)
{
	for (const FDataForgePlannedPropertyWrite& PropertyWrite : PlannedAsset.PropertyWrites)
	{
		TArray<FProperty*> PropertyChain;
		FString PropertyError;
		if (!DataForgePipeline::ResolvePropertyChain(Target.GetClass(), PropertyWrite.PropertyPath, PropertyChain, PropertyError))
		{
			DataForgePipeline::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF3010"), PropertyError, PlannedAsset.RecordId, FName(*PropertyWrite.PropertyPath));
			return false;
		}

		FProperty* TargetProperty = PropertyChain.Last();
		void* PropertyAddress = DataForgePipeline::ResolvePropertyAddress(&Target, PropertyChain);
		if (!TargetProperty || !PropertyAddress)
		{
			DataForgePipeline::AddDiagnostic(
				OutDiagnostics,
				EDataForgeSeverity::Error,
				TEXT("DF3012"),
				FString::Printf(TEXT("Target property storage is unavailable for '%s'. Rebuild the plan after its asset schema changes."), *PropertyWrite.PropertyPath),
				PlannedAsset.RecordId,
				FName(*PropertyWrite.PropertyPath));
			return false;
		}
		FStringOutputDevice ErrorOutput;
		if (!TargetProperty->ImportText_Direct(*PropertyWrite.ExportedValue, PropertyAddress, &Target, PPF_None, &ErrorOutput))
		{
			DataForgePipeline::AddDiagnostic(
				OutDiagnostics,
				EDataForgeSeverity::Error,
				TEXT("DF3011"),
				FString::Printf(TEXT("Could not apply '%s' to '%s'. %s"), *PropertyWrite.ExportedValue, *PropertyWrite.PropertyPath, *ErrorOutput),
				PlannedAsset.RecordId,
				FName(*PropertyWrite.PropertyPath));
			return false;
		}
	}
	return true;
}
