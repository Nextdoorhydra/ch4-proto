#include "DataForgeEditorService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DataForgeDependencyGraph.h"
#include "DataForgeCore.h"
#include "DataForgePipeline.h"
#include "DataForgeRuleSet.h"
#include "DataTableEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Logging/MessageLog.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "DataForgeEditorService"

namespace DataForgeEditorService
{
	struct FPreviewEntry
	{
		FString RuleSignature;
		FDataForgeApplyPlan Plan;
	};

	TMap<TWeakObjectPtr<UDataForgeRuleSet>, FPreviewEntry> PreviewCache;
	TMap<TWeakObjectPtr<UDataForgeRuleSet>, FDataForgeDataSet> ProbeCache;

	enum class ESourceSampleType : uint8
	{
		Unknown,
		Boolean,
		Integer,
		Number,
		String
	};

	bool IsBooleanLiteral(const FString& Value)
	{
		return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Value.Equals(TEXT("false"), ESearchCase::IgnoreCase);
	}

	bool IsNumberLiteral(const FString& Value, bool& bOutInteger)
	{
		bOutInteger = true;
		int32 Index = 0;
		if (Value.IsValidIndex(Index) && (Value[Index] == TEXT('+') || Value[Index] == TEXT('-')))
		{
			++Index;
		}
		bool bSawDigit = false;
		bool bSawExponent = false;
		bool bSawExponentDigit = false;
		bool bSawDecimal = false;
		for (; Index < Value.Len(); ++Index)
		{
			const TCHAR Character = Value[Index];
			if (FChar::IsDigit(Character))
			{
				bSawDigit = true;
				bSawExponentDigit |= bSawExponent;
				continue;
			}
			if (Character == TEXT('.') && !bSawDecimal && !bSawExponent)
			{
				bSawDecimal = true;
				bOutInteger = false;
				continue;
			}
			if ((Character == TEXT('e') || Character == TEXT('E')) && bSawDigit && !bSawExponent)
			{
				bSawExponent = true;
				bOutInteger = false;
				if (Value.IsValidIndex(Index + 1) && (Value[Index + 1] == TEXT('+') || Value[Index + 1] == TEXT('-')))
				{
					++Index;
				}
				continue;
			}
			return false;
		}
		return bSawDigit && (!bSawExponent || bSawExponentDigit);
	}

	ESourceSampleType InferSourceType(const FDataForgeDataSet& DataSet, FName Column)
	{
		bool bSawValue = false;
		bool bAllBoolean = true;
		bool bAllInteger = true;
		bool bAllNumber = true;
		for (const FDataForgeRow& Row : DataSet.Rows)
		{
			const FString* SourceValue = Row.Values.Find(Column);
			if (!SourceValue || SourceValue->TrimStartAndEnd().IsEmpty())
			{
				continue;
			}
			bSawValue = true;
			const FString Value = SourceValue->TrimStartAndEnd();
			bAllBoolean &= IsBooleanLiteral(Value);
			bool bIntegerLiteral = false;
			const bool bNumberLiteral = IsNumberLiteral(Value, bIntegerLiteral);
			bAllInteger &= bNumberLiteral && bIntegerLiteral;
			bAllNumber &= bNumberLiteral;
		}

		if (!bSawValue) return ESourceSampleType::Unknown;
		if (bAllBoolean) return ESourceSampleType::Boolean;
		if (bAllInteger) return ESourceSampleType::Integer;
		if (bAllNumber) return ESourceSampleType::Number;
		return ESourceSampleType::String;
	}

	FString SourceTypeName(ESourceSampleType Type)
	{
		switch (Type)
		{
		case ESourceSampleType::Boolean: return TEXT("Boolean");
		case ESourceSampleType::Integer: return TEXT("Integer");
		case ESourceSampleType::Number: return TEXT("Number");
		case ESourceSampleType::String: return TEXT("String");
		default: return TEXT("Unknown");
		}
	}

	FProperty* ResolveTargetProperty(const UDataForgeRuleSet& RuleSet, const FDataForgeBindingRule& Binding)
	{
		UStruct* TargetStruct = nullptr;
		FString Path = Binding.TargetProperty;
		if (Binding.Target == EDataForgeBindingTarget::DataTableRow)
		{
			TargetStruct = RuleSet.Output.RowStruct;
			if (Path.StartsWith(TEXT("row."), ESearchCase::IgnoreCase))
			{
				Path.RightChopInline(4);
			}
		}
		else if (const FDataForgeGeneratedAssetOutputRule* Output = RuleSet.GeneratedOutputs.FindByPredicate([&Binding](const FDataForgeGeneratedAssetOutputRule& Candidate)
		{
			return Candidate.OutputName == Binding.TargetOutput;
		}))
		{
			TargetStruct = Output->AssetClass.Get();
			const FString Prefix = Binding.TargetOutput.ToString() + TEXT(".");
			if (Path.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				Path.RightChopInline(Prefix.Len());
			}
		}

		TArray<FString> Segments;
		Path.ParseIntoArray(Segments, TEXT("."), true);
		FProperty* Property = nullptr;
		for (int32 Index = 0; TargetStruct && Index < Segments.Num(); ++Index)
		{
			Property = FindFProperty<FProperty>(TargetStruct, FName(*Segments[Index]));
			if (!Property)
			{
				return nullptr;
			}
			if (Index + 1 < Segments.Num())
			{
				const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
				TargetStruct = StructProperty ? StructProperty->Struct : nullptr;
			}
		}
		return Property;
	}

	bool ValuesMatchEnum(const FDataForgeDataSet& DataSet, FName Column, const UEnum& Enum)
	{
		for (const FDataForgeRow& Row : DataSet.Rows)
		{
			const FString Value = Row.Values.FindRef(Column).TrimStartAndEnd();
			if (Value.IsEmpty())
			{
				continue;
			}
			if (Enum.GetValueByNameString(Value, EGetByNameFlags::CheckAuthoredName) == INDEX_NONE)
			{
				return false;
			}
		}
		return true;
	}

	FDataForgeBindingSuggestion MakeSuggestion(
		EDataForgeBindingCompatibility Compatibility,
		const FString& SourceType,
		const FString& TargetType,
		const FString& Message)
	{
		FDataForgeBindingSuggestion Suggestion;
		Suggestion.Compatibility = Compatibility;
		Suggestion.SourceType = SourceType;
		Suggestion.TargetType = TargetType;
		Suggestion.Message = Message;
		return Suggestion;
	}

	bool HasErrors(const TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		return Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == EDataForgeSeverity::Error;
		});
	}

	FString MakeRuleSignature(const UDataForgeRuleSet& RuleSet)
	{
		FString Signature = FString::Printf(
			TEXT("%s|%d|%s|%s|%s|%d|%s|%s|%s|%d|%d|%d"),
			*RuleSet.RuleSetId.ToString(EGuidFormats::Digits),
			RuleSet.RuleVersion,
			*RuleSet.Source.AdapterId.ToString(),
			*RuleSet.Source.File.FilePath,
			*RuleSet.Source.SourceAsset.ToSoftObjectPath().ToString(),
			RuleSet.Source.ProbeRowLimit,
			*RuleSet.Schema.PrimaryKey.ToString(),
			RuleSet.Output.RowStruct ? *RuleSet.Output.RowStruct->GetPathName() : TEXT("None"),
			*RuleSet.Output.AssetPath,
			RuleSet.Output.bCreateIfMissing,
			RuleSet.Output.bRemoveRowsMissingFromSource,
			RuleSet.Output.bSaveAfterApply);
		Signature += FString::Printf(TEXT("|WarnUnmapped:%d"), RuleSet.Schema.bWarnOnUnmappedColumns);
		TArray<FName> SourceParameterKeys;
		RuleSet.Source.Parameters.GetKeys(SourceParameterKeys);
		SourceParameterKeys.Sort(FNameLexicalLess());
		for (const FName Key : SourceParameterKeys)
		{
			Signature += FString::Printf(TEXT("|SP:%s:%s"), *Key.ToString(), *RuleSet.Source.Parameters.FindChecked(Key));
		}
		for (int32 InputIndex = 0; InputIndex < RuleSet.Source.Inputs.Num(); ++InputIndex)
		{
			const FDataForgeSourceInput& Input = RuleSet.Source.Inputs[InputIndex];
			Signature += FString::Printf(TEXT("|SI:%d:%s:%s:%s:%s:%s"), InputIndex, *Input.AdapterId.ToString(),
				*Input.File.FilePath, *Input.SourceAsset.ToSoftObjectPath().ToString(), *Input.JoinColumn.ToString(), *Input.ColumnPrefix);
			TArray<FName> InputParameterKeys;
			Input.Parameters.GetKeys(InputParameterKeys);
			InputParameterKeys.Sort(FNameLexicalLess());
			for (const FName Key : InputParameterKeys)
			{
				Signature += FString::Printf(TEXT("|SIP:%d:%s:%s"), InputIndex, *Key.ToString(), *Input.Parameters.FindChecked(Key));
			}
		}

		for (const FName RequiredColumn : RuleSet.Schema.RequiredColumns)
		{
			Signature += TEXT("|RC:") + RequiredColumn.ToString();
		}
		for (const FDataForgeAssetRule& AssetRule : RuleSet.AssetRules)
		{
			Signature += FString::Printf(
				TEXT("|AR:%s:%d:%s:%s:%s"),
				*AssetRule.RuleId.ToString(),
				static_cast<int32>(AssetRule.Ownership),
				*AssetRule.BaseFolder,
				*AssetRule.SubfolderPattern,
				*AssetRule.AssetNamePattern);
		}
		for (const FDataForgeGeneratedAssetOutputRule& Output : RuleSet.GeneratedOutputs)
		{
			Signature += FString::Printf(
				TEXT("|GO:%s:%d:%s:%s"),
				*Output.OutputName.ToString(),
				static_cast<int32>(Output.Type),
				Output.AssetClass ? *Output.AssetClass->GetPathName() : TEXT("None"),
				*Output.AssetRuleId.ToString());
		}
		for (const FDataForgeBindingRule& Binding : RuleSet.Bindings)
		{
			Signature += FString::Printf(
				TEXT("|B:%d:%s:%s:%d:%s:%s:%s:%d"),
				static_cast<int32>(Binding.Source),
				*Binding.SourceColumn.ToString(),
				*Binding.SourceOutput.ToString(),
				static_cast<int32>(Binding.Target),
				*Binding.TargetOutput.ToString(),
				*Binding.TargetProperty,
				*Binding.AssetRuleId.ToString(),
				Binding.bRequired);
		}
		TArray<FString> DependencyPaths;
		for (const FDataForgeDependencyRule& Dependency : RuleSet.Dependencies)
		{
			DependencyPaths.Add(Dependency.RuleSet.ToSoftObjectPath().ToString());
		}
		DependencyPaths.Sort();
		for (const FString& DependencyPath : DependencyPaths)
		{
			Signature += TEXT("|D:") + DependencyPath;
		}
		return Signature;
	}

	void UpdateRuleSetStatus(
		UDataForgeRuleSet& RuleSet,
		const FString& Status,
		const FString& Summary,
		const TArray<FDataForgeDiagnostic>& Diagnostics,
		const TArray<FName>* Columns = nullptr)
	{
#if WITH_EDITORONLY_DATA
		RuleSet.LastStatus = Status;
		RuleSet.LastSummary = Summary;
		RuleSet.LastDiagnostics = Diagnostics;
		if (Columns)
		{
			RuleSet.LastDetectedColumns = *Columns;
		}
#endif
	}

	bool WriteRecoveryManifest(const UDataForgeRuleSet& RuleSet, const FDataForgeApplyPlan& Plan, FString& OutFilename)
	{
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DataForge"), TEXT("Recovery"));
		if (!IFileManager::Get().MakeDirectory(*Directory, true))
		{
			return false;
		}

		const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
		OutFilename = FPaths::Combine(Directory, FString::Printf(TEXT("%s_%s.json"), *RuleSet.RuleSetId.ToString(EGuidFormats::Digits), *Timestamp));

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("ruleSet"), RuleSet.GetPathName());
		Root->SetStringField(TEXT("ruleSetId"), RuleSet.RuleSetId.ToString());
		Root->SetNumberField(TEXT("ruleVersion"), RuleSet.RuleVersion);
		Root->SetStringField(TEXT("sourceRevision"), Plan.SourceRevision);
		Root->SetStringField(TEXT("targetRevision"), Plan.TargetRevision);
		Root->SetStringField(TEXT("target"), RuleSet.Output.AssetPath);
		Root->SetStringField(TEXT("createdUtc"), FDateTime::UtcNow().ToIso8601());

		TArray<TSharedPtr<FJsonValue>> Operations;
		for (const FDataForgePlannedRow& Row : Plan.Rows)
		{
			TSharedRef<FJsonObject> Operation = MakeShared<FJsonObject>();
			Operation->SetStringField(TEXT("row"), Row.RowName.ToString());
			Operation->SetStringField(TEXT("change"), StaticEnum<EDataForgeRowChange>()->GetNameStringByValue(static_cast<int64>(Row.Change)));
			Operations.Add(MakeShared<FJsonValueObject>(Operation));
		}
		Root->SetArrayField(TEXT("operations"), Operations);

		TArray<TSharedPtr<FJsonValue>> ManagedAssetOperations;
		for (const FDataForgePlannedAsset& Asset : Plan.ManagedAssets)
		{
			TSharedRef<FJsonObject> Operation = MakeShared<FJsonObject>();
			Operation->SetStringField(TEXT("record"), Asset.RecordId.ToString());
			Operation->SetStringField(TEXT("output"), Asset.OutputName.ToString());
			Operation->SetStringField(TEXT("previousPackageName"), Asset.PreviousPackageName);
			Operation->SetStringField(TEXT("previousObjectPath"), Asset.PreviousObjectPath);
			Operation->SetStringField(TEXT("packageName"), Asset.PackageName);
			Operation->SetStringField(TEXT("objectPath"), Asset.ObjectPath);
			Operation->SetStringField(TEXT("change"), StaticEnum<EDataForgeManagedAssetChange>()->GetNameStringByValue(static_cast<int64>(Asset.Change)));
			TArray<TSharedPtr<FJsonValue>> PropertyWrites;
			for (const FDataForgePlannedPropertyWrite& Write : Asset.PropertyWrites)
			{
				TSharedRef<FJsonObject> Property = MakeShared<FJsonObject>();
				Property->SetStringField(TEXT("property"), Write.PropertyPath);
				Property->SetStringField(TEXT("previous"), Write.PreviousValue);
				Property->SetStringField(TEXT("desired"), Write.ExportedValue);
				PropertyWrites.Add(MakeShared<FJsonValueObject>(Property));
			}
			Operation->SetArrayField(TEXT("propertyWrites"), PropertyWrites);
			ManagedAssetOperations.Add(MakeShared<FJsonValueObject>(Operation));
		}
		Root->SetArrayField(TEXT("managedAssets"), ManagedAssetOperations);
		if (const UDataTable* ExistingTable = Plan.ExistingTable.Get())
		{
			Root->SetStringField(TEXT("previousTableJson"), ExistingTable->GetTableAsJSON());
		}

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		return FJsonSerializer::Serialize(Root, Writer) && FFileHelper::SaveStringToFile(Json, *OutFilename);
	}

	FDataForgeResult MakeResult(bool bSuccess, const TArray<FDataForgeDiagnostic>& Diagnostics, const FString& Summary)
	{
		FDataForgeResult Result;
		Result.bSuccess = bSuccess;
		Result.Diagnostics = Diagnostics;
		Result.Summary = Summary;
		return Result;
	}
}

FDataForgeResult FDataForgeEditorService::Probe(UDataForgeRuleSet& RuleSet, FDataForgeDataSet* OutDataSet)
{
	FDataForgeDataSet DataSet;
	TArray<FDataForgeDiagnostic> Diagnostics;
	const bool bSuccess = FDataForgeCompiler::Probe(RuleSet, DataSet, Diagnostics);
	const FString Summary = bSuccess
		? FString::Printf(TEXT("Detected %d columns and sampled %d rows. Source revision: %s"), DataSet.Columns.Num(), DataSet.Rows.Num(), *DataSet.SourceRevision.Left(12))
		: TEXT("Source probe failed.");
	DataForgeEditorService::UpdateRuleSetStatus(RuleSet, bSuccess ? TEXT("Probed") : TEXT("Invalid"), Summary, Diagnostics, &DataSet.Columns);
	const FDataForgeResult Result = DataForgeEditorService::MakeResult(bSuccess, Diagnostics, Summary);
	if (bSuccess)
	{
		DataForgeEditorService::ProbeCache.Add(&RuleSet, DataSet);
	}
	else
	{
		DataForgeEditorService::ProbeCache.Remove(&RuleSet);
	}
	if (OutDataSet)
	{
		*OutDataSet = MoveTemp(DataSet);
	}
	LogResult(RuleSet, Result, !bSuccess);
	return Result;
}

FDataForgeResult FDataForgeEditorService::Preview(UDataForgeRuleSet& RuleSet, FDataForgeApplyPlan* OutPlan)
{
	FCompiledDataForgeRuleSet Compiled;
	TArray<FDataForgeDiagnostic> Diagnostics;
	if (!FDataForgeCompiler::Compile(RuleSet, Compiled, Diagnostics))
	{
		DataForgeEditorService::PreviewCache.Remove(&RuleSet);
		DataForgeEditorService::UpdateRuleSetStatus(RuleSet, TEXT("Invalid"), TEXT("RuleSet compile failed. Content was not modified."), Diagnostics);
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, Diagnostics, TEXT("RuleSet compile failed. Content was not modified."));
		LogResult(RuleSet, Result, true);
		return Result;
	}

	FDataForgeApplyPlan Plan = FDataForgeCompiler::BuildPlan(Compiled);
	Plan.Diagnostics.Insert(Diagnostics, 0);
	const bool bSuccess = !Plan.HasErrors();
	const FString Summary = Plan.MakeSummary();
	DataForgeEditorService::UpdateRuleSetStatus(RuleSet, bSuccess ? TEXT("Previewed") : TEXT("Invalid"), Summary, Plan.Diagnostics, &Compiled.DataSet.Columns);
	if (OutPlan)
	{
		*OutPlan = Plan;
	}

	if (bSuccess)
	{
		DataForgeEditorService::FPreviewEntry& Entry = DataForgeEditorService::PreviewCache.FindOrAdd(&RuleSet);
		Entry.RuleSignature = DataForgeEditorService::MakeRuleSignature(RuleSet);
		Entry.Plan = MoveTemp(Plan);
	}
	else
	{
		DataForgeEditorService::PreviewCache.Remove(&RuleSet);
	}

	const TArray<FDataForgeDiagnostic>& ResultDiagnostics = bSuccess
		? DataForgeEditorService::PreviewCache.FindChecked(&RuleSet).Plan.Diagnostics
		: Plan.Diagnostics;
	const FDataForgeResult Result = DataForgeEditorService::MakeResult(bSuccess, ResultDiagnostics, Summary);
	LogResult(RuleSet, Result, true);
	return Result;
}

FDataForgeResult FDataForgeEditorService::PreviewDependencyGraph(UDataForgeRuleSet& RootRuleSet, FDataForgeApplyPlan* OutRootPlan)
{
	const TArray<const UDataForgeRuleSet*> Roots = { &RootRuleSet };
	TArray<const UDataForgeRuleSet*> ExecutionOrder;
	TArray<FDataForgeDiagnostic> Diagnostics;
	if (!FDataForgeDependencyGraph::BuildExecutionOrder(Roots, ExecutionOrder, Diagnostics))
	{
		const FString Summary = TEXT("Dependency graph validation failed. Content was not modified.");
		DataForgeEditorService::UpdateRuleSetStatus(RootRuleSet, TEXT("Invalid"), Summary, Diagnostics);
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, Diagnostics, Summary);
		LogResult(RootRuleSet, Result, true);
		return Result;
	}

	FDataForgeApplyPlan RootPlan;
	for (const UDataForgeRuleSet* ConstRuleSet : ExecutionOrder)
	{
		UDataForgeRuleSet* RuleSet = const_cast<UDataForgeRuleSet*>(ConstRuleSet);
		FDataForgeApplyPlan Plan;
		const FDataForgeResult Result = Preview(*RuleSet, &Plan);
		Diagnostics.Append(Result.Diagnostics);
		if (!Result.bSuccess)
		{
			const FString Summary = FString::Printf(TEXT("Dependency graph Preview failed at %s."), *RuleSet->GetPathName());
			DataForgeEditorService::UpdateRuleSetStatus(RootRuleSet, TEXT("Invalid"), Summary, Diagnostics);
			return DataForgeEditorService::MakeResult(false, Diagnostics, Summary);
		}
		if (RuleSet == &RootRuleSet)
		{
			RootPlan = MoveTemp(Plan);
		}
	}

	if (OutRootPlan)
	{
		*OutRootPlan = RootPlan;
	}
	const FString Summary = FString::Printf(TEXT("Dependency graph Preview succeeded for %d RuleSet(s). Root: %s"), ExecutionOrder.Num(), *RootPlan.MakeSummary());
	DataForgeEditorService::UpdateRuleSetStatus(RootRuleSet, TEXT("Previewed Graph"), Summary, Diagnostics);
	return DataForgeEditorService::MakeResult(true, Diagnostics, Summary);
}

FDataForgeResult FDataForgeEditorService::Apply(UDataForgeRuleSet& RuleSet)
{
	const DataForgeEditorService::FPreviewEntry* CachedPreview = DataForgeEditorService::PreviewCache.Find(&RuleSet);
	if (!CachedPreview)
	{
		const FString Summary = TEXT("Apply blocked: run a successful Preview first.");
		TArray<FDataForgeDiagnostic> Diagnostics;
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF4001");
		Diagnostic.Message = Summary;
		DataForgeEditorService::UpdateRuleSetStatus(RuleSet, TEXT("Preview Required"), Summary, Diagnostics);
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, Diagnostics, Summary);
		LogResult(RuleSet, Result, true);
		return Result;
	}

	FCompiledDataForgeRuleSet CurrentCompiled;
	TArray<FDataForgeDiagnostic> CurrentDiagnostics;
	const bool bCurrentCompileSucceeded = FDataForgeCompiler::Compile(RuleSet, CurrentCompiled, CurrentDiagnostics);
	FDataForgeApplyPlan CurrentPlan;
	if (bCurrentCompileSucceeded)
	{
		CurrentPlan = FDataForgeCompiler::BuildPlan(CurrentCompiled);
		CurrentPlan.Diagnostics.Insert(CurrentDiagnostics, 0);
		CurrentDiagnostics = CurrentPlan.Diagnostics;
	}
	if (!bCurrentCompileSucceeded
		|| CurrentPlan.HasErrors()
		|| CachedPreview->RuleSignature != DataForgeEditorService::MakeRuleSignature(RuleSet)
		|| CachedPreview->Plan.SourceRevision != CurrentCompiled.DataSet.SourceRevision
		|| CachedPreview->Plan.TargetRevision != CurrentPlan.TargetRevision)
	{
		DataForgeEditorService::PreviewCache.Remove(&RuleSet);
		const FString Summary = TEXT("Apply blocked: the RuleSet, source, DataTable, or managed assets changed after Preview. Preview again.");
		FDataForgeDiagnostic& Diagnostic = CurrentDiagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF4002");
		Diagnostic.Message = Summary;
		DataForgeEditorService::UpdateRuleSetStatus(RuleSet, TEXT("Out of Date"), Summary, CurrentDiagnostics);
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, CurrentDiagnostics, Summary);
		LogResult(RuleSet, Result, true);
		return Result;
	}

	const FDataForgeApplyPlan& Plan = CurrentPlan;
	FString RecoveryFilename;
	if (!DataForgeEditorService::WriteRecoveryManifest(RuleSet, Plan, RecoveryFilename))
	{
		const FString Summary = TEXT("Apply blocked: recovery manifest could not be written.");
		TArray<FDataForgeDiagnostic> Diagnostics = Plan.Diagnostics;
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF4003");
		Diagnostic.Message = Summary;
		DataForgeEditorService::UpdateRuleSetStatus(RuleSet, TEXT("Apply Failed"), Summary, Diagnostics);
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, Diagnostics, Summary);
		LogResult(RuleSet, Result, true);
		return Result;
	}

	FScopedSlowTask SlowTask(3.0f, LOCTEXT("ApplyingDataForge", "Applying DataForge plan..."));
	SlowTask.MakeDialog();
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("PreparingManagedAssets", "Creating and binding managed assets"));
	const FScopedTransaction Transaction(LOCTEXT("ApplyTransaction", "Apply DataForge RuleSet"));
	TArray<UDataAsset*> AssetsToSave;
	TArray<FDataForgeDiagnostic> Diagnostics = Plan.Diagnostics;
	for (const FDataForgePlannedAsset& PlannedAsset : Plan.ManagedAssets)
	{
		if (PlannedAsset.Change == EDataForgeManagedAssetChange::Orphan)
		{
			continue;
		}
		if (PlannedAsset.Change == EDataForgeManagedAssetChange::Unchanged)
		{
			continue;
		}

		UDataAsset* Asset = PlannedAsset.ExistingAsset.Get();
		if (PlannedAsset.Change == EDataForgeManagedAssetChange::Move)
		{
			if (!Asset
				|| !FDataForgeCompiler::IsManagedAssetOwnedBy(*Asset, RuleSet, PlannedAsset.OutputName, PlannedAsset.RecordId)
				|| Asset->GetPathName() != PlannedAsset.PreviousObjectPath)
			{
				FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
				Diagnostic.Severity = EDataForgeSeverity::Error;
				Diagnostic.Code = TEXT("DF4011");
				Diagnostic.RecordId = PlannedAsset.RecordId;
				Diagnostic.Field = PlannedAsset.OutputName;
				Diagnostic.Message = FString::Printf(TEXT("Managed asset ownership or source path changed before Move: %s"), *PlannedAsset.PreviousObjectPath);
				continue;
			}

			TArray<FAssetRenameData> RenameData;
			RenameData.Emplace(
				Asset,
				FPackageName::GetLongPackagePath(PlannedAsset.PackageName),
				FPackageName::GetLongPackageAssetName(PlannedAsset.PackageName));
			if (!FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameAssets(RenameData)
				|| Asset->GetPathName() != PlannedAsset.ObjectPath)
			{
				FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
				Diagnostic.Severity = EDataForgeSeverity::Error;
				Diagnostic.Code = TEXT("DF4012");
				Diagnostic.RecordId = PlannedAsset.RecordId;
				Diagnostic.Field = PlannedAsset.OutputName;
				Diagnostic.Message = FString::Printf(TEXT("Managed asset Move failed: %s -> %s"), *PlannedAsset.PreviousObjectPath, *PlannedAsset.ObjectPath);
				continue;
			}
		}
		if (!Asset)
		{
			UClass* AssetClass = PlannedAsset.AssetClass.Get();
			if (!AssetClass)
			{
				FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
				Diagnostic.Severity = EDataForgeSeverity::Error;
				Diagnostic.Code = TEXT("DF4010");
				Diagnostic.RecordId = PlannedAsset.RecordId;
				Diagnostic.Message = TEXT("Generated asset class became unavailable during Apply.");
				continue;
			}
			UPackage* Package = CreatePackage(*PlannedAsset.PackageName);
			const FName AssetName(*FPackageName::GetLongPackageAssetName(PlannedAsset.PackageName));
			Asset = NewObject<UDataAsset>(Package, AssetClass, AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Asset);
		}

		Asset->Modify();
		if (PlannedAsset.Change != EDataForgeManagedAssetChange::Unchanged
			&& !FDataForgeCompiler::ApplyPlannedProperties(*Asset, PlannedAsset, Diagnostics))
		{
			continue;
		}

		FMetaData& MetaData = Asset->GetPackage()->GetMetaData();
		MetaData.SetValue(Asset, TEXT("DataForge.Managed"), TEXT("true"));
		MetaData.SetValue(Asset, TEXT("DataForge.RuleSetId"), *RuleSet.RuleSetId.ToString(EGuidFormats::Digits));
		MetaData.SetValue(Asset, TEXT("DataForge.RecordId"), *PlannedAsset.RecordId.ToString());
		MetaData.SetValue(Asset, TEXT("DataForge.Role"), *PlannedAsset.OutputName.ToString());
		MetaData.SetValue(Asset, TEXT("DataForge.RuleVersion"), *FString::FromInt(RuleSet.RuleVersion));
		Asset->MarkPackageDirty();
		AssetsToSave.Add(Asset);
	}
	if (DataForgeEditorService::HasErrors(Diagnostics))
	{
		const FString Summary = TEXT("Apply failed while materializing managed assets. Packages remain dirty and were not saved.");
		DataForgeEditorService::UpdateRuleSetStatus(RuleSet, TEXT("Apply Failed"), Summary, Diagnostics);
		DataForgeEditorService::PreviewCache.Remove(&RuleSet);
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, Diagnostics, Summary);
		LogResult(RuleSet, Result, true);
		return Result;
	}

	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("PreparingOutput", "Binding DataTable output"));

	UDataTable* Table = Plan.ExistingTable.Get();
	bool bCreated = false;
	if (!Table)
	{
		UPackage* Package = CreatePackage(*RuleSet.Output.AssetPath);
		const FName AssetName(*FPackageName::GetLongPackageAssetName(RuleSet.Output.AssetPath));
		// A transactional DataTable can be serialized as part of its creation before
		// RowStruct is assigned, which emits a false "Missing RowStruct" error.
		Table = NewObject<UDataTable>(Package, AssetName, RF_Public | RF_Standalone);
		Table->RowStruct = RuleSet.Output.RowStruct;
		Table->SetFlags(RF_Transactional);
		FAssetRegistryModule::AssetCreated(Table);
		bCreated = true;
	}

	if (!bCreated)
	{
		Table->Modify();
	}
	FDataTableEditorUtils::BroadcastPreChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
	for (const FDataForgePlannedRow& PlannedRow : Plan.Rows)
	{
		if (PlannedRow.Change == EDataForgeRowChange::Orphan)
		{
			if (RuleSet.Output.bRemoveRowsMissingFromSource)
			{
				Table->RemoveRow(PlannedRow.RowName);
			}
			continue;
		}
		if (PlannedRow.Change != EDataForgeRowChange::Unchanged && PlannedRow.DesiredData.IsValid())
		{
			Table->AddRow(PlannedRow.RowName, PlannedRow.DesiredData->GetStructMemory(), RuleSet.Output.RowStruct);
		}
	}
	FDataTableEditorUtils::BroadcastPostChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
	Table->MarkPackageDirty();

	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SavingOutput", "Saving generated assets and DataTable output"));
	bool bSaved = true;
	if (RuleSet.Output.bSaveAfterApply)
	{
		for (UDataAsset* Asset : AssetsToSave)
		{
			const FString AssetFilename = FPackageName::LongPackageNameToFilename(Asset->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs AssetSaveArgs;
			AssetSaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			AssetSaveArgs.SaveFlags = SAVE_NoError;
			bSaved &= UPackage::SavePackage(Asset->GetPackage(), Asset, *AssetFilename, AssetSaveArgs);
		}
		const FString Filename = FPackageName::LongPackageNameToFilename(RuleSet.Output.AssetPath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		bSaved = UPackage::SavePackage(Table->GetPackage(), Table, *Filename, SaveArgs);
	}

	if (!bSaved)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF4004");
		Diagnostic.Message = TEXT("DataTable changes were made but the package could not be saved. The package remains dirty; use the recovery manifest if needed.");
	}

	const bool bSuccess = bSaved;
	const FString Summary = FString::Printf(
		TEXT("%s DataTable %s. %s Recovery: %s"),
		bCreated ? TEXT("Created") : TEXT("Updated"),
		*RuleSet.Output.AssetPath,
		*Plan.MakeSummary(),
		*RecoveryFilename);
	DataForgeEditorService::UpdateRuleSetStatus(RuleSet, bSuccess ? TEXT("Applied") : TEXT("Apply Failed"), Summary, Diagnostics);
	DataForgeEditorService::PreviewCache.Remove(&RuleSet);
	const FDataForgeResult Result = DataForgeEditorService::MakeResult(bSuccess, Diagnostics, Summary);
	LogResult(RuleSet, Result, true);
	return Result;
}

FDataForgeResult FDataForgeEditorService::CleanupOrphans(UDataForgeRuleSet& RuleSet)
{
	const DataForgeEditorService::FPreviewEntry* CachedPreview = DataForgeEditorService::PreviewCache.Find(&RuleSet);
	if (!CachedPreview)
	{
		TArray<FDataForgeDiagnostic> Diagnostics;
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF4020");
		Diagnostic.Message = TEXT("Cleanup blocked: run a successful Preview and review orphan candidates first.");
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, Diagnostics, Diagnostic.Message);
		LogResult(RuleSet, Result, true);
		return Result;
	}

	FCompiledDataForgeRuleSet Compiled;
	TArray<FDataForgeDiagnostic> Diagnostics;
	const bool bCompiled = FDataForgeCompiler::Compile(RuleSet, Compiled, Diagnostics);
	FDataForgeApplyPlan CurrentPlan;
	if (bCompiled)
	{
		CurrentPlan = FDataForgeCompiler::BuildPlan(Compiled);
		CurrentPlan.Diagnostics.Insert(Diagnostics, 0);
		Diagnostics = CurrentPlan.Diagnostics;
	}
	if (!bCompiled
		|| CurrentPlan.HasErrors()
		|| CachedPreview->RuleSignature != DataForgeEditorService::MakeRuleSignature(RuleSet)
		|| CachedPreview->Plan.SourceRevision != Compiled.DataSet.SourceRevision
		|| CachedPreview->Plan.TargetRevision != CurrentPlan.TargetRevision)
	{
		DataForgeEditorService::PreviewCache.Remove(&RuleSet);
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF4021");
		Diagnostic.Message = TEXT("Cleanup blocked: the RuleSet, source, or managed assets changed after Preview. Preview again.");
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, Diagnostics, Diagnostic.Message);
		LogResult(RuleSet, Result, true);
		return Result;
	}

	TArray<FAssetData> AssetsToDelete;
	for (const FDataForgePlannedAsset& PlannedAsset : CurrentPlan.ManagedAssets)
	{
		if (PlannedAsset.Change != EDataForgeManagedAssetChange::Orphan)
		{
			continue;
		}
		UDataAsset* Asset = PlannedAsset.ExistingAsset.Get();
		if (PlannedAsset.OutputName.IsNone()
			|| PlannedAsset.RecordId.IsNone()
			|| !Asset
			|| Asset->GetPathName() != PlannedAsset.ObjectPath
			|| !FDataForgeCompiler::IsManagedAssetOwnedBy(*Asset, RuleSet, PlannedAsset.OutputName, PlannedAsset.RecordId))
		{
			FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
			Diagnostic.Severity = EDataForgeSeverity::Error;
			Diagnostic.Code = TEXT("DF4022");
			Diagnostic.RecordId = PlannedAsset.RecordId;
			Diagnostic.Field = PlannedAsset.OutputName;
			Diagnostic.Message = FString::Printf(TEXT("Cleanup refused an asset whose ownership or path changed: %s"), *PlannedAsset.ObjectPath);
			continue;
		}
		AssetsToDelete.Emplace(Asset);
	}

	if (DataForgeEditorService::HasErrors(Diagnostics))
	{
		const FString Summary = TEXT("Cleanup failed ownership revalidation. No assets were deleted.");
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, Diagnostics, Summary);
		LogResult(RuleSet, Result, true);
		return Result;
	}
	if (AssetsToDelete.IsEmpty())
	{
		const FString Summary = TEXT("Cleanup found no managed orphan assets.");
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(true, Diagnostics, Summary);
		LogResult(RuleSet, Result, false);
		return Result;
	}

	FString RecoveryFilename;
	if (!DataForgeEditorService::WriteRecoveryManifest(RuleSet, CurrentPlan, RecoveryFilename))
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF4023");
		Diagnostic.Message = TEXT("Cleanup blocked: recovery manifest could not be written.");
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, Diagnostics, Diagnostic.Message);
		LogResult(RuleSet, Result, true);
		return Result;
	}

	const int32 RequestedDeleteCount = AssetsToDelete.Num();
	DataForgeEditorService::PreviewCache.Remove(&RuleSet);
	const int32 DeletedCount = ObjectTools::DeleteAssets(AssetsToDelete, false);
	if (DeletedCount != RequestedDeleteCount)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF4024");
		Diagnostic.Message = FString::Printf(TEXT("Cleanup deleted %d of %d managed orphan assets. Refer to recovery manifest: %s"), DeletedCount, RequestedDeleteCount, *RecoveryFilename);
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, Diagnostics, Diagnostic.Message);
		LogResult(RuleSet, Result, true);
		return Result;
	}

	const FString Summary = FString::Printf(TEXT("Deleted %d managed orphan asset(s). Recovery: %s"), DeletedCount, *RecoveryFilename);
	DataForgeEditorService::UpdateRuleSetStatus(RuleSet, TEXT("Cleaned"), Summary, Diagnostics);
	const FDataForgeResult Result = DataForgeEditorService::MakeResult(true, Diagnostics, Summary);
	LogResult(RuleSet, Result, false);
	return Result;
}

FDataForgeResult FDataForgeEditorService::ApplyDependencyGraph(UDataForgeRuleSet& RootRuleSet)
{
	const TArray<const UDataForgeRuleSet*> Roots = { &RootRuleSet };
	TArray<const UDataForgeRuleSet*> ExecutionOrder;
	TArray<FDataForgeDiagnostic> Diagnostics;
	if (!FDataForgeDependencyGraph::BuildExecutionOrder(Roots, ExecutionOrder, Diagnostics))
	{
		const FString Summary = TEXT("Dependency graph validation failed. Nothing was applied.");
		DataForgeEditorService::UpdateRuleSetStatus(RootRuleSet, TEXT("Invalid"), Summary, Diagnostics);
		const FDataForgeResult Result = DataForgeEditorService::MakeResult(false, Diagnostics, Summary);
		LogResult(RootRuleSet, Result, true);
		return Result;
	}

	int32 AppliedCount = 0;
	for (const UDataForgeRuleSet* ConstRuleSet : ExecutionOrder)
	{
		UDataForgeRuleSet* RuleSet = const_cast<UDataForgeRuleSet*>(ConstRuleSet);
		const FDataForgeResult PreviewResult = Preview(*RuleSet);
		Diagnostics.Append(PreviewResult.Diagnostics);
		if (!PreviewResult.bSuccess)
		{
			const FString Summary = FString::Printf(TEXT("Dependency graph Apply stopped before %s. %d prerequisite(s) were already applied."), *RuleSet->GetPathName(), AppliedCount);
			DataForgeEditorService::UpdateRuleSetStatus(RootRuleSet, TEXT("Apply Failed"), Summary, Diagnostics);
			return DataForgeEditorService::MakeResult(false, Diagnostics, Summary);
		}

		const FDataForgeResult ApplyResult = Apply(*RuleSet);
		Diagnostics.Append(ApplyResult.Diagnostics);
		if (!ApplyResult.bSuccess)
		{
			const FString Summary = FString::Printf(TEXT("Dependency graph Apply failed at %s. %d prerequisite(s) were already applied."), *RuleSet->GetPathName(), AppliedCount);
			DataForgeEditorService::UpdateRuleSetStatus(RootRuleSet, TEXT("Apply Failed"), Summary, Diagnostics);
			return DataForgeEditorService::MakeResult(false, Diagnostics, Summary);
		}
		++AppliedCount;
	}

	const FString Summary = FString::Printf(TEXT("Dependency graph Apply succeeded for %d RuleSet(s)."), AppliedCount);
	DataForgeEditorService::UpdateRuleSetStatus(RootRuleSet, TEXT("Applied Graph"), Summary, Diagnostics);
	return DataForgeEditorService::MakeResult(true, Diagnostics, Summary);
}

int32 FDataForgeEditorService::AutoMapExactNames(UDataForgeRuleSet& RuleSet, const TArray<FName>& SourceColumns)
{
	if (!RuleSet.Output.RowStruct)
	{
		return 0;
	}

	const FScopedTransaction Transaction(LOCTEXT("AutoMapTransaction", "Auto Map DataForge Bindings"));
	RuleSet.Modify();
	int32 AddedCount = 0;
	for (const FName Column : SourceColumns)
	{
		FProperty* Property = FindFProperty<FProperty>(RuleSet.Output.RowStruct, Column);
		if (!Property || !Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
		{
			continue;
		}
		const bool bAlreadyMapped = RuleSet.Bindings.ContainsByPredicate([Column](const FDataForgeBindingRule& Binding)
		{
			return Binding.Target == EDataForgeBindingTarget::DataTableRow
				&& Binding.TargetProperty.Equals(Column.ToString(), ESearchCase::IgnoreCase);
		});
		if (bAlreadyMapped)
		{
			continue;
		}

		FDataForgeBindingRule& Binding = RuleSet.Bindings.AddDefaulted_GetRef();
		Binding.Source = EDataForgeBindingSource::SourceValue;
		Binding.SourceColumn = Column;
		Binding.Target = EDataForgeBindingTarget::DataTableRow;
		Binding.TargetProperty = Column.ToString();
		++AddedCount;
	}

#if WITH_EDITORONLY_DATA
	RuleSet.LastStatus = TEXT("Draft");
	RuleSet.LastSummary = FString::Printf(TEXT("Auto Map added %d exact-name row bindings. Review and Preview before Apply."), AddedCount);
#endif
	if (RuleSet.GetPackage() != GetTransientPackage())
	{
		RuleSet.MarkPackageDirty();
	}
	return AddedCount;
}

FString FDataForgeBindingSuggestion::ToDisplayString() const
{
	const TCHAR* CompatibilityText = TEXT("Unknown");
	switch (Compatibility)
	{
	case EDataForgeBindingCompatibility::Direct: CompatibilityText = TEXT("Direct"); break;
	case EDataForgeBindingCompatibility::Convertible: CompatibilityText = TEXT("Convertible"); break;
	case EDataForgeBindingCompatibility::Risky: CompatibilityText = TEXT("Risky"); break;
	case EDataForgeBindingCompatibility::Unsupported: CompatibilityText = TEXT("Unsupported"); break;
	default: break;
	}
	return FString::Printf(TEXT("%s -> %s [%s]: %s"), *SourceType, *TargetType, CompatibilityText, *Message);
}

FDataForgeBindingSuggestion FDataForgeEditorService::AnalyzeBinding(
	const UDataForgeRuleSet& RuleSet,
	const FDataForgeBindingRule& Binding,
	const FDataForgeDataSet& DataSet)
{
	FProperty* TargetProperty = DataForgeEditorService::ResolveTargetProperty(RuleSet, Binding);
	if (!TargetProperty)
	{
		return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Unsupported, TEXT("Unknown"), TEXT("Missing Property"), TEXT("Select a valid target property."));
	}

	const FString TargetType = TargetProperty->GetCPPType();
	if (Binding.Source == EDataForgeBindingSource::GeneratedOutput)
	{
		return CastField<FSoftObjectProperty>(TargetProperty)
			? DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Direct, TEXT("Generated Output"), TargetType, TEXT("Soft reference preserves mutation-free Preview."))
			: DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Unsupported, TEXT("Generated Output"), TargetType, TEXT("Generated outputs require a soft object target."));
	}
	if (Binding.Source == EDataForgeBindingSource::ResolvedAsset)
	{
		if (CastField<FSoftObjectProperty>(TargetProperty) || CastField<FSoftClassProperty>(TargetProperty))
		{
			return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Direct, TEXT("Resolved Asset"), TargetType, TEXT("Resolved path binds as a soft reference."));
		}
		if (CastField<FObjectPropertyBase>(TargetProperty))
		{
			return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Convertible, TEXT("Resolved Asset"), TargetType, TEXT("Hard reference import may load the asset; prefer a soft reference."));
		}
		return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Unsupported, TEXT("Resolved Asset"), TargetType, TEXT("Choose an object or class reference target."));
	}

	const DataForgeEditorService::ESourceSampleType SourceType = DataForgeEditorService::InferSourceType(DataSet, Binding.SourceColumn);
	const FString SourceTypeText = DataForgeEditorService::SourceTypeName(SourceType);
	if (SourceType == DataForgeEditorService::ESourceSampleType::Unknown)
	{
		return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Unknown, SourceTypeText, TargetType, TEXT("No non-empty probe values are available."));
	}
	if (CastField<FStrProperty>(TargetProperty) || CastField<FNameProperty>(TargetProperty) || CastField<FTextProperty>(TargetProperty))
	{
		return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Direct, SourceTypeText, TargetType, TEXT("Canonical source text imports directly."));
	}
	if (CastField<FBoolProperty>(TargetProperty))
	{
		return SourceType == DataForgeEditorService::ESourceSampleType::Boolean
			? DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Direct, SourceTypeText, TargetType, TEXT("All sampled values are true/false literals."))
			: DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Risky, SourceTypeText, TargetType, TEXT("Use explicit true/false literals."));
	}
	const UEnum* Enum = nullptr;
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(TargetProperty))
	{
		Enum = EnumProperty->GetEnum();
	}
	else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(TargetProperty))
	{
		Enum = ByteProperty->Enum;
	}
	if (Enum)
	{
		return DataForgeEditorService::ValuesMatchEnum(DataSet, Binding.SourceColumn, *Enum)
			? DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Direct, SourceTypeText, TargetType, TEXT("All sampled values match enum names."))
			: DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Unsupported, SourceTypeText, TargetType, TEXT("At least one sampled value is not a valid enum name."));
	}
	if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(TargetProperty))
	{
		if (NumericProperty->IsInteger())
		{
			if (SourceType == DataForgeEditorService::ESourceSampleType::Integer)
			{
				return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Direct, SourceTypeText, TargetType, TEXT("All sampled values are integers."));
			}
			return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Risky, SourceTypeText, TargetType, TEXT("Fractional or text values may fail or lose precision."));
		}
		if (SourceType == DataForgeEditorService::ESourceSampleType::Integer)
		{
			return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Convertible, SourceTypeText, TargetType, TEXT("Integer values can be represented by a floating-point target."));
		}
		return SourceType == DataForgeEditorService::ESourceSampleType::Number
			? DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Direct, SourceTypeText, TargetType, TEXT("All sampled values are numeric."))
			: DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Unsupported, SourceTypeText, TargetType, TEXT("Non-numeric sampled values cannot bind to a numeric target."));
	}

	if (CastField<FSoftObjectProperty>(TargetProperty) || CastField<FSoftClassProperty>(TargetProperty) || CastField<FObjectPropertyBase>(TargetProperty))
	{
		return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Risky, SourceTypeText, TargetType, TEXT("Prefer a ResolvedAsset binding instead of importing an unchecked path."));
	}
	if (CastField<FArrayProperty>(TargetProperty))
	{
		return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Risky, SourceTypeText, TargetType, TEXT("Values must use Unreal array import syntax, for example (A,B)."));
	}
	return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Unsupported, SourceTypeText, TargetType, TEXT("No safe built-in conversion is known."));
}

FDataForgeBindingSuggestion FDataForgeEditorService::GetCachedBindingSuggestion(
	const UDataForgeRuleSet& RuleSet,
	const FDataForgeBindingRule& Binding)
{
	const FDataForgeDataSet* DataSet = DataForgeEditorService::ProbeCache.Find(&RuleSet);
	if (!DataSet)
	{
		return DataForgeEditorService::MakeSuggestion(EDataForgeBindingCompatibility::Unknown, TEXT("Unknown"), TEXT("Unknown"), TEXT("Run Probe to inspect conversion compatibility."));
	}
	return AnalyzeBinding(RuleSet, Binding, *DataSet);
}

void FDataForgeEditorService::InvalidateProbeCache(UDataForgeRuleSet& RuleSet)
{
	DataForgeEditorService::ProbeCache.Remove(&RuleSet);
}

void FDataForgeEditorService::LogResult(const UDataForgeRuleSet& RuleSet, const FDataForgeResult& Result, bool bOpenMessageLog)
{
	FMessageLog MessageLog(TEXT("DataForge"));
	MessageLog.NewPage(FText::FromString(RuleSet.GetName()));
	for (const FDataForgeDiagnostic& Diagnostic : Result.Diagnostics)
	{
		const FText Text = FText::FromString(FString::Printf(
			TEXT("%s %s [row=%s field=%s sourceRow=%d]"),
			*Diagnostic.Code,
			*Diagnostic.Message,
			*Diagnostic.RecordId.ToString(),
			*Diagnostic.Field.ToString(),
			Diagnostic.SourceRow));
		switch (Diagnostic.Severity)
		{
		case EDataForgeSeverity::Error:
			MessageLog.Error(Text);
			break;
		case EDataForgeSeverity::Warning:
			MessageLog.Warning(Text);
			break;
		default:
			MessageLog.Info(Text);
			break;
		}
	}
	MessageLog.Info(FText::FromString(Result.Summary));
	if (bOpenMessageLog)
	{
		MessageLog.Open(Result.bSuccess ? EMessageSeverity::Warning : EMessageSeverity::Error, true);
	}

	UE_LOG(LogDataForge, Display, TEXT("%s: %s"), *RuleSet.GetPathName(), *Result.Summary);
}

#undef LOCTEXT_NAMESPACE
