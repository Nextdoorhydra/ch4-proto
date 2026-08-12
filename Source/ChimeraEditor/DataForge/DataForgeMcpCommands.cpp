#include "DataForgeMcpCommands.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeCore.h"
#include "DataForgeEditorService.h"
#include "DataForgeRuleSet.h"
#include "Engine/DataTable.h"
#include "GoogleSheetConfig.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
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

	void ExecuteCreateCommand(const TArray<FString>& Arguments)
	{
		const FString Args = FString::Join(Arguments, TEXT(" "));
		FDataForgeGoogleRuleSetRequest Request;
		FParse::Value(*Args, TEXT("Config="), Request.GoogleParserPath);
		FParse::Value(*Args, TEXT("RuleSet="), Request.RuleSetPath);
		FParse::Value(*Args, TEXT("RowStruct="), Request.RowStructPath);
		FParse::Value(*Args, TEXT("Output="), Request.OutputDataTablePath);
		FString PrimaryKey;
		FParse::Value(*Args, TEXT("PrimaryKey="), PrimaryKey);
		Request.PrimaryKey = FName(*PrimaryKey);
		Request.bApply = ParseBoolArgument(Args, TEXT("Apply="), true);

		FDataForgeGoogleRuleSetResult Result;
		DataForgeMcpCommands::CreateRuleSetFromGoogleParser(Request, Result);
		UE_LOG(LogDataForge, Display, TEXT("[DataForge MCP] %s"), *Result.Message);
	}
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
	OutResult.Message = FString::Printf(TEXT("Created %s from %s; output=%s columns=%d bindings=%d apply=%s"),
		*OutResult.RuleSetObjectPath, *Config->GetPathName(), *OutResult.OutputDataTableObjectPath,
		OutResult.DetectedColumnCount, OutResult.BindingCount, Request.bApply ? TEXT("true") : TEXT("false"));
	return true;
}

IConsoleObject* DataForgeMcpCommands::Register()
{
	return IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("DataForge.MCP.CreateRuleSetFromGoogleParser"),
		TEXT("Create a DataForge RuleSet from GoogleSheetConfig. Config= is required; RuleSet=, RowStruct=, Output=, PrimaryKey=, Apply= are optional."),
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
