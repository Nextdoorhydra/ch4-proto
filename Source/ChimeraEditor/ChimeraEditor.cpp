#include "ChimeraEditor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeCore.h"
#include "DataForge/DataForgeMcpCommands.h"
#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeAutoReconciler.h"
#include "DataForgeBindingPresetAuthoring.h"
#include "DataForgeEditorService.h"
#include "DataForgeBindingPreset.h"
#include "DataForgeFolderSource.h"
#include "DataForgeNamingPolicy.h"
#include "DataForgePipeline.h"
#include "DataForgeRuleSet.h"
#include "Data/DataForgeExamples/CMMultiAssetReferenceExample.h"
#include "GoogleSheetConfig.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Tests/GoogleDataForgeIntegrationTestTypes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

#define LOCTEXT_NAMESPACE "FChimeraEditorModule"

namespace
{
	bool ReferencesGoogleSheetConfig(const UDataForgeRuleSet& RuleSet, const FSoftObjectPath& ConfigPath)
	{
		if (RuleSet.Source.AdapterId == TEXT("GoogleSheetCache"))
		{
			return RuleSet.Source.SourceAsset.ToSoftObjectPath() == ConfigPath;
		}
		return RuleSet.Source.AdapterId == TEXT("MultiSource")
			&& RuleSet.Source.Inputs.ContainsByPredicate([&ConfigPath](const FDataForgeSourceInput& Input)
			{
				return Input.AdapterId == TEXT("GoogleSheetCache") && Input.SourceAsset.ToSoftObjectPath() == ConfigPath;
			});
	}

	class FGoogleSheetCacheDataForgeAdapter final : public IDataForgeSourceAdapter
	{
	public:
		virtual FDataForgeSourceDescriptor Describe() const override
		{
			return { TEXT("GoogleSheetCache"), LOCTEXT("GoogleSheetCacheAdapter", "Google Sheet Cache"),
				TEXT("Reads the normalized JSON cache produced by a GoogleSheetConfig asset."), TEXT("json") };
		}

		virtual bool Probe(const FDataForgeSourceConfig& Source, FDataForgeDataSet& OutDataSet, TArray<FDataForgeDiagnostic>& OutDiagnostics) const override
		{
			return Read(Source, FMath::Max(1, Source.ProbeRowLimit), OutDataSet, OutDiagnostics);
		}

		virtual bool Fetch(const FDataForgeSourceConfig& Source, FDataForgeDataSet& OutDataSet, TArray<FDataForgeDiagnostic>& OutDiagnostics) const override
		{
			return Read(Source, INDEX_NONE, OutDataSet, OutDiagnostics);
		}

	private:
		static bool Read(const FDataForgeSourceConfig& Source, int32 RowLimit, FDataForgeDataSet& OutDataSet, TArray<FDataForgeDiagnostic>& OutDiagnostics)
		{
			const UGoogleSheetConfig* Config = Cast<UGoogleSheetConfig>(Source.SourceAsset.LoadSynchronous());
			if (!Config)
			{
				FDataForgeDiagnostic& Diagnostic = OutDiagnostics.AddDefaulted_GetRef();
				Diagnostic.Severity = EDataForgeSeverity::Error;
				Diagnostic.Code = TEXT("DFGS01");
				Diagnostic.Message = TEXT("Select a GoogleSheetConfig in Source Asset.");
				return false;
			}

			const FString CacheFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GoogleSheetLoader"),
				FPaths::MakeValidFileName(Config->GetPathName(), TEXT('_')) + TEXT(".json"));
			if (!IFileManager::Get().FileExists(*CacheFile))
			{
				FDataForgeDiagnostic& Diagnostic = OutDiagnostics.AddDefaulted_GetRef();
				Diagnostic.Severity = EDataForgeSeverity::Error;
				Diagnostic.Code = TEXT("DFGS02");
				Diagnostic.Message = FString::Printf(TEXT("Google Sheet cache is missing. Enable Save Normalized Json and run Fetch on %s: %s"), *Config->GetName(), *CacheFile);
				return false;
			}

			FString Json;
			if (!FFileHelper::LoadFileToString(Json, *CacheFile))
			{
				FDataForgeDiagnostic& Diagnostic = OutDiagnostics.AddDefaulted_GetRef();
				Diagnostic.Severity = EDataForgeSeverity::Error;
				Diagnostic.Code = TEXT("DFGS03");
				Diagnostic.Message = FString::Printf(TEXT("Could not read Google Sheet cache: %s"), *CacheFile);
				return false;
			}
			return FDataForgeJsonSourceAdapter::Parse(Json, RowLimit, OutDataSet, OutDiagnostics);
		}
	};

	void AddMultiSourceDiagnostic(TArray<FDataForgeDiagnostic>& Diagnostics, EDataForgeSeverity Severity, const TCHAR* Code, const FString& Message, FName Field = NAME_None)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Field = Field;
		Diagnostic.Message = Message;
	}

	class FMultiSourceDataForgeAdapter final : public IDataForgeSourceAdapter
	{
	public:
		virtual FDataForgeSourceDescriptor Describe() const override
		{
			return { TEXT("MultiSource"), LOCTEXT("MultiSourceAdapter", "Multi Source (Join)"),
				TEXT("Left-joins CSV, JSON, and Google Sheet Cache inputs into one canonical data set."), FString() };
		}

		virtual bool Probe(const FDataForgeSourceConfig& Source, FDataForgeDataSet& OutDataSet, TArray<FDataForgeDiagnostic>& OutDiagnostics) const override
		{
			return Read(Source, true, OutDataSet, OutDiagnostics);
		}

		virtual bool Fetch(const FDataForgeSourceConfig& Source, FDataForgeDataSet& OutDataSet, TArray<FDataForgeDiagnostic>& OutDiagnostics) const override
		{
			return Read(Source, false, OutDataSet, OutDiagnostics);
		}

	private:
		static FDataForgeSourceConfig MakeChildConfig(const FDataForgeSourceInput& Input, int32 ProbeRowLimit)
		{
			FDataForgeSourceConfig Child;
			Child.AdapterId = Input.AdapterId;
			Child.File = Input.File;
			Child.SourceAsset = Input.SourceAsset;
			Child.ProbeRowLimit = ProbeRowLimit;
			Child.Parameters = Input.Parameters;
			return Child;
		}

		static bool ValidateAndIndex(const FDataForgeDataSet& DataSet, FName JoinColumn, int32 InputIndex,
			TMap<FString, const FDataForgeRow*>& OutRows, TArray<FDataForgeDiagnostic>& Diagnostics)
		{
			if (JoinColumn.IsNone() || !DataSet.Columns.Contains(JoinColumn))
			{
				AddMultiSourceDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DFMS03"),
					FString::Printf(TEXT("Input %d Join Column '%s' is empty or missing."), InputIndex + 1, *JoinColumn.ToString()), JoinColumn);
				return false;
			}
			bool bValid = true;
			for (const FDataForgeRow& Row : DataSet.Rows)
			{
				const FString* Key = Row.Values.Find(JoinColumn);
				if (!Key || Key->IsEmpty())
				{
					AddMultiSourceDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DFMS04"),
						FString::Printf(TEXT("Input %d contains an empty join key at source row %d."), InputIndex + 1, Row.SourceRow), JoinColumn);
					bValid = false;
					continue;
				}
				if (OutRows.Contains(*Key))
				{
					AddMultiSourceDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DFMS05"),
						FString::Printf(TEXT("Input %d contains duplicate join key '%s'."), InputIndex + 1, **Key), JoinColumn);
					bValid = false;
					continue;
				}
				OutRows.Add(*Key, &Row);
			}
			return bValid;
		}

		static bool Read(const FDataForgeSourceConfig& Source, bool bProbe, FDataForgeDataSet& OutDataSet, TArray<FDataForgeDiagnostic>& OutDiagnostics)
		{
			OutDataSet = FDataForgeDataSet();
			if (Source.Inputs.IsEmpty())
			{
				AddMultiSourceDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DFMS01"), TEXT("Multi Source requires at least one input."));
				return false;
			}

			TArray<FDataForgeDataSet> Inputs;
			FString CombinedRevision;
			for (int32 Index = 0; Index < Source.Inputs.Num(); ++Index)
			{
				const FDataForgeSourceInput& Input = Source.Inputs[Index];
				if (Input.AdapterId == TEXT("MultiSource"))
				{
					AddMultiSourceDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DFMS02"), TEXT("Multi Source cannot recursively contain Multi Source."));
					return false;
				}
				const TSharedPtr<const IDataForgeSourceAdapter> Adapter = FDataForgeSourceAdapterRegistry::Get().Find(Input.AdapterId);
				if (!Adapter.IsValid())
				{
					AddMultiSourceDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DFMS02"),
						FString::Printf(TEXT("Input %d adapter '%s' is not registered."), Index + 1, *Input.AdapterId.ToString()));
					return false;
				}
				FDataForgeDataSet& InputData = Inputs.AddDefaulted_GetRef();
				const FDataForgeSourceConfig Child = MakeChildConfig(Input, Source.ProbeRowLimit);
				const bool bRead = bProbe && Index == 0
					? Adapter->Probe(Child, InputData, OutDiagnostics)
					: Adapter->Fetch(Child, InputData, OutDiagnostics);
				if (!bRead) return false;
				CombinedRevision += FString::Printf(TEXT("%d|%s|%s;"), Index, *Input.AdapterId.ToString(), *InputData.SourceRevision);
			}

			TMap<FString, const FDataForgeRow*> PrimaryIndex;
			if (!ValidateAndIndex(Inputs[0], Source.Inputs[0].JoinColumn, 0, PrimaryIndex, OutDiagnostics)) return false;
			OutDataSet = Inputs[0];

			for (int32 InputIndex = 1; InputIndex < Inputs.Num(); ++InputIndex)
			{
				const FDataForgeSourceInput& InputConfig = Source.Inputs[InputIndex];
				const FDataForgeDataSet& InputData = Inputs[InputIndex];
				TMap<FString, const FDataForgeRow*> ForeignRows;
				if (!ValidateAndIndex(InputData, InputConfig.JoinColumn, InputIndex, ForeignRows, OutDiagnostics)) return false;

				TArray<TPair<FName, FName>> JoinedColumns;
				for (const FName Column : InputData.Columns)
				{
					if (Column == InputConfig.JoinColumn) continue;
					const FName TargetColumn(*(InputConfig.ColumnPrefix + Column.ToString()));
					JoinedColumns.Emplace(Column, TargetColumn);
					if (!OutDataSet.Columns.Contains(TargetColumn)) OutDataSet.Columns.Add(TargetColumn);
				}

				TSet<FString> MatchedKeys;
				for (FDataForgeRow& PrimaryRow : OutDataSet.Rows)
				{
					const FString Key = PrimaryRow.Values.FindRef(Source.Inputs[0].JoinColumn);
					const FDataForgeRow* const* ForeignRow = ForeignRows.Find(Key);
					if (!ForeignRow)
					{
						AddMultiSourceDiagnostic(OutDiagnostics, EDataForgeSeverity::Warning, TEXT("DFMS06"),
							FString::Printf(TEXT("Input %d has no row for primary key '%s'."), InputIndex + 1, *Key), InputConfig.JoinColumn);
						for (const TPair<FName, FName>& Columns : JoinedColumns) PrimaryRow.Values.FindOrAdd(Columns.Value);
						continue;
					}
					MatchedKeys.Add(Key);
					for (const TPair<FName, FName>& Columns : JoinedColumns)
					{
						const FString Value = (*ForeignRow)->Values.FindRef(Columns.Key);
						if (const FString* Existing = PrimaryRow.Values.Find(Columns.Value); Existing && !Existing->IsEmpty() && *Existing != Value)
						{
							AddMultiSourceDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DFMS07"),
								FString::Printf(TEXT("Input %d column '%s' conflicts for key '%s'. Add a Column Prefix or align the values."), InputIndex + 1, *Columns.Value.ToString(), *Key), Columns.Value);
						}
						else
						{
							PrimaryRow.Values.Add(Columns.Value, Value);
						}
					}
				}
				for (const TPair<FString, const FDataForgeRow*>& Pair : ForeignRows)
				{
					if (!MatchedKeys.Contains(Pair.Key))
					{
						AddMultiSourceDiagnostic(OutDiagnostics, EDataForgeSeverity::Warning, TEXT("DFMS08"),
							FString::Printf(TEXT("Input %d key '%s' has no matching primary row and was ignored."), InputIndex + 1, *Pair.Key), InputConfig.JoinColumn);
					}
				}
			}

			OutDataSet.SourceRevision = FMD5::HashAnsiString(*CombinedRevision);
			return !OutDiagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic) { return Diagnostic.Severity == EDataForgeSeverity::Error; });
		}
	};
}

void FChimeraEditorModule::StartupModule()
{
	FDataForgeSourceAdapterRegistry::Get().Register(MakeShared<FGoogleSheetCacheDataForgeAdapter>());
	FDataForgeSourceAdapterRegistry::Get().Register(MakeShared<FMultiSourceDataForgeAdapter>());
	GoogleSheetCacheUpdatedHandle = UGoogleSheetConfig::OnCacheUpdated().AddRaw(this, &FChimeraEditorModule::OnGoogleSheetCacheUpdated);
	DataForgeMcpCommand = DataForgeMcpCommands::Register();
}

void FChimeraEditorModule::ShutdownModule()
{
	DataForgeMcpCommands::Unregister(DataForgeMcpCommand);
	UGoogleSheetConfig::OnCacheUpdated().Remove(GoogleSheetCacheUpdatedHandle);
	FDataForgeSourceAdapterRegistry::Get().Unregister(TEXT("GoogleSheetCache"));
	FDataForgeSourceAdapterRegistry::Get().Unregister(TEXT("MultiSource"));
}

void FChimeraEditorModule::OnGoogleSheetCacheUpdated(UGoogleSheetConfig& Config)
{
	FDataForgeAutoReconciler& Reconciler = FDataForgeAutoReconciler::Get();
	const FDataForgeReconcileBatchResult Batch = Reconciler.ReconcileSourceAssetNow(Config, TEXT("Google Sheet cache updated"));

	if (Batch.IsSuccess())
	{
		Config.LastMessage += FString::Printf(TEXT(" | DataForge auto-applied %d RuleSet(s)."), Batch.AppliedCount);
		UE_LOG(LogDataForge, Display, TEXT("Google Sheet '%s' auto-applied %d DataForge RuleSet(s)."), *Config.GetPathName(), Batch.AppliedCount);
	}
	else
	{
		Config.FetchStatus = EFetchStatus::Failed;
		Config.LastMessage += FString::Printf(TEXT(" | DataForge auto-apply failed after %d success(es): %s"), Batch.AppliedCount, *FString::Join(Batch.Failures, TEXT("; ")));
		UE_LOG(LogDataForge, Error, TEXT("Google Sheet '%s' auto-apply failures: %s"), *Config.GetPathName(), *FString::Join(Batch.Failures, TEXT("; ")));
	}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeMcpSpecParsingTest,
	"DataForge.Integration.McpStructuredSpecParsing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeMcpSpecParsingTest::RunTest(const FString& Parameters)
{
	const FString Json = FString::Printf(TEXT(R"JSON(
{
  "config": "/Game/Data/Body/GS_Body",
  "ruleSet": "/Game/Data/Body/RS_Body",
  "primaryKey": "ID",
  "assetLayoutProfile": {
    "path": "/Game/DataForge/Profiles/ALP_Body",
    "parameters": {"Feature": "Body"}
  },
  "assetRules": [
    {"id":"BodyData","ownership":"Managed","baseFolder":"/Game/Data/Body","assetNamePattern":"DA_{ID}"},
    {"id":"Icon","ownership":"External","baseFolder":"/Game/Data/Texture","assetNamePattern":"T_{ID}"}
  ],
  "generatedOutputs": [
    {"name":"BodyData","type":"DataAsset","class":"%s","assetRule":"BodyData"}
  ],
  "bindings": [
    {"source":"ResolvedAsset","column":"ID","assetRule":"Icon","target":"GeneratedOutput","targetOutput":"BodyData","property":"Icon"},
    {"source":"GeneratedOutput","sourceOutput":"BodyData","target":"DataTableRow","property":"BodyData"}
  ]
}
)JSON"), *UGoogleDataForgeTestDataAsset::StaticClass()->GetPathName());

	FDataForgeGoogleRuleSetRequest Request;
	FString Error;
	TestTrue(TEXT("Structured MCP spec parses"), DataForgeMcpCommands::ParseRequestSpec(Json, Request, Error));
	TestTrue(TEXT("Structured MCP spec has no parse error"), Error.IsEmpty());
	TestEqual(TEXT("Config path is preserved"), Request.GoogleParserPath, FString(TEXT("/Game/Data/Body/GS_Body")));
	TestEqual(TEXT("Asset Layout Profile path is parsed"), Request.AssetLayoutProfilePath, FString(TEXT("/Game/DataForge/Profiles/ALP_Body")));
	TestEqual(TEXT("Asset Layout Profile parameter is parsed"), Request.AssetLayoutParameters.FindRef(TEXT("Feature")), FString(TEXT("Body")));
	TestEqual(TEXT("Both Managed and External Asset Rules are parsed"), Request.AssetRules.Num(), 2);
	TestEqual(TEXT("Generated Output is parsed"), Request.GeneratedOutputs.Num(), 1);
	TestEqual(TEXT("Explicit asset and output bindings are parsed"), Request.Bindings.Num(), 2);
	if (Request.GeneratedOutputs.Num() == 1)
	{
		TestEqual(TEXT("Generated Output class resolves"), Request.GeneratedOutputs[0].AssetClass.Get(), UGoogleDataForgeTestDataAsset::StaticClass());
		TestEqual(TEXT("Generated Output selects its Managed rule"), Request.GeneratedOutputs[0].AssetRuleId, FName(TEXT("BodyData")));
	}
	if (Request.Bindings.Num() == 2)
	{
		TestEqual(TEXT("Resolved asset binding selects Icon rule"), Request.Bindings[0].AssetRuleId, FName(TEXT("Icon")));
		TestTrue(TEXT("Absent sourceOutput does not inherit another JSON field"), Request.Bindings[0].SourceOutput.IsNone());
		TestEqual(TEXT("Generated output binding source is preserved"), Request.Bindings[1].SourceOutput, FName(TEXT("BodyData")));
	}

	FDataForgeGoogleRuleSetRequest InvalidRequest;
	TestFalse(TEXT("Invalid ownership is rejected"), DataForgeMcpCommands::ParseRequestSpec(
		TEXT("{\"config\":\"/Game/GS\",\"ruleSet\":\"/Game/RS\",\"assetRules\":[{\"id\":\"Bad\",\"ownership\":\"Shared\",\"baseFolder\":\"/Game/Data\",\"assetNamePattern\":\"A_{ID}\"}]}"),
		InvalidRequest, Error));
	TestTrue(TEXT("Invalid ownership reports a useful error"), Error.Contains(TEXT("Managed or External")));
	TestFalse(TEXT("Profile path and purpose cannot be combined"), DataForgeMcpCommands::ParseRequestSpec(
		TEXT("{\"config\":\"/Game/GS\",\"ruleSet\":\"/Game/RS\",\"assetLayoutProfile\":{\"path\":\"/Game/ALP\",\"purpose\":\"Character\"}}"),
		InvalidRequest, Error));
	TestTrue(TEXT("Ambiguous Profile selector reports a useful error"), Error.Contains(TEXT("either 'path' or 'purpose'")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeMcpProfileDiscoveryTest,
	"DataForge.Integration.McpAssetLayoutProfileDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeMcpProfileDiscoveryTest::RunTest(const FString& Parameters)
{
	const FName Purpose(TEXT("AutomationMcpAmbiguousLayout"));
	auto CreateProfile = [Purpose](const TCHAR* PackageName, const TCHAR* AssetName, bool bUseTag)
	{
		UPackage* Package = CreatePackage(PackageName);
		UDataForgeAssetLayoutProfile* Profile = NewObject<UDataForgeAssetLayoutProfile>(Package, AssetName, RF_Public | RF_Standalone);
		Profile->ProfileId = FGuid::NewGuid();
		if (bUseTag) Profile->Tags.Add(Purpose);
		else Profile->Purpose = Purpose;
		FAssetRegistryModule::AssetCreated(Profile);
		return Profile;
	};
	UDataForgeAssetLayoutProfile* ProfileA = CreateProfile(TEXT("/Game/DataForgeTests/McpDiscovery/ALP_A"), TEXT("ALP_A"), false);
	UDataForgeAssetLayoutProfile* ProfileB = CreateProfile(TEXT("/Game/DataForgeTests/McpDiscovery/ALP_B"), TEXT("ALP_B"), true);

	FDataForgeGoogleRuleSetRequest Request;
	Request.AssetLayoutPurpose = Purpose;
	UDataForgeAssetLayoutProfile* Resolved = nullptr;
	TArray<FString> Candidates;
	FString Error;
	TestFalse(TEXT("Purpose discovery refuses multiple candidates"), DataForgeMcpCommands::ResolveAssetLayoutProfile(Request, Resolved, Candidates, Error));
	TestEqual(TEXT("Purpose and tag matches are both returned"), Candidates.Num(), 2);
	TestTrue(TEXT("Ambiguous discovery lists candidates"), Error.Contains(TEXT("ambiguous")) && Error.Contains(ProfileA->GetPathName()) && Error.Contains(ProfileB->GetPathName()));

	Request.AssetLayoutPurpose = NAME_None;
	Request.AssetLayoutProfilePath = ProfileA->GetPathName();
	TestTrue(TEXT("Exact Profile path resolves deterministically"), DataForgeMcpCommands::ResolveAssetLayoutProfile(Request, Resolved, Candidates, Error));
	TestTrue(TEXT("Exact Profile object is returned"), Resolved == ProfileA);
	FAssetRegistryModule::AssetDeleted(ProfileA);
	FAssetRegistryModule::AssetDeleted(ProfileB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGoogleSheetCacheDataForgeAdapterTest,
	"DataForge.Integration.GoogleSheetCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGoogleSheetCacheDataForgeAdapterTest::RunTest(const FString& Parameters)
{
	UGoogleSheetConfig* Config = NewObject<UGoogleSheetConfig>(GetTransientPackage(), TEXT("DataForgeGoogleSheetCacheTest"));
	const FString CacheDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GoogleSheetLoader"));
	IFileManager::Get().MakeDirectory(*CacheDirectory, true);
	const FString CacheFile = FPaths::Combine(CacheDirectory,
		FPaths::MakeValidFileName(Config->GetPathName(), TEXT('_')) + TEXT(".json"));
	if (!FFileHelper::SaveStringToFile(TEXT("{\"headers\":[\"Id\",\"Name\"],\"rows\":[[\"1\",\"Sword\"]]}"), *CacheFile))
	{
		AddError(TEXT("Could not create the Google Sheet cache test file."));
		return false;
	}

	FDataForgeSourceConfig Source;
	Source.AdapterId = TEXT("GoogleSheetCache");
	Source.SourceAsset = Config;
	FDataForgeDataSet DataSet;
	TArray<FDataForgeDiagnostic> Diagnostics;
	const TSharedPtr<const IDataForgeSourceAdapter> Adapter = FDataForgeSourceAdapterRegistry::Get().Find(Source.AdapterId);
	TestTrue(TEXT("Google Sheet Cache adapter is registered"), Adapter.IsValid());
	if (Adapter.IsValid())
	{
		TestTrue(TEXT("Normalized Google Sheet cache parses"), Adapter->Fetch(Source, DataSet, Diagnostics));
		TestEqual(TEXT("Cache exposes two columns"), DataSet.Columns.Num(), 2);
		TestEqual(TEXT("Cache exposes one row"), DataSet.Rows.Num(), 1);
		TestFalse(TEXT("Cache revision is populated"), DataSet.SourceRevision.IsEmpty());
	}
	IFileManager::Get().Delete(*CacheFile, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMultiSourceDataForgeAdapterTest,
	"DataForge.Integration.MultiSourceForeignKeyJoin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMultiSourceDataForgeAdapterTest::RunTest(const FString& Parameters)
{
	const FString TestDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
	IFileManager::Get().MakeDirectory(*TestDirectory, true);
	const FString CsvFile = FPaths::Combine(TestDirectory, TEXT("DataForgeMultiPrimary.csv"));
	const FString JsonFile = FPaths::Combine(TestDirectory, TEXT("DataForgeMultiStats.json"));
	if (!FFileHelper::SaveStringToFile(TEXT("Id,Name\n1,Sword\n2,Shield\n"), *CsvFile)
		|| !FFileHelper::SaveStringToFile(TEXT("[{\"ItemId\":\"1\",\"Price\":\"100\"},{\"ItemId\":\"2\",\"Price\":\"80\"}]"), *JsonFile))
	{
		AddError(TEXT("Could not create Multi Source test inputs."));
		return false;
	}

	UGoogleSheetConfig* Config = NewObject<UGoogleSheetConfig>(GetTransientPackage(), TEXT("DataForgeMultiGoogle"));
	const FString CacheDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GoogleSheetLoader"));
	IFileManager::Get().MakeDirectory(*CacheDirectory, true);
	const FString CacheFile = FPaths::Combine(CacheDirectory, FPaths::MakeValidFileName(Config->GetPathName(), TEXT('_')) + TEXT(".json"));
	FFileHelper::SaveStringToFile(TEXT("{\"headers\":[\"Code\",\"Category\"],\"rows\":[[\"1\",\"Weapon\"],[\"2\",\"Armor\"]]}"), *CacheFile);

	FDataForgeSourceConfig Source;
	Source.AdapterId = TEXT("MultiSource");
	FDataForgeSourceInput& Primary = Source.Inputs.AddDefaulted_GetRef();
	Primary.AdapterId = TEXT("Csv");
	Primary.File.FilePath = CsvFile;
	Primary.JoinColumn = TEXT("Id");
	FDataForgeSourceInput& Stats = Source.Inputs.AddDefaulted_GetRef();
	Stats.AdapterId = TEXT("Json");
	Stats.File.FilePath = JsonFile;
	Stats.JoinColumn = TEXT("ItemId");
	FDataForgeSourceInput& Sheet = Source.Inputs.AddDefaulted_GetRef();
	Sheet.AdapterId = TEXT("GoogleSheetCache");
	Sheet.SourceAsset = Config;
	Sheet.JoinColumn = TEXT("Code");

	FDataForgeDataSet DataSet;
	TArray<FDataForgeDiagnostic> Diagnostics;
	const TSharedPtr<const IDataForgeSourceAdapter> Adapter = FDataForgeSourceAdapterRegistry::Get().Find(Source.AdapterId);
	TestTrue(TEXT("Multi Source adapter is registered"), Adapter.IsValid());
	if (Adapter.IsValid())
	{
		TestTrue(TEXT("CSV, JSON, and Google cache join succeeds"), Adapter->Fetch(Source, DataSet, Diagnostics));
		TestEqual(TEXT("Primary source controls row count"), DataSet.Rows.Num(), 2);
		TestTrue(TEXT("JSON column is joined"), DataSet.Columns.Contains(TEXT("Price")));
		TestTrue(TEXT("Google column is joined"), DataSet.Columns.Contains(TEXT("Category")));
		if (DataSet.Rows.Num() == 2)
		{
			TestEqual(TEXT("Foreign JSON value reaches row"), DataSet.Rows[0].Values.FindRef(TEXT("Price")), FString(TEXT("100")));
			TestEqual(TEXT("Foreign Google value reaches row"), DataSet.Rows[1].Values.FindRef(TEXT("Category")), FString(TEXT("Armor")));
		}
	}
	IFileManager::Get().Delete(*CsvFile, false, true);
	IFileManager::Get().Delete(*JsonFile, false, true);
	IFileManager::Get().Delete(*CacheFile, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGoogleSheetAutoApplyConfigurationTest,
	"DataForge.Integration.GoogleSheetAutoApplyConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGoogleSheetAutoApplyConfigurationTest::RunTest(const FString& Parameters)
{
	UGoogleSheetConfig* Config = NewObject<UGoogleSheetConfig>(GetTransientPackage(), TEXT("DataForgeAutoApplyConfig"));
	TestTrue(TEXT("Normalized cache is enabled by default"), Config->bSaveNormalizedJson);
	TestTrue(TEXT("DataForge auto apply is enabled by default"), Config->bAutoApplyDataForge);
	TestNull(TEXT("DataParser is optional by default"), Config->GetActiveParser());

	const FSoftObjectPath ConfigPath(Config);
	UDataForgeRuleSet* DirectRule = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	DirectRule->Source.AdapterId = TEXT("GoogleSheetCache");
	DirectRule->Source.SourceAsset = Config;
	TestTrue(TEXT("Direct Google cache RuleSet is discovered"), ReferencesGoogleSheetConfig(*DirectRule, ConfigPath));

	UDataForgeRuleSet* MultiRule = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	MultiRule->Source.AdapterId = TEXT("MultiSource");
	FDataForgeSourceInput& Input = MultiRule->Source.Inputs.AddDefaulted_GetRef();
	Input.AdapterId = TEXT("GoogleSheetCache");
	Input.SourceAsset = Config;
	TestTrue(TEXT("Multi Source Google input is discovered"), ReferencesGoogleSheetConfig(*MultiRule, ConfigPath));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeMcpGoogleRuleSetBootstrapTest,
	"DataForge.Integration.McpGoogleRuleSetBootstrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeMcpGoogleRuleSetBootstrapTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("MCP bootstrap console command is registered"),
		IConsoleManager::Get().FindConsoleObject(TEXT("DataForge.MCP.CreateRuleSetFromGoogleParser")));
	const FString Root = TEXT("/Game/DataForgeTests/McpBootstrap");
	const FString ConfigPackageName = Root / TEXT("GS_McpBootstrap");
	const FString TablePackageName = Root / TEXT("DT_McpBootstrap");
	const FString RuleSetPackageName = Root / TEXT("RS_McpBootstrap");
	const FString ProfilePackageName = Root / TEXT("ALP_McpBootstrap");

	UPackage* TablePackage = CreatePackage(*TablePackageName);
	UDataTable* TargetTable = NewObject<UDataTable>(TablePackage, TEXT("DT_McpBootstrap"), RF_Public | RF_Standalone);
	TargetTable->RowStruct = FGoogleDataForgeTestRow::StaticStruct();

	UPackage* ConfigPackage = CreatePackage(*ConfigPackageName);
	UGoogleSheetConfig* Config = NewObject<UGoogleSheetConfig>(ConfigPackage, TEXT("GS_McpBootstrap"), RF_Public | RF_Standalone);
	UGoogleDataForgeTestParser* Parser = NewObject<UGoogleDataForgeTestParser>(Config);
	Parser->TargetTable = TargetTable;
	Config->DataParser = Parser;
	UPackage* ProfilePackage = CreatePackage(*ProfilePackageName);
	UDataForgeAssetLayoutProfile* Profile = NewObject<UDataForgeAssetLayoutProfile>(ProfilePackage, TEXT("ALP_McpBootstrap"), RF_Public | RF_Standalone);
	Profile->ProfileId = FGuid::NewGuid();
	Profile->Purpose = TEXT("AutomationMcpBootstrap");
	FDataForgeProfileParameter& LayoutParameter = Profile->Parameters.AddDefaulted_GetRef();
	LayoutParameter.Name = TEXT("GeneratedRoot");
	LayoutParameter.Type = EDataForgeProfileParameterType::ContentPath;
	LayoutParameter.bRequired = true;
	FDataForgeLayoutRoot& LayoutRoot = Profile->Roots.AddDefaulted_GetRef();
	LayoutRoot.RootId = TEXT("GeneratedRoot");
	LayoutRoot.PathPattern = TEXT("${GeneratedRoot}");
	FDataForgeAssetRuleGroupTemplate& LayoutGroup = Profile->Groups.AddDefaulted_GetRef();
	LayoutGroup.TemplateId = FGuid::NewGuid();
	LayoutGroup.GroupId = TEXT("GeneratedAssets");
	LayoutGroup.RootId = LayoutRoot.RootId;
	FDataForgeAssetRuleTemplate& LayoutRule = LayoutGroup.Rules.AddDefaulted_GetRef();
	LayoutRule.TemplateId = FGuid::NewGuid();
	LayoutRule.RuleId = TEXT("DataAsset");
	LayoutRule.Ownership = EDataForgeAssetOwnership::Managed;
	LayoutRule.AssetNamePattern = TEXT("DA_{Id}");
	FAssetRegistryModule::AssetCreated(Profile);

	const FString CacheDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GoogleSheetLoader"));
	IFileManager::Get().MakeDirectory(*CacheDirectory, true);
	const FString CacheFile = FPaths::Combine(CacheDirectory,
		FPaths::MakeValidFileName(Config->GetPathName(), TEXT('_')) + TEXT(".json"));
	TestTrue(TEXT("MCP bootstrap cache is created"), FFileHelper::SaveStringToFile(
		TEXT("{\"headers\":[\"Id\",\"DisplayName\",\"Price\",\"Category\"],\"rows\":[[\"1\",\"MCP Sword\",\"900\",\"Weapon\"]]}"), *CacheFile));

	FDataForgeGoogleRuleSetRequest Request;
	Request.GoogleParserPath = Config->GetPathName();
	Request.RuleSetPath = RuleSetPackageName;
	Request.bSaveAssets = false;
	Request.AssetLayoutProfilePath = Profile->GetPathName();
	Request.AssetLayoutParameters.Add(TEXT("GeneratedRoot"), Root / TEXT("Generated"));
	FDataForgeGeneratedAssetOutputRule& GeneratedOutput = Request.GeneratedOutputs.AddDefaulted_GetRef();
	GeneratedOutput.OutputName = TEXT("DataAsset");
	GeneratedOutput.AssetClass = UGoogleDataForgeTestDataAsset::StaticClass();
	GeneratedOutput.AssetRuleId = TEXT("DataAsset");
	FDataForgeGoogleRuleSetResult Result;
	TestTrue(TEXT("MCP bootstrap command succeeds"), DataForgeMcpCommands::CreateRuleSetFromGoogleParser(Request, Result));
	TestTrue(TEXT("MCP bootstrap reports success"), Result.bSuccess);
	TestEqual(TEXT("MCP bootstrap detects four columns"), Result.DetectedColumnCount, 4);
	TestEqual(TEXT("MCP bootstrap infers row, generated-property, and generated-reference bindings"), Result.BindingCount, 7);
	TestEqual(TEXT("MCP bootstrap reports the exact Profile"), Result.AssetLayoutProfileObjectPath, Profile->GetPathName());

	UDataForgeRuleSet* RuleSet = FindObject<UDataForgeRuleSet>(nullptr, *(RuleSetPackageName + TEXT(".RS_McpBootstrap")));
	TestNotNull(TEXT("MCP bootstrap creates a maintainable RuleSet asset"), RuleSet);
	if (RuleSet)
	{
		TestEqual(TEXT("RuleSet references Google parser config"), RuleSet->Source.SourceAsset.Get(), static_cast<UObject*>(Config));
		TestEqual(TEXT("RuleSet reuses parser TargetTable path"), RuleSet->Output.AssetPath, TablePackageName);
		TestEqual(TEXT("RuleSet infers Id primary key"), RuleSet->Schema.PrimaryKey, FName(TEXT("Id")));
		TestEqual(TEXT("RuleSet records Profile provenance"), RuleSet->ProfileOrigin.ProfileId, Profile->ProfileId);
		TestEqual(TEXT("RuleSet stores materialized Profile parameters"), RuleSet->ProfileOrigin.ParameterValues.FindRef(TEXT("GeneratedRoot")), Root / TEXT("Generated"));
	}
	TestNotNull(TEXT("MCP bootstrap applies the first DataTable row"),
		TargetTable->FindRow<FGoogleDataForgeTestRow>(TEXT("1"), TEXT("MCP bootstrap test")));
	UGoogleDataForgeTestDataAsset* GeneratedAsset = FindObject<UGoogleDataForgeTestDataAsset>(nullptr,
		*(Root / TEXT("Generated/DA_1.DA_1")));
	TestNotNull(TEXT("Structured bootstrap creates the requested Managed DA"), GeneratedAsset);
	if (GeneratedAsset)
	{
		TestEqual(TEXT("Exact-name source field reaches the Managed DA"), GeneratedAsset->DisplayName, FString(TEXT("MCP Sword")));
		TestEqual(TEXT("Generated DA receives numeric source data"), GeneratedAsset->Price, 900);
	}
	TestTrue(TEXT("MCP bootstrap enables normalized JSON"), Config->bSaveNormalizedJson);
	TestTrue(TEXT("MCP bootstrap enables DataForge auto apply"), Config->bAutoApplyDataForge);

	IFileManager::Get().Delete(*CacheFile, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGoogleSheetRuleSetEndToEndTest,
	"DataForge.Integration.GoogleSheetRuleSetEndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGoogleSheetRuleSetEndToEndTest::RunTest(const FString& Parameters)
{
	const FString Root = TEXT("/Game/DataForgeTests/GoogleAutomation");
	const FString ConfigPackageName = Root / TEXT("GS_TestConfig");
	const FString RuleSetPackageName = Root / TEXT("RS_GoogleAutomation");
	const FString TablePackageName = Root / TEXT("DT_GoogleAutomation");
	const FString TexturePackageName = Root / TEXT("Textures/T_Icon_A");
	const FString DataAssetPackageName = Root / TEXT("Generated/DA_1");
	const FString PrimaryAssetPackageName = Root / TEXT("Generated/PDA_1");
	const FString NormalizedJson = TEXT("{\"headers\":[\"Id\",\"DisplayName\",\"TextureId\",\"Category\"],\"rows\":[[\"1\",\"Test Sword\",\"Icon_A\",\"Weapon\"]]}");

	UGoogleDataForgeTestParser* Parser = NewObject<UGoogleDataForgeTestParser>(GetTransientPackage());
	FString ParseMessage;
	TestTrue(TEXT("1. Test Google parser accepts normalized sheet JSON"), Parser->Parse(NormalizedJson, ParseMessage));
	TestTrue(TEXT("1. Test Google parser completion callback runs"), Parser->bCompleted);
	TestEqual(TEXT("1. Test Google parser captures one row"), Parser->CapturedRowCount, 1);
	TestEqual(TEXT("1. Test Google parser captures four headers"), Parser->CapturedHeaders.Num(), 4);

	UPackage* ConfigPackage = CreatePackage(*ConfigPackageName);
	UGoogleSheetConfig* Config = NewObject<UGoogleSheetConfig>(ConfigPackage, TEXT("GS_TestConfig"), RF_Public | RF_Standalone);
	Config->DataParser = Parser;
	Config->bSaveNormalizedJson = true;
	Config->bAutoApplyDataForge = true;
	FAssetRegistryModule::AssetCreated(Config);
	const FString CacheDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GoogleSheetLoader"));
	IFileManager::Get().MakeDirectory(*CacheDirectory, true);
	const FString CacheFile = FPaths::Combine(CacheDirectory, FPaths::MakeValidFileName(Config->GetPathName(), TEXT('_')) + TEXT(".json"));
	TestTrue(TEXT("1. Normalized Google cache test file is created"), FFileHelper::SaveStringToFile(NormalizedJson, *CacheFile));

	const FString ForeignJsonFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeGoogleForeign.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ForeignJsonFile), true);
	TestTrue(TEXT("3. Foreign JSON test file is created"), FFileHelper::SaveStringToFile(TEXT("[{\"ItemId\":\"1\",\"Price\":\"1250\"}]"), *ForeignJsonFile));

	UPackage* TexturePackage = CreatePackage(*TexturePackageName);
	UTexture2D* Texture = NewObject<UTexture2D>(TexturePackage, TEXT("T_Icon_A"), RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(Texture);

	UPackage* RuleSetPackage = CreatePackage(*RuleSetPackageName);
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(RuleSetPackage, TEXT("RS_GoogleAutomation"), RF_Public | RF_Standalone);
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.AdapterId = TEXT("MultiSource");
	FDataForgeSourceInput& SheetInput = RuleSet->Source.Inputs.AddDefaulted_GetRef();
	SheetInput.AdapterId = TEXT("GoogleSheetCache");
	SheetInput.SourceAsset = Config;
	SheetInput.JoinColumn = TEXT("Id");
	FDataForgeSourceInput& ForeignInput = RuleSet->Source.Inputs.AddDefaulted_GetRef();
	ForeignInput.AdapterId = TEXT("Json");
	ForeignInput.File.FilePath = ForeignJsonFile;
	ForeignInput.JoinColumn = TEXT("ItemId");
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Schema.RequiredColumns = { TEXT("Id"), TEXT("DisplayName"), TEXT("TextureId"), TEXT("Category"), TEXT("Price") };
	RuleSet->Schema.bWarnOnUnmappedColumns = false;
	RuleSet->Output.RowStruct = FGoogleDataForgeTestRow::StaticStruct();
	RuleSet->Output.AssetPath = TablePackageName;
	RuleSet->Output.bSaveAfterApply = false;

	FDataForgeAssetRule& TextureRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	TextureRule.RuleId = TEXT("Texture");
	TextureRule.Ownership = EDataForgeAssetOwnership::External;
	TextureRule.BaseFolder = Root / TEXT("Textures");
	TextureRule.AssetNamePattern = TEXT("T_{TextureId}");
	FDataForgeAssetRule& DataRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	DataRule.RuleId = TEXT("DataAsset");
	DataRule.Ownership = EDataForgeAssetOwnership::Managed;
	DataRule.BaseFolder = Root / TEXT("Generated");
	DataRule.AssetNamePattern = TEXT("DA_{Id}");
	FDataForgeAssetRule& PrimaryRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	PrimaryRule.RuleId = TEXT("PrimaryAsset");
	PrimaryRule.Ownership = EDataForgeAssetOwnership::Managed;
	PrimaryRule.BaseFolder = Root / TEXT("Generated");
	PrimaryRule.AssetNamePattern = TEXT("PDA_{Id}");

	FDataForgeGeneratedAssetOutputRule& DataOutput = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	DataOutput.OutputName = TEXT("da");
	DataOutput.Type = EDataForgeGeneratedAssetType::DataAsset;
	DataOutput.AssetClass = UGoogleDataForgeTestDataAsset::StaticClass();
	DataOutput.AssetRuleId = DataRule.RuleId;
	FDataForgeGeneratedAssetOutputRule& PrimaryOutput = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	PrimaryOutput.OutputName = TEXT("pda");
	PrimaryOutput.Type = EDataForgeGeneratedAssetType::PrimaryDataAsset;
	PrimaryOutput.AssetClass = UGoogleDataForgeTestPrimaryAsset::StaticClass();
	PrimaryOutput.AssetRuleId = PrimaryRule.RuleId;

	const auto AddBinding = [RuleSet](EDataForgeBindingSource Source, FName SourceColumn, FName SourceOutput,
		EDataForgeBindingTarget Target, FName TargetOutput, const TCHAR* TargetProperty, FName AssetRuleId = NAME_None)
	{
		FDataForgeBindingRule& Binding = RuleSet->Bindings.AddDefaulted_GetRef();
		Binding.Source = Source;
		Binding.SourceColumn = SourceColumn;
		Binding.SourceOutput = SourceOutput;
		Binding.Target = Target;
		Binding.TargetOutput = TargetOutput;
		Binding.TargetProperty = TargetProperty;
		Binding.AssetRuleId = AssetRuleId;
	};
	AddBinding(EDataForgeBindingSource::SourceValue, TEXT("DisplayName"), NAME_None, EDataForgeBindingTarget::DataTableRow, NAME_None, TEXT("DisplayName"));
	AddBinding(EDataForgeBindingSource::SourceValue, TEXT("Price"), NAME_None, EDataForgeBindingTarget::DataTableRow, NAME_None, TEXT("Price"));
	AddBinding(EDataForgeBindingSource::SourceValue, TEXT("Category"), NAME_None, EDataForgeBindingTarget::DataTableRow, NAME_None, TEXT("Category"));
	AddBinding(EDataForgeBindingSource::SourceValue, TEXT("DisplayName"), NAME_None, EDataForgeBindingTarget::GeneratedOutput, TEXT("da"), TEXT("DisplayName"));
	AddBinding(EDataForgeBindingSource::ResolvedAsset, TEXT("TextureId"), NAME_None, EDataForgeBindingTarget::GeneratedOutput, TEXT("da"), TEXT("Icon"), TEXT("Texture"));
	AddBinding(EDataForgeBindingSource::SourceValue, TEXT("DisplayName"), NAME_None, EDataForgeBindingTarget::GeneratedOutput, TEXT("pda"), TEXT("DisplayName"));
	AddBinding(EDataForgeBindingSource::ResolvedAsset, TEXT("TextureId"), NAME_None, EDataForgeBindingTarget::GeneratedOutput, TEXT("pda"), TEXT("Icon"), TEXT("Texture"));
	AddBinding(EDataForgeBindingSource::GeneratedOutput, NAME_None, TEXT("da"), EDataForgeBindingTarget::DataTableRow, NAME_None, TEXT("DataAsset"));
	AddBinding(EDataForgeBindingSource::GeneratedOutput, NAME_None, TEXT("pda"), EDataForgeBindingTarget::DataTableRow, NAME_None, TEXT("PrimaryAsset"));
	FAssetRegistryModule::AssetCreated(RuleSet);

	TestTrue(TEXT("2. RuleSet references the Google parser config through Multi Source"), ReferencesGoogleSheetConfig(*RuleSet, FSoftObjectPath(Config)));
	UGoogleSheetConfig::OnCacheUpdated().Broadcast(*Config);

	UDataTable* Table = FindObject<UDataTable>(nullptr, *(TablePackageName + TEXT(".DT_GoogleAutomation")));
	TestNotNull(TEXT("3. Automatic update creates the joined DataTable"), Table);
	if (Table)
	{
		const FGoogleDataForgeTestRow* Row = Table->FindRow<FGoogleDataForgeTestRow>(TEXT("1"), TEXT("Google integration test"));
		TestNotNull(TEXT("3. Joined DataTable contains primary row"), Row);
		if (Row)
		{
			TestEqual(TEXT("3. Foreign-key JSON Price is applied"), Row->Price, 1250);
			TestEqual(TEXT("3. Google Category is applied"), Row->Category, FString(TEXT("Weapon")));
			TestEqual(TEXT("4. DataTable references generated DA"), Row->DataAsset.ToSoftObjectPath().ToString(), DataAssetPackageName + TEXT(".DA_1"));
			TestEqual(TEXT("4. DataTable references generated PDA"), Row->PrimaryAsset.ToSoftObjectPath().ToString(), PrimaryAssetPackageName + TEXT(".PDA_1"));
		}
	}

	UGoogleDataForgeTestDataAsset* DataAsset = FindObject<UGoogleDataForgeTestDataAsset>(nullptr, *(DataAssetPackageName + TEXT(".DA_1")));
	UGoogleDataForgeTestPrimaryAsset* PrimaryAsset = FindObject<UGoogleDataForgeTestPrimaryAsset>(nullptr, *(PrimaryAssetPackageName + TEXT(".PDA_1")));
	TestNotNull(TEXT("4. DA is automatically generated"), DataAsset);
	TestNotNull(TEXT("4. PDA is automatically generated"), PrimaryAsset);
	if (DataAsset)
	{
		TestEqual(TEXT("4. DA receives Google display name"), DataAsset->DisplayName, FString(TEXT("Test Sword")));
		TestEqual(TEXT("5. DA receives resolved test texture"), DataAsset->Icon.Get(), Texture);
	}
	if (PrimaryAsset)
	{
		TestEqual(TEXT("4. PDA receives Google display name"), PrimaryAsset->DisplayName, FString(TEXT("Test Sword")));
		TestEqual(TEXT("5. PDA receives resolved test texture"), PrimaryAsset->Icon.Get(), Texture);
	}
	TestEqual(TEXT("Automatic apply reports success"), Config->FetchStatus, EFetchStatus::None);
	TestTrue(TEXT("Automatic apply status contains applied RuleSet count"), Config->LastMessage.Contains(TEXT("auto-applied 1 RuleSet")));

	IFileManager::Get().Delete(*CacheFile, false, true);
	IFileManager::Get().Delete(*ForeignJsonFile, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgePersistentCsvExampleTest,
	"DataForge.Examples.PersistentCsvAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgePersistentCsvExampleTest::RunTest(const FString& Parameters)
{
	const FString Root = TEXT("/Game/DataForgeExamples");
	const FString ItemsFile = TEXT("Content/DataForgeExamples/Source/Items.csv");
	const FString PricesFile = TEXT("Content/DataForgeExamples/Source/Prices.csv");
	const FString RuleSetPackageName = Root / TEXT("Rules/RS_CsvItemExample");
	const FString TablePackageName = Root / TEXT("Output/DT_CsvItems");

	const auto SaveAsset = [this](UObject* Asset, const TCHAR* Label)
	{
		if (!Asset)
		{
			AddError(FString::Printf(TEXT("%s is null."), Label));
			return false;
		}
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, SaveArgs);
		TestTrue(FString::Printf(TEXT("%s is saved under Content"), Label), bSaved);
		return bSaved;
	};

	const auto CreateExampleTexture = [&SaveAsset](const FString& PackageName, const FName AssetName, const FColor Color)
	{
		const FString ObjectPath = PackageName + TEXT(".") + AssetName.ToString();
		UTexture2D* Texture = FindObject<UTexture2D>(nullptr, *ObjectPath);
		if (!Texture && FPackageName::DoesPackageExist(PackageName))
		{
			Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
		}
		bool bCreated = false;
		if (!Texture)
		{
			UPackage* Package = CreatePackage(*PackageName);
			Texture = NewObject<UTexture2D>(Package, AssetName, RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Texture);
			bCreated = true;
		}
		if (!bCreated)
		{
			return Texture;
		}

		constexpr int32 Size = 8;
		TArray<uint8> Pixels;
		Pixels.SetNumUninitialized(Size * Size * 4);
		for (int32 PixelIndex = 0; PixelIndex < Size * Size; ++PixelIndex)
		{
			const bool bBright = ((PixelIndex % Size) + (PixelIndex / Size)) % 2 == 0;
			const FColor Pixel = bBright ? Color : Color.ReinterpretAsLinear().Desaturate(0.45f).ToFColor(true);
			Pixels[PixelIndex * 4 + 0] = Pixel.B;
			Pixels[PixelIndex * 4 + 1] = Pixel.G;
			Pixels[PixelIndex * 4 + 2] = Pixel.R;
			Pixels[PixelIndex * 4 + 3] = Pixel.A;
		}
		Texture->Source.Init(Size, Size, 1, 1, TSF_BGRA8, Pixels.GetData());
		Texture->SRGB = true;
		Texture->CompressionSettings = TC_EditorIcon;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->PostEditChange();
		Texture->MarkPackageDirty();
		SaveAsset(Texture, bCreated ? TEXT("Example texture") : TEXT("Updated example texture"));
		return Texture;
	};

	UTexture2D* SwordTexture = CreateExampleTexture(
		Root / TEXT("Textures/Weapons/T_Sword"), TEXT("T_Sword"), FColor(40, 130, 255));
	UTexture2D* PotionTexture = CreateExampleTexture(
		Root / TEXT("Textures/Consumables/T_Potion"), TEXT("T_Potion"), FColor(210, 45, 170));
	TestNotNull(TEXT("Path example sword texture exists"), SwordTexture);
	TestNotNull(TEXT("Path example potion texture exists"), PotionTexture);

	const FString RuleSetObjectPath = RuleSetPackageName + TEXT(".RS_CsvItemExample");
	UDataForgeRuleSet* RuleSet = FindObject<UDataForgeRuleSet>(nullptr, *RuleSetObjectPath);
	if (!RuleSet && FPackageName::DoesPackageExist(RuleSetPackageName))
	{
		RuleSet = LoadObject<UDataForgeRuleSet>(nullptr, *RuleSetObjectPath);
	}
	const bool bRuleSetCreated = !RuleSet;
	if (!RuleSet)
	{
		UPackage* Package = CreatePackage(*RuleSetPackageName);
		RuleSet = NewObject<UDataForgeRuleSet>(Package, TEXT("RS_CsvItemExample"), RF_Public | RF_Standalone | RF_Transactional);
                FAssetRegistryModule::AssetCreated(RuleSet);
        }
        if (bRuleSetCreated) {
          RuleSet->Modify();
          if (!RuleSet->RuleSetId.IsValid())
            RuleSet->RuleSetId = FGuid::NewGuid();
          RuleSet->Source = FDataForgeSourceConfig();
          RuleSet->Source.AdapterId = TEXT("MultiSource");
          FDataForgeSourceInput &ItemsInput =
              RuleSet->Source.Inputs.AddDefaulted_GetRef();
          ItemsInput.AdapterId = TEXT("Csv");
          ItemsInput.File.FilePath = ItemsFile;
          ItemsInput.JoinColumn = TEXT("Id");
          FDataForgeSourceInput &PricesInput =
              RuleSet->Source.Inputs.AddDefaulted_GetRef();
          PricesInput.AdapterId = TEXT("Csv");
          PricesInput.File.FilePath = PricesFile;
          PricesInput.JoinColumn = TEXT("ItemId");
          RuleSet->Schema.PrimaryKey = TEXT("Id");
          RuleSet->Schema.RequiredColumns = {
              TEXT("Id"),       TEXT("DisplayName"), TEXT("TextureId"),
              TEXT("Category"), TEXT("Price"),       TEXT("Rarity")};
          RuleSet->Schema.bWarnOnUnmappedColumns = false;
          RuleSet->Output.RowStruct = FGoogleDataForgeTestRow::StaticStruct();
          RuleSet->Output.AssetPath = TablePackageName;
          RuleSet->Output.bCreateIfMissing = true;
          RuleSet->Output.bSaveAfterApply = true;
          RuleSet->AssetRules.Reset();
          RuleSet->GeneratedOutputs.Reset();
          RuleSet->Bindings.Reset();

          FDataForgeAssetRule &TextureRule =
              RuleSet->AssetRules.AddDefaulted_GetRef();
          TextureRule.RuleId = TEXT("Texture");
          TextureRule.Ownership = EDataForgeAssetOwnership::External;
          TextureRule.BaseFolder = Root / TEXT("Textures");
          TextureRule.SubfolderPattern = TEXT("{Category}");
          TextureRule.AssetNamePattern = TEXT("T_{TextureId}");
          FDataForgeAssetRule &DataRule =
              RuleSet->AssetRules.AddDefaulted_GetRef();
          DataRule.RuleId = TEXT("DataAsset");
          DataRule.Ownership = EDataForgeAssetOwnership::Managed;
          DataRule.BaseFolder = Root / TEXT("Generated");
          DataRule.SubfolderPattern = TEXT("{Category}");
          DataRule.AssetNamePattern = TEXT("DA_{Id}");
          FDataForgeAssetRule &PrimaryRule =
              RuleSet->AssetRules.AddDefaulted_GetRef();
          PrimaryRule.RuleId = TEXT("PrimaryAsset");
          PrimaryRule.Ownership = EDataForgeAssetOwnership::Managed;
          PrimaryRule.BaseFolder = Root / TEXT("Generated");
          PrimaryRule.SubfolderPattern = TEXT("{Category}");
          PrimaryRule.AssetNamePattern = TEXT("PDA_{Id}");

          FDataForgeGeneratedAssetOutputRule &DataOutput =
              RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
          DataOutput.OutputName = TEXT("da");
          DataOutput.Type = EDataForgeGeneratedAssetType::DataAsset;
          DataOutput.AssetClass = UGoogleDataForgeTestDataAsset::StaticClass();
          DataOutput.AssetRuleId = DataRule.RuleId;
          FDataForgeGeneratedAssetOutputRule &PrimaryOutput =
              RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
          PrimaryOutput.OutputName = TEXT("pda");
          PrimaryOutput.Type = EDataForgeGeneratedAssetType::PrimaryDataAsset;
          PrimaryOutput.AssetClass =
              UGoogleDataForgeTestPrimaryAsset::StaticClass();
          PrimaryOutput.AssetRuleId = PrimaryRule.RuleId;

          const auto AddBinding =
              [RuleSet](EDataForgeBindingSource Source, FName SourceColumn,
                        FName SourceOutput, EDataForgeBindingTarget Target,
                        FName TargetOutput, const TCHAR *TargetProperty,
                        FName AssetRuleId = NAME_None) {
                FDataForgeBindingRule &Binding =
                    RuleSet->Bindings.AddDefaulted_GetRef();
                Binding.Source = Source;
                Binding.SourceColumn = SourceColumn;
                Binding.SourceOutput = SourceOutput;
                Binding.Target = Target;
                Binding.TargetOutput = TargetOutput;
                Binding.TargetProperty = TargetProperty;
                Binding.AssetRuleId = AssetRuleId;
              };
          for (const FName Column :
               {FName(TEXT("DisplayName")), FName(TEXT("Price")),
                FName(TEXT("Rarity")), FName(TEXT("Category"))}) {
            AddBinding(EDataForgeBindingSource::SourceValue, Column, NAME_None,
                       EDataForgeBindingTarget::DataTableRow, NAME_None,
                       *Column.ToString());
          }
          for (const FName Output : {FName(TEXT("da")), FName(TEXT("pda"))}) {
            AddBinding(EDataForgeBindingSource::SourceValue,
                       TEXT("DisplayName"), NAME_None,
                       EDataForgeBindingTarget::GeneratedOutput, Output,
                       TEXT("DisplayName"));
            AddBinding(EDataForgeBindingSource::SourceValue, TEXT("Price"),
                       NAME_None, EDataForgeBindingTarget::GeneratedOutput,
                       Output, TEXT("Price"));
            AddBinding(EDataForgeBindingSource::SourceValue, TEXT("Category"),
                       NAME_None, EDataForgeBindingTarget::GeneratedOutput,
                       Output, TEXT("Category"));
            AddBinding(EDataForgeBindingSource::ResolvedAsset,
                       TEXT("TextureId"), NAME_None,
                       EDataForgeBindingTarget::GeneratedOutput, Output,
                       TEXT("Icon"), TEXT("Texture"));
          }
          AddBinding(EDataForgeBindingSource::GeneratedOutput, NAME_None,
                     TEXT("da"), EDataForgeBindingTarget::DataTableRow,
                     NAME_None, TEXT("DataAsset"));
          AddBinding(EDataForgeBindingSource::GeneratedOutput, NAME_None,
                     TEXT("pda"), EDataForgeBindingTarget::DataTableRow,
                     NAME_None, TEXT("PrimaryAsset"));
          RuleSet->MarkPackageDirty();
          SaveAsset(RuleSet, TEXT("CSV example RuleSet"));
        }

        const FDataForgeResult Preview = FDataForgeEditorService::Preview(*RuleSet);
	TestTrue(TEXT("CSV example previews without errors"), Preview.bSuccess);
	const FDataForgeResult Apply = FDataForgeEditorService::Apply(*RuleSet);
	TestTrue(TEXT("CSV example applies and saves generated assets"), Apply.bSuccess);

	UDataTable* Table = FindObject<UDataTable>(nullptr, *(TablePackageName + TEXT(".DT_CsvItems")));
	TestNotNull(TEXT("Joined CSV DataTable exists"), Table);
	if (Table)
	{
		const FGoogleDataForgeTestRow* SwordRow = Table->FindRow<FGoogleDataForgeTestRow>(TEXT("1001"), TEXT("Persistent CSV example"));
		const FGoogleDataForgeTestRow* PotionRow = Table->FindRow<FGoogleDataForgeTestRow>(TEXT("1002"), TEXT("Persistent CSV example"));
		TestNotNull(TEXT("Sword row was generated"), SwordRow);
		TestNotNull(TEXT("Potion row was generated"), PotionRow);
		if (SwordRow)
		{
			TestEqual(TEXT("Foreign-key price was joined"), SwordRow->Price, 1250);
			TestEqual(TEXT("Foreign-key rarity was joined"), SwordRow->Rarity, FString(TEXT("Rare")));
			UGoogleDataForgeTestDataAsset* SwordAsset = Cast<UGoogleDataForgeTestDataAsset>(SwordRow->DataAsset.LoadSynchronous());
			TestNotNull(TEXT("Sword DA was generated"), SwordAsset);
			if (SwordAsset) TestEqual(TEXT("Sword texture was assigned by tokenized path"), SwordAsset->Icon.Get(), SwordTexture);
		}
		if (PotionRow)
		{
			TestEqual(TEXT("Second foreign-key price was joined"), PotionRow->Price, 75);
			UGoogleDataForgeTestPrimaryAsset* PotionAsset = Cast<UGoogleDataForgeTestPrimaryAsset>(PotionRow->PrimaryAsset.LoadSynchronous());
			TestNotNull(TEXT("Potion PDA was generated"), PotionAsset);
			if (PotionAsset) TestEqual(TEXT("Potion texture was assigned by tokenized path"), PotionAsset->Icon.Get(), PotionTexture);
		}
	}

	for (const FString& PackageName : {
		TablePackageName,
		Root / TEXT("Generated/Weapons/DA_1001"), Root / TEXT("Generated/Weapons/PDA_1001"),
		Root / TEXT("Generated/Consumables/DA_1002"), Root / TEXT("Generated/Consumables/PDA_1002") })
	{
		TestTrue(FString::Printf(TEXT("Persistent asset file exists: %s"), *PackageName), FPackageName::DoesPackageExist(PackageName));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgePersistentMultiAssetReferenceExampleTest,
	"DataForge.Examples.PersistentMultiAssetReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgePersistentMultiAssetReferenceExampleTest::RunTest(const FString& Parameters)
{
	const FString Root = TEXT("/Game/DataForgeExamples/MultiAssetRefs");
	const FString SourceFile = TEXT("Content/DataForgeExamples/MultiAssetRefs/Source/Products.csv");
	const FString Definitions = Root / TEXT("Definitions");
	const FString Inventory = Root / TEXT("Inventory");
	const FString RuleSetPackageName = Root / TEXT("Rules/RS_MultiAssetRefs");
	const FString TablePackageName = Root / TEXT("Output/DT_MultiAssetRefs");

	const auto SaveAsset = [this](UObject* Asset, const TCHAR* Label)
	{
		if (!Asset)
		{
			AddError(FString::Printf(TEXT("%s is null."), Label));
			return false;
		}
		Asset->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, SaveArgs);
		TestTrue(FString::Printf(TEXT("%s is saved under Content"), Label), bSaved);
		return bSaved;
	};

	const auto LoadOrCreate = [](UClass* AssetClass, const FString& PackageName, bool& bOutCreated) -> UObject*
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		const FString ObjectPath = PackageName + TEXT(".") + AssetName;
		UObject* Asset = StaticFindObject(AssetClass, nullptr, *ObjectPath);
		if (!Asset && FPackageName::DoesPackageExist(PackageName))
		{
			Asset = StaticLoadObject(AssetClass, nullptr, *ObjectPath);
		}
		bOutCreated = !Asset;
		if (!Asset)
		{
			UPackage* Package = CreatePackage(*PackageName);
			Asset = NewObject<UObject>(Package, AssetClass, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Asset);
		}
		return Asset;
	};

	const auto CreateTexture = [&LoadOrCreate, &SaveAsset](
		const FString& Subject, int32 Numbering, const FColor Color) -> UTexture2D*
	{
		const FString AssetName = FString::Printf(TEXT("T_CM%sTexture_%d"), *Subject, Numbering);
		const FString PackageName = FString::Printf(TEXT("/Game/DataForgeExamples/MultiAssetRefs/Inventory/%s/Texture/%s"), *Subject, *AssetName);
		bool bCreated = false;
		UTexture2D* Texture = Cast<UTexture2D>(LoadOrCreate(UTexture2D::StaticClass(), PackageName, bCreated));
		if (!Texture || !bCreated) return Texture;
		constexpr int32 Size = 8;
		TArray<uint8> Pixels;
		Pixels.SetNumUninitialized(Size * Size * 4);
		for (int32 PixelIndex = 0; PixelIndex < Size * Size; ++PixelIndex)
		{
			const FColor Pixel = ((PixelIndex % Size) + (PixelIndex / Size)) % 2 == 0
				? Color : Color.ReinterpretAsLinear().Desaturate(0.5f).ToFColor(true);
			Pixels[PixelIndex * 4 + 0] = Pixel.B;
			Pixels[PixelIndex * 4 + 1] = Pixel.G;
			Pixels[PixelIndex * 4 + 2] = Pixel.R;
			Pixels[PixelIndex * 4 + 3] = Pixel.A;
		}
		Texture->Source.Init(Size, Size, 1, 1, TSF_BGRA8, Pixels.GetData());
		Texture->SRGB = true;
		Texture->CompressionSettings = TC_EditorIcon;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->PostEditChange();
		SaveAsset(Texture, TEXT("Multi-reference texture"));
		return Texture;
	};

	const auto CreateMaterial = [&LoadOrCreate, &SaveAsset](const FString& Subject, int32 Numbering) -> UMaterial*
	{
		const FString AssetName = FString::Printf(TEXT("M_CM%sMaterial_%d"), *Subject, Numbering);
		const FString PackageName = FString::Printf(TEXT("/Game/DataForgeExamples/MultiAssetRefs/Inventory/%s/Material/%s"), *Subject, *AssetName);
		bool bCreated = false;
		UMaterial* Material = Cast<UMaterial>(LoadOrCreate(UMaterial::StaticClass(), PackageName, bCreated));
		if (Material && bCreated)
		{
			Material->PostEditChange();
			SaveAsset(Material, TEXT("Multi-reference material"));
		}
		return Material;
	};

	TMap<FString, UTexture2D*> Textures;
	Textures.Add(TEXT("Armor1"), CreateTexture(TEXT("Armor"), 1, FColor(45, 120, 220)));
	Textures.Add(TEXT("Armor2"), CreateTexture(TEXT("Armor"), 2, FColor(100, 100, 255)));
	Textures.Add(TEXT("Robot1"), CreateTexture(TEXT("Robot"), 1, FColor(210, 110, 40)));
	Textures.Add(TEXT("Robot2"), CreateTexture(TEXT("Robot"), 2, FColor(80, 80, 80)));
	TMap<FString, UMaterial*> Materials;
	Materials.Add(TEXT("Armor1"), CreateMaterial(TEXT("Armor"), 1));
	Materials.Add(TEXT("Armor2"), CreateMaterial(TEXT("Armor"), 2));
	Materials.Add(TEXT("Robot1"), CreateMaterial(TEXT("Robot"), 1));
	for (const TPair<FString, UTexture2D*>& Pair : Textures) TestNotNull(*FString::Printf(TEXT("Texture %s exists"), *Pair.Key), Pair.Value);
	for (const TPair<FString, UMaterial*>& Pair : Materials) TestNotNull(*FString::Printf(TEXT("Material %s exists"), *Pair.Key), Pair.Value);

	bool bCreated = false;
	UDataForgeNamingPolicy* Policy = Cast<UDataForgeNamingPolicy>(LoadOrCreate(
		UDataForgeNamingPolicy::StaticClass(), Definitions / TEXT("NP_MultiAssetRefs"), bCreated));
	Policy->ProjectPrefix = TEXT("CM");
	Policy->AssetKinds.Reset();
	FDataForgeAssetKindNamingRule& TextureKind = Policy->AssetKinds.AddDefaulted_GetRef();
	TextureKind.AssetKind = TEXT("Texture");
	TextureKind.TypePrefix = TEXT("T");
	TextureKind.FolderName = TEXT("Texture");
	TextureKind.ExpectedAssetClass = UTexture2D::StaticClass();
	FDataForgeAssetKindNamingRule& MaterialKind = Policy->AssetKinds.AddDefaulted_GetRef();
	MaterialKind.AssetKind = TEXT("Material");
	MaterialKind.TypePrefix = TEXT("M");
	MaterialKind.FolderName = TEXT("Material");
	MaterialKind.ExpectedAssetClass = UMaterialInterface::StaticClass();
	SaveAsset(Policy, TEXT("Multi-reference Naming Policy"));

	UDataForgeAssetLayoutRecipe* Recipe = Cast<UDataForgeAssetLayoutRecipe>(LoadOrCreate(
		UDataForgeAssetLayoutRecipe::StaticClass(), Definitions / TEXT("ALR_MultiAssetRefs"), bCreated));
	Recipe->RecipeId = TEXT("MultiAssetRefs");
	Recipe->Domain = TEXT("Example");
	Recipe->NamingPolicy = Policy;
	Recipe->SubjectSource = EDataForgeLayoutSubjectSource::FolderSegment;
	Recipe->SubjectFolderIndex = 0;
	Recipe->KindFolderIndex = 1;
	Recipe->bRequireKindFolderMatch = true;
	SaveAsset(Recipe, TEXT("Multi-reference Layout Recipe"));

	UDataForgeFolderSourceConfig* FolderConfig = Cast<UDataForgeFolderSourceConfig>(LoadOrCreate(
		UDataForgeFolderSourceConfig::StaticClass(), Definitions / TEXT("FSC_MultiAssetRefs"), bCreated));
	FolderConfig->RootFolder = Inventory;
	FolderConfig->bRecursive = true;
	FolderConfig->AllowedAssetKinds = { TEXT("Texture"), TEXT("Material") };
	FolderConfig->ExcludedFolders.Reset();
	FolderConfig->bExcludeDataForgeManagedAssets = true;
	FolderConfig->LayoutRecipe = Recipe;
	SaveAsset(FolderConfig, TEXT("Multi-reference Folder Source"));

	UDataForgeBindingPreset* Preset = Cast<UDataForgeBindingPreset>(LoadOrCreate(
		UDataForgeBindingPreset::StaticClass(), Definitions / TEXT("BP_MultiAssetRefs"), bCreated));
	if (!Preset->PresetId.IsValid()) Preset->PresetId = FGuid::NewGuid();
	Preset->OutputName = TEXT("PrimaryAsset");
	Preset->TargetClass = UCMMultiAssetReferencePrimaryAsset::StaticClass();
	Preset->AssetNamePrefix = TEXT("PDA");
	Preset->RowReferenceProperty = TEXT("PrimaryAsset");
	Preset->Slots.Reset();
	FDataForgeBindingPresetSlot& TextureSlot = Preset->Slots.AddDefaulted_GetRef();
	TextureSlot.SlotId = TEXT("Textures");
	TextureSlot.AssetKind = TEXT("Texture");
	TextureSlot.Role = TEXT("Texture");
	TextureSlot.AssociationSourceId = TEXT("Inventory");
	TextureSlot.SourceKeyColumn = TEXT("Id");
	TextureSlot.TargetProperty = TEXT("Textures");
	TextureSlot.ExpectedAssetClass = UTexture2D::StaticClass();
	TextureSlot.Cardinality = EDataForgeBindingCardinality::Many;
	TextureSlot.Reconcile = EDataForgeBindingReconcileMode::ReplaceManaged;
	TextureSlot.bRequired = true;
	FDataForgeBindingPresetSlot& MaterialSlot = Preset->Slots.AddDefaulted_GetRef();
	MaterialSlot.SlotId = TEXT("Materials");
	MaterialSlot.AssetKind = TEXT("Material");
	MaterialSlot.Role = TEXT("Material");
	MaterialSlot.AssociationSourceId = TEXT("Inventory");
	MaterialSlot.SourceKeyColumn = TEXT("Id");
	MaterialSlot.TargetProperty = TEXT("Materials");
	MaterialSlot.ExpectedAssetClass = UMaterialInterface::StaticClass();
	MaterialSlot.Cardinality = EDataForgeBindingCardinality::Many;
	MaterialSlot.Reconcile = EDataForgeBindingReconcileMode::ReplaceManaged;
	MaterialSlot.bRequired = true;
	SaveAsset(Preset, TEXT("Multi-reference Binding Preset"));

	bool bRuleSetCreated = false;
	UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(LoadOrCreate(
		UDataForgeRuleSet::StaticClass(), RuleSetPackageName, bRuleSetCreated));
	if (!RuleSet) return false;
	if (!RuleSet->RuleSetId.IsValid()) RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source = FDataForgeSourceConfig();
	RuleSet->Source.AdapterId = TEXT("Csv");
	RuleSet->Source.File.FilePath = SourceFile;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Schema.RequiredColumns = { TEXT("Id"), TEXT("DisplayName") };
	RuleSet->Schema.bWarnOnUnmappedColumns = false;
	RuleSet->Output.RowStruct = FCMMultiAssetReferenceRow::StaticStruct();
	RuleSet->Output.AssetPath = TablePackageName;
	RuleSet->Output.bCreateIfMissing = true;
	RuleSet->Output.bSaveAfterApply = true;
	RuleSet->AssetRules.Reset();
	RuleSet->GeneratedOutputs.Reset();
	RuleSet->Bindings.Reset();

	RuleSet->AssociationSources.Reset();
	FDataForgeAssociationSourceRule& Association = RuleSet->AssociationSources.AddDefaulted_GetRef();
	Association.SourceId = TEXT("Inventory");
	Association.Source.AdapterId = TEXT("AssetRegistryFolder");
	Association.Source.SourceAsset = FolderConfig;
	Association.MatchColumn = TEXT("Subject");
	Association.AssetPathColumn = TEXT("ObjectPath");
	Association.AssetKindColumn = TEXT("AssetKind");
	Association.RoleColumn = TEXT("Role");
	RuleSet->BindingPreset = Preset;
	const FDataForgeBindingPresetMaterialization Materialization =
		FDataForgeBindingPresetAuthoring::Materialize(*RuleSet, *Preset, Root / TEXT("Generated"));
	TestTrue(TEXT("Binding Preset materializes the PDA output and slots"), Materialization.bSuccess);

	const auto AddBinding = [RuleSet](
		EDataForgeBindingSource Source, FName SourceColumn, EDataForgeBindingTarget Target,
		FName TargetOutput, const TCHAR* TargetProperty, FName AssetRuleId = NAME_None, FName SourceOutput = NAME_None)
	{
		FDataForgeBindingRule& Binding = RuleSet->Bindings.AddDefaulted_GetRef();
		Binding.Source = Source;
		Binding.SourceColumn = SourceColumn;
		Binding.SourceOutput = SourceOutput;
		Binding.Target = Target;
		Binding.TargetOutput = TargetOutput;
		Binding.TargetProperty = TargetProperty;
		Binding.AssetRuleId = AssetRuleId;
	};
	for (const FName Column : { FName(TEXT("Id")), FName(TEXT("DisplayName")) })
	{
		AddBinding(EDataForgeBindingSource::SourceValue, Column, EDataForgeBindingTarget::DataTableRow, NAME_None, *Column.ToString());
		AddBinding(EDataForgeBindingSource::SourceValue, Column, EDataForgeBindingTarget::GeneratedOutput, TEXT("PrimaryAsset"), *Column.ToString());
	}
	SaveAsset(RuleSet, TEXT("Multi-reference RuleSet"));

	FDataForgeApplyPlan Plan;
	const FDataForgeResult Preview = FDataForgeEditorService::Preview(*RuleSet, &Plan);
	TestTrue(TEXT("Multi-reference example previews without errors"), Preview.bSuccess);
	const FDataForgeResult Apply = FDataForgeEditorService::Apply(*RuleSet);
	TestTrue(TEXT("Multi-reference example applies"), Apply.bSuccess);

	UDataTable* Table = LoadObject<UDataTable>(nullptr, *(TablePackageName + TEXT(".DT_MultiAssetRefs")));
	TestNotNull(TEXT("Multi-reference DataTable exists"), Table);
	if (Table)
	{
		const FCMMultiAssetReferenceRow* Armor = Table->FindRow<FCMMultiAssetReferenceRow>(TEXT("Armor"), TEXT("Multi-reference example"));
		const FCMMultiAssetReferenceRow* Robot = Table->FindRow<FCMMultiAssetReferenceRow>(TEXT("Robot"), TEXT("Multi-reference example"));
		TestNotNull(TEXT("Armor row exists"), Armor);
		TestNotNull(TEXT("Robot row exists"), Robot);
		if (Armor)
		{
			UCMMultiAssetReferencePrimaryAsset* PDA = Armor->PrimaryAsset.LoadSynchronous();
			TestNotNull(TEXT("Armor PDA exists"), PDA);
			if (PDA)
			{
				TestTrue(TEXT("Armor PDA keeps at least the two baseline textures"), PDA->Textures.Num() >= 2);
				TestTrue(TEXT("Armor PDA keeps at least the two baseline materials"), PDA->Materials.Num() >= 2);
				TestTrue(TEXT("Armor PDA inferred its first texture from ID and folders"), PDA->Textures.Contains(Textures.FindChecked(TEXT("Armor1"))));
				TestTrue(TEXT("Armor PDA inferred its second material without a CSV asset id"), PDA->Materials.Contains(Materials.FindChecked(TEXT("Armor2"))));
			}
		}
		if (Robot)
		{
			UCMMultiAssetReferencePrimaryAsset* PDA = Robot->PrimaryAsset.LoadSynchronous();
			TestNotNull(TEXT("Robot PDA exists"), PDA);
			if (PDA)
			{
				TestTrue(TEXT("Robot PDA keeps at least the two baseline textures"), PDA->Textures.Num() >= 2);
				TestTrue(TEXT("Robot PDA keeps at least the baseline material"), PDA->Materials.Num() >= 1);
			}
		}
	}

	for (const FString& PackageName : { TablePackageName, Root / TEXT("Generated/PDA_Armor"), Root / TEXT("Generated/PDA_Robot") })
	{
		TestTrue(FString::Printf(TEXT("Persistent multi-reference asset exists: %s"), *PackageName), FPackageName::DoesPackageExist(PackageName));
	}
	return !HasAnyErrors();
}
#endif

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FChimeraEditorModule, ChimeraEditor)
