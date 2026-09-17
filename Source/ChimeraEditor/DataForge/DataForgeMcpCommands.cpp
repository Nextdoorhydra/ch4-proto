#include "DataForgeMcpCommands.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeAssetLayoutAuthoring.h"
#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeCore.h"
#include "DataForgeEditorService.h"
#include "DataForgeRuleSet.h"
#include "Engine/DataTable.h"
#include "GoogleSheetConfig.h"
#include "HAL/IConsoleManager.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
	FString ToObjectPath(const FString& Path)
	{
		if (Path.Contains(TEXT(".")))
		{
			return Path;
		}
		return Path + TEXT(".") + FPackageName::GetLongPackageAssetName(Path);
	}

	FString ToPackagePath(const FString& Path)
	{
		return Path.Contains(TEXT(".")) ? FPackageName::ObjectPathToPackageName(Path) : Path;
	}

	template <typename TObject>
	TObject* LoadProjectObject(const FString& Path)
	{
		return Path.IsEmpty() ? nullptr : LoadObject<TObject>(nullptr, *ToObjectPath(Path));
	}

	UDataTable* FindParserTargetTable(const UGoogleSheetConfig& Config)
	{
		const UObject* Parser = Config.GetActiveParser();
		if (!Parser)
		{
			return nullptr;
		}
		for (const FName CandidateName : { FName(TEXT("TargetTable")), FName(TEXT("DataTable")) })
		{
			const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Parser->GetClass(), CandidateName);
			if (Property && Property->PropertyClass->IsChildOf(UDataTable::StaticClass()))
			{
				return Cast<UDataTable>(Property->GetObjectPropertyValue_InContainer(Parser));
			}
		}
		return nullptr;
	}

	FName InferPrimaryKey(const TArray<FName>& Columns, FName Requested)
	{
		if (!Requested.IsNone())
		{
			return Columns.Contains(Requested) ? Requested : NAME_None;
		}
		for (const TCHAR* Preferred : { TEXT("RowName"), TEXT("Id"), TEXT("ID") })
		{
			const FName Candidate(Preferred);
			if (Columns.Contains(Candidate)) return Candidate;
		}
		for (const FName Column : Columns)
		{
			if (Column.ToString().EndsWith(TEXT("Id"), ESearchCase::IgnoreCase)) return Column;
		}
		return Columns.IsEmpty() ? NAME_None : Columns[0];
	}

	bool SaveAsset(UObject& Asset, FString& OutError)
	{
		const FString PackageName = Asset.GetOutermost()->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Asset.GetOutermost(), &Asset, *Filename, SaveArgs))
		{
			OutError = TEXT("Could not save asset: ") + Asset.GetPathName();
			return false;
		}
		return true;
	}

	bool ParseBoolArgument(const FString& Args, const TCHAR* Key, bool DefaultValue)
	{
		FString Value;
		if (!FParse::Value(*Args, Key, Value)) return DefaultValue;
		return !Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) && Value != TEXT("0");
	}

	bool ReadRequiredString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& OutValue, FString& OutError)
	{
		if (!Object.IsValid() || !Object->TryGetStringField(Field, OutValue) || OutValue.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Spec field '%s' is required."), Field);
			return false;
		}
		return true;
	}

	bool ParseBindingSource(const FString& Value, EDataForgeBindingSource& OutValue)
	{
		if (Value.Equals(TEXT("SourceValue"), ESearchCase::IgnoreCase)) OutValue = EDataForgeBindingSource::SourceValue;
		else if (Value.Equals(TEXT("ResolvedAsset"), ESearchCase::IgnoreCase)) OutValue = EDataForgeBindingSource::ResolvedAsset;
		else if (Value.Equals(TEXT("GeneratedOutput"), ESearchCase::IgnoreCase)) OutValue = EDataForgeBindingSource::GeneratedOutput;
		else return false;
		return true;
	}

	bool ParseBindingTarget(const FString& Value, EDataForgeBindingTarget& OutValue)
	{
		if (Value.Equals(TEXT("DataTableRow"), ESearchCase::IgnoreCase)) OutValue = EDataForgeBindingTarget::DataTableRow;
		else if (Value.Equals(TEXT("GeneratedOutput"), ESearchCase::IgnoreCase)) OutValue = EDataForgeBindingTarget::GeneratedOutput;
		else return false;
		return true;
	}

	bool ParseSpecFile(const FString& ConfiguredFilename, FDataForgeGoogleRuleSetRequest& OutRequest, FString& OutError)
	{
		FString Filename = ConfiguredFilename;
		if (FPaths::IsRelative(Filename)) Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Filename);
		FPaths::NormalizeFilename(Filename);
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Filename))
		{
			OutError = TEXT("Could not read DataForge MCP spec: ") + Filename;
			return false;
		}
		return DataForgeMcpCommands::ParseRequestSpec(Json, OutRequest, OutError);
	}

	void ExecuteCreateCommand(const TArray<FString>& Arguments)
	{
		const FString Args = FString::Join(Arguments, TEXT(" "));
		FDataForgeGoogleRuleSetRequest Request;
		FString SpecFilename;
		FParse::Value(*Args, TEXT("Spec="), SpecFilename);
		if (!SpecFilename.IsEmpty())
		{
			FString Error;
			if (!ParseSpecFile(SpecFilename, Request, Error))
			{
				UE_LOG(LogDataForge, Error, TEXT("[DataForge MCP] %s"), *Error);
				return;
			}
		}
		else
		{
		FParse::Value(*Args, TEXT("Config="), Request.GoogleParserPath);
		FParse::Value(*Args, TEXT("RuleSet="), Request.RuleSetPath);
		FParse::Value(*Args, TEXT("RowStruct="), Request.RowStructPath);
		FParse::Value(*Args, TEXT("Output="), Request.OutputDataTablePath);
		FParse::Value(*Args, TEXT("Profile="), Request.AssetLayoutProfilePath);
		FString ProfilePurpose;
		FParse::Value(*Args, TEXT("ProfilePurpose="), ProfilePurpose);
		Request.AssetLayoutPurpose = FName(*ProfilePurpose);
		FString PrimaryKey;
		FParse::Value(*Args, TEXT("PrimaryKey="), PrimaryKey);
		Request.PrimaryKey = FName(*PrimaryKey);
		Request.bApply = ParseBoolArgument(Args, TEXT("Apply="), true);
		}

		FDataForgeGoogleRuleSetResult Result;
		DataForgeMcpCommands::CreateRuleSetFromGoogleParser(Request, Result);
		UE_LOG(LogDataForge, Display, TEXT("[DataForge MCP] %s"), *Result.Message);
	}
}

bool DataForgeMcpCommands::ParseRequestSpec(
	const FString& Json,
	FDataForgeGoogleRuleSetRequest& OutRequest,
	FString& OutError)
{
	OutRequest = FDataForgeGoogleRuleSetRequest();
	OutError.Reset();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Spec is not valid JSON.");
		return false;
	}
	if (!ReadRequiredString(Root, TEXT("config"), OutRequest.GoogleParserPath, OutError)
		|| !ReadRequiredString(Root, TEXT("ruleSet"), OutRequest.RuleSetPath, OutError))
	{
		return false;
	}
	Root->TryGetStringField(TEXT("rowStruct"), OutRequest.RowStructPath);
	Root->TryGetStringField(TEXT("output"), OutRequest.OutputDataTablePath);
	FString PrimaryKey;
	Root->TryGetStringField(TEXT("primaryKey"), PrimaryKey);
	OutRequest.PrimaryKey = FName(*PrimaryKey);
	Root->TryGetBoolField(TEXT("apply"), OutRequest.bApply);
	Root->TryGetBoolField(TEXT("saveAssets"), OutRequest.bSaveAssets);

	if (const TSharedPtr<FJsonValue>* ProfileField = Root->Values.Find(TEXT("assetLayoutProfile")))
	{
		if (!ProfileField->IsValid())
		{
			OutError = TEXT("Spec field 'assetLayoutProfile' must be a path string or object.");
			return false;
		}
		if ((*ProfileField)->Type == EJson::String)
		{
			OutRequest.AssetLayoutProfilePath = (*ProfileField)->AsString();
		}
		else if ((*ProfileField)->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> ProfileObject = (*ProfileField)->AsObject();
			FString Purpose;
			ProfileObject->TryGetStringField(TEXT("path"), OutRequest.AssetLayoutProfilePath);
			ProfileObject->TryGetStringField(TEXT("purpose"), Purpose);
			OutRequest.AssetLayoutPurpose = FName(*Purpose);
			if (const TSharedPtr<FJsonValue>* ParametersField = ProfileObject->Values.Find(TEXT("parameters")))
			{
				if (!ParametersField->IsValid() || (*ParametersField)->Type != EJson::Object)
				{
					OutError = TEXT("assetLayoutProfile.parameters must be an object containing string values.");
					return false;
				}
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Parameter : (*ParametersField)->AsObject()->Values)
				{
					if (!Parameter.Value.IsValid() || Parameter.Value->Type != EJson::String)
					{
						OutError = FString::Printf(TEXT("Asset Layout parameter '%s' must be a string."), *Parameter.Key);
						return false;
					}
					OutRequest.AssetLayoutParameters.Add(FName(*Parameter.Key), Parameter.Value->AsString());
				}
			}
		}
		else
		{
			OutError = TEXT("Spec field 'assetLayoutProfile' must be a path string or object.");
			return false;
		}
		if (!OutRequest.AssetLayoutProfilePath.IsEmpty() && !OutRequest.AssetLayoutPurpose.IsNone())
		{
			OutError = TEXT("assetLayoutProfile must specify either 'path' or 'purpose', not both.");
			return false;
		}
		if (OutRequest.AssetLayoutProfilePath.IsEmpty() && OutRequest.AssetLayoutPurpose.IsNone())
		{
			OutError = TEXT("assetLayoutProfile requires a non-empty 'path' or 'purpose'.");
			return false;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetRuleValues = nullptr;
	if (Root->TryGetArrayField(TEXT("assetRules"), AssetRuleValues))
	{
		for (const TSharedPtr<FJsonValue>& Value : *AssetRuleValues)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Id, Ownership;
			FDataForgeAssetRule Rule;
			if (!ReadRequiredString(Object, TEXT("id"), Id, OutError)
				|| !ReadRequiredString(Object, TEXT("ownership"), Ownership, OutError)
				|| !ReadRequiredString(Object, TEXT("baseFolder"), Rule.BaseFolder, OutError)
				|| !ReadRequiredString(Object, TEXT("assetNamePattern"), Rule.AssetNamePattern, OutError)) return false;
			Rule.RuleId = FName(*Id);
			if (Ownership.Equals(TEXT("Managed"), ESearchCase::IgnoreCase)) Rule.Ownership = EDataForgeAssetOwnership::Managed;
			else if (Ownership.Equals(TEXT("External"), ESearchCase::IgnoreCase)) Rule.Ownership = EDataForgeAssetOwnership::External;
			else
			{
				OutError = FString::Printf(TEXT("Asset Rule '%s' ownership must be Managed or External."), *Id);
				return false;
			}
			Object->TryGetStringField(TEXT("subfolderPattern"), Rule.SubfolderPattern);
			OutRequest.AssetRules.Add(MoveTemp(Rule));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* OutputValues = nullptr;
	if (Root->TryGetArrayField(TEXT("generatedOutputs"), OutputValues))
	{
		for (const TSharedPtr<FJsonValue>& Value : *OutputValues)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Name, Type, ClassPath, AssetRuleId;
			if (!ReadRequiredString(Object, TEXT("name"), Name, OutError)
				|| !ReadRequiredString(Object, TEXT("type"), Type, OutError)
				|| !ReadRequiredString(Object, TEXT("class"), ClassPath, OutError)
				|| !ReadRequiredString(Object, TEXT("assetRule"), AssetRuleId, OutError)) return false;
			UClass* AssetClass = LoadObject<UClass>(nullptr, *ClassPath);
			if (!AssetClass || !AssetClass->IsChildOf(UDataAsset::StaticClass()))
			{
				OutError = FString::Printf(TEXT("Generated Output '%s' class is not a UDataAsset class: %s"), *Name, *ClassPath);
				return false;
			}
			FDataForgeGeneratedAssetOutputRule OutputRule;
			OutputRule.OutputName = FName(*Name);
			OutputRule.AssetClass = AssetClass;
			OutputRule.AssetRuleId = FName(*AssetRuleId);
			if (Type.Equals(TEXT("DataAsset"), ESearchCase::IgnoreCase)) OutputRule.Type = EDataForgeGeneratedAssetType::DataAsset;
			else if (Type.Equals(TEXT("PrimaryDataAsset"), ESearchCase::IgnoreCase)) OutputRule.Type = EDataForgeGeneratedAssetType::PrimaryDataAsset;
			else
			{
				OutError = FString::Printf(TEXT("Generated Output '%s' type must be DataAsset or PrimaryDataAsset."), *Name);
				return false;
			}
			OutRequest.GeneratedOutputs.Add(MoveTemp(OutputRule));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* BindingValues = nullptr;
	if (Root->TryGetArrayField(TEXT("bindings"), BindingValues))
	{
		for (const TSharedPtr<FJsonValue>& Value : *BindingValues)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Source, Target;
			FDataForgeBindingRule Binding;
			if (!ReadRequiredString(Object, TEXT("source"), Source, OutError)
				|| !ReadRequiredString(Object, TEXT("target"), Target, OutError)
				|| !ReadRequiredString(Object, TEXT("property"), Binding.TargetProperty, OutError)) return false;
			if (!ParseBindingSource(Source, Binding.Source) || !ParseBindingTarget(Target, Binding.Target))
			{
				OutError = TEXT("Binding source or target has an unsupported value.");
				return false;
			}
			FString Field;
			Object->TryGetStringField(TEXT("column"), Field); Binding.SourceColumn = FName(*Field);
			Field.Reset(); Object->TryGetStringField(TEXT("sourceOutput"), Field); Binding.SourceOutput = FName(*Field);
			Field.Reset(); Object->TryGetStringField(TEXT("targetOutput"), Field); Binding.TargetOutput = FName(*Field);
			Field.Reset(); Object->TryGetStringField(TEXT("assetRule"), Field); Binding.AssetRuleId = FName(*Field);
			Object->TryGetBoolField(TEXT("required"), Binding.bRequired);
			OutRequest.Bindings.Add(MoveTemp(Binding));
		}
	}
	return true;
}

void DataForgeMcpCommands::DiscoverAssetLayoutProfiles(FName Purpose, TArray<FString>& OutObjectPaths)
{
	OutObjectPaths.Reset();
	if (Purpose.IsNone()) return;
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> ProfileAssets;
	AssetRegistry.GetAssetsByClass(UDataForgeAssetLayoutProfile::StaticClass()->GetClassPathName(), ProfileAssets, true);
	for (const FAssetData& AssetData : ProfileAssets)
	{
		const UDataForgeAssetLayoutProfile* Profile = Cast<UDataForgeAssetLayoutProfile>(AssetData.GetAsset());
		if (Profile && (Profile->Purpose == Purpose || Profile->Tags.Contains(Purpose)))
		{
			OutObjectPaths.Add(AssetData.GetObjectPathString());
		}
	}
	OutObjectPaths.Sort();
}

bool DataForgeMcpCommands::ResolveAssetLayoutProfile(
	const FDataForgeGoogleRuleSetRequest& Request,
	UDataForgeAssetLayoutProfile*& OutProfile,
	TArray<FString>& OutCandidates,
	FString& OutError)
{
	OutProfile = nullptr;
	OutCandidates.Reset();
	OutError.Reset();
	if (!Request.AssetLayoutProfilePath.IsEmpty())
	{
		OutProfile = LoadProjectObject<UDataForgeAssetLayoutProfile>(Request.AssetLayoutProfilePath);
		if (!OutProfile)
		{
			OutError = TEXT("Asset Layout Profile does not exist: ") + Request.AssetLayoutProfilePath;
			return false;
		}
		OutCandidates.Add(OutProfile->GetPathName());
		return true;
	}
	if (Request.AssetLayoutPurpose.IsNone())
	{
		return true;
	}

	DiscoverAssetLayoutProfiles(Request.AssetLayoutPurpose, OutCandidates);
	if (OutCandidates.Num() != 1)
	{
		OutError = OutCandidates.IsEmpty()
			? FString::Printf(TEXT("No Asset Layout Profile matches purpose or tag '%s'. Supply an exact assetLayoutProfile.path or use manual assetRules."), *Request.AssetLayoutPurpose.ToString())
			: FString::Printf(TEXT("Asset Layout Profile discovery for '%s' is ambiguous (%d candidates): %s"), *Request.AssetLayoutPurpose.ToString(), OutCandidates.Num(), *FString::Join(OutCandidates, TEXT(", ")));
		return false;
	}
	OutProfile = LoadProjectObject<UDataForgeAssetLayoutProfile>(OutCandidates[0]);
	if (!OutProfile)
	{
		OutError = TEXT("Discovered Asset Layout Profile could not be loaded: ") + OutCandidates[0];
		return false;
	}
	return true;
}

bool DataForgeMcpCommands::CreateRuleSetFromGoogleParser(
	const FDataForgeGoogleRuleSetRequest& Request,
	FDataForgeGoogleRuleSetResult& OutResult)
{
	OutResult = FDataForgeGoogleRuleSetResult();
	UGoogleSheetConfig* Config = LoadProjectObject<UGoogleSheetConfig>(Request.GoogleParserPath);
	if (!Config)
	{
		OutResult.Message = TEXT("Config must reference an existing GoogleSheetConfig asset.");
		return false;
	}
	UDataForgeAssetLayoutProfile* LayoutProfile = nullptr;
	TArray<FString> LayoutCandidates;
	FString LayoutError;
	if (!ResolveAssetLayoutProfile(Request, LayoutProfile, LayoutCandidates, LayoutError))
	{
		OutResult.Message = LayoutError;
		return false;
	}

	UDataTable* ParserTable = FindParserTargetTable(*Config);
	UScriptStruct* RowStruct = ParserTable ? const_cast<UScriptStruct*>(ParserTable->GetRowStruct()) : nullptr;
	FString OutputPath = ParserTable ? ParserTable->GetOutermost()->GetName() : FString();
	if (!Request.RowStructPath.IsEmpty()) RowStruct = LoadProjectObject<UScriptStruct>(Request.RowStructPath);
	if (!Request.OutputDataTablePath.IsEmpty()) OutputPath = ToPackagePath(Request.OutputDataTablePath);
	if (!RowStruct || !RowStruct->IsChildOf(FTableRowBase::StaticStruct()))
	{
		OutResult.Message = TEXT("The parser has no compatible TargetTable. Supply RowStruct=/Script/Module.RowStruct.");
		return false;
	}
	if (!FPackageName::IsValidLongPackageName(OutputPath))
	{
		OutResult.Message = TEXT("The parser has no target DataTable. Supply Output=/Game/Path/DT_Name.");
		return false;
	}

	const FString RuleSetPath = ToPackagePath(Request.RuleSetPath.IsEmpty()
		? TEXT("/Game/DataForge/Rules/RS_") + Config->GetName()
		: Request.RuleSetPath);
	if (!FPackageName::IsValidLongPackageName(RuleSetPath))
	{
		OutResult.Message = TEXT("RuleSet must be a valid /Game package path.");
		return false;
	}
	const FString RuleSetObjectPath = ToObjectPath(RuleSetPath);
	if (FindObject<UDataForgeRuleSet>(nullptr, *RuleSetObjectPath) || FPackageName::DoesPackageExist(RuleSetPath))
	{
		OutResult.Message = TEXT("RuleSet already exists and was not modified: ") + RuleSetObjectPath;
		return false;
	}

	TStrongObjectPtr<UDataForgeRuleSet> Draft(NewObject<UDataForgeRuleSet>(GetTransientPackage()));
	Draft->RuleSetId = FGuid::NewGuid();
	Draft->Source.AdapterId = TEXT("GoogleSheetCache");
	Draft->Source.SourceAsset = Config;
	Draft->Output.RowStruct = RowStruct;
	Draft->Output.AssetPath = OutputPath;
	Draft->Output.bSaveAfterApply = Request.bSaveAssets;
	Draft->AssetRules = Request.AssetRules;
	Draft->GeneratedOutputs = Request.GeneratedOutputs;
	Draft->Bindings = Request.Bindings;

	FDataForgeDataSet DataSet;
	const FDataForgeResult ProbeResult = FDataForgeEditorService::Probe(*Draft, &DataSet);
	if (!ProbeResult.bSuccess)
	{
		OutResult.Message = TEXT("Google parser Probe failed. Run Fetch with Save Normalized Json enabled: ") + ProbeResult.Summary;
		return false;
	}
	Draft->Schema.RequiredColumns = DataSet.Columns;
	Draft->Schema.PrimaryKey = InferPrimaryKey(DataSet.Columns, Request.PrimaryKey);
	if (Draft->Schema.PrimaryKey.IsNone())
	{
		OutResult.Message = TEXT("PrimaryKey was not found in the Google parser columns.");
		return false;
	}
	if (LayoutProfile)
	{
		const FDataForgeResult LayoutResult = FDataForgeAssetLayoutAuthoring::Materialize(
			*Draft,
			*LayoutProfile,
			Request.AssetLayoutParameters,
			DataSet.Columns);
		if (!LayoutResult.bSuccess)
		{
			const FString Diagnostics = FString::JoinBy(LayoutResult.Diagnostics, TEXT(" | "), [](const FDataForgeDiagnostic& Diagnostic)
			{
				return Diagnostic.Code + TEXT(": ") + Diagnostic.Message;
			});
			OutResult.Message = TEXT("Asset Layout Profile materialization failed: ") + (Diagnostics.IsEmpty() ? LayoutResult.Summary : Diagnostics);
			return false;
		}
		OutResult.AssetLayoutProfileObjectPath = LayoutProfile->GetPathName();
	}
	FDataForgeEditorService::AutoMapExactNames(*Draft, DataSet.Columns);
	const FDataForgeResult DraftPreview = FDataForgeEditorService::Preview(*Draft);
	if (!DraftPreview.bSuccess)
	{
		OutResult.Message = TEXT("Inferred RuleSet Preview failed: ") + DraftPreview.Summary;
		return false;
	}

	UPackage* RuleSetPackage = CreatePackage(*RuleSetPath);
	const FName RuleSetName(*FPackageName::GetLongPackageAssetName(RuleSetPath));
	UDataForgeRuleSet* RuleSet = DuplicateObject<UDataForgeRuleSet>(Draft.Get(), RuleSetPackage, RuleSetName);
	RuleSet->SetFlags(RF_Public | RF_Standalone);
	if (Request.bApply)
	{
		const FDataForgeResult PreviewResult = FDataForgeEditorService::Preview(*RuleSet);
		const FDataForgeResult ApplyResult = PreviewResult.bSuccess ? FDataForgeEditorService::Apply(*RuleSet) : PreviewResult;
		if (!ApplyResult.bSuccess)
		{
			OutResult.Message = TEXT("RuleSet Apply failed: ") + ApplyResult.Summary;
			return false;
		}
	}

	FAssetRegistryModule::AssetCreated(RuleSet);
	Config->bSaveNormalizedJson = true;
	Config->bAutoApplyDataForge = true;
	Config->MarkPackageDirty();
	RuleSet->MarkPackageDirty();
	if (Request.bSaveAssets)
	{
		FString SaveError;
		if (!SaveAsset(*RuleSet, SaveError) || !SaveAsset(*Config, SaveError))
		{
			OutResult.Message = SaveError;
			return false;
		}
	}

	OutResult.bSuccess = true;
	OutResult.RuleSetObjectPath = RuleSet->GetPathName();
	OutResult.OutputDataTableObjectPath = ToObjectPath(OutputPath);
	OutResult.DetectedColumnCount = DataSet.Columns.Num();
	OutResult.BindingCount = RuleSet->Bindings.Num();
	OutResult.Message = FString::Printf(TEXT("Created %s from %s; output=%s profile=%s columns=%d bindings=%d apply=%s"),
		*OutResult.RuleSetObjectPath, *Config->GetPathName(), *OutResult.OutputDataTableObjectPath,
		OutResult.AssetLayoutProfileObjectPath.IsEmpty() ? TEXT("manual") : *OutResult.AssetLayoutProfileObjectPath,
		OutResult.DetectedColumnCount, OutResult.BindingCount, Request.bApply ? TEXT("true") : TEXT("false"));
	return true;
}

IConsoleObject* DataForgeMcpCommands::Register()
{
	return IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("DataForge.MCP.CreateRuleSetFromGoogleParser"),
		TEXT("Create a DataForge RuleSet from GoogleSheetConfig. Use Spec=path.json for assetLayoutProfile, rules, and outputs; or Config= with optional Profile= or ProfilePurpose=, RuleSet=, RowStruct=, Output=, PrimaryKey=, Apply=."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ExecuteCreateCommand),
		ECVF_Default);
}

void DataForgeMcpCommands::Unregister(IConsoleObject*& Command)
{
	if (Command)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Command);
		Command = nullptr;
	}
}
