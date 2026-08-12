#include "DataForgePipeline.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeCore.h"
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
			return AssetRule && ResolveAssetValue(*AssetRule, SourceRow, RecordId, Binding.Rule.SourceColumn, OutValue, Diagnostics);
		}
		return true;
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
