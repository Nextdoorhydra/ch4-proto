#include "DataForgeDependencyGraph.h"
#include "DataForgePipeline.h"
#include "DataForgeRuleSet.h"
#include "Tests/DataForgeTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StructOnScope.h"

namespace DataForgeTests
{
	class FParsedDataAdapter final : public IDataForgeSourceAdapter
	{
	public:
		virtual FDataForgeSourceDescriptor Describe() const override
		{
			return { TEXT("AutomationParsedData"), FText::FromString(TEXT("Automation Parsed Data")), TEXT("Test adapter"), FString() };
		}

		virtual bool Probe(
			const FDataForgeSourceConfig& Source,
			FDataForgeDataSet& OutDataSet,
			TArray<FDataForgeDiagnostic>& OutDiagnostics) const override
		{
			return Supply(OutDataSet);
		}

		virtual bool Fetch(
			const FDataForgeSourceConfig& Source,
			FDataForgeDataSet& OutDataSet,
			TArray<FDataForgeDiagnostic>& OutDiagnostics) const override
		{
			return Supply(OutDataSet);
		}

	private:
		static bool Supply(FDataForgeDataSet& OutDataSet)
		{
			OutDataSet = FDataForgeDataSet();
			OutDataSet.Columns = { TEXT("Id"), TEXT("DisplayName") };
			FDataForgeRow& Row = OutDataSet.Rows.AddDefaulted_GetRef();
			Row.SourceRow = 7;
			Row.Values.Add(TEXT("Id"), TEXT("ExternalOne"));
			Row.Values.Add(TEXT("DisplayName"), TEXT("Supplied by parser"));
			OutDataSet.SourceRevision = TEXT("parsed-data-revision");
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeDependencyGraphTest,
	"DataForge.Core.Dependency.ExecutionOrderAndCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeDependencyGraphTest::RunTest(const FString& Parameters)
{
	UDataForgeRuleSet* Tags = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	UDataForgeRuleSet* Items = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	UDataForgeRuleSet* Shop = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	Items->Dependencies.AddDefaulted_GetRef().RuleSet = Tags;
	Shop->Dependencies.AddDefaulted_GetRef().RuleSet = Items;

	const TArray<const UDataForgeRuleSet*> Roots = { Shop };
	TArray<const UDataForgeRuleSet*> Order;
	TArray<FDataForgeDiagnostic> Diagnostics;
	TestTrue(TEXT("Acyclic dependencies produce an execution order"), FDataForgeDependencyGraph::BuildExecutionOrder(Roots, Order, Diagnostics));
	TestEqual(TEXT("Every dependency appears once"), Order.Num(), 3);
	if (Order.Num() == 3)
	{
		TestTrue(TEXT("Transitive prerequisite executes first"), Order[0] == Tags);
		TestTrue(TEXT("Direct prerequisite executes second"), Order[1] == Items);
		TestTrue(TEXT("Root executes last"), Order[2] == Shop);
	}

	Tags->Dependencies.AddDefaulted_GetRef().RuleSet = Shop;
	Order.Reset();
	Diagnostics.Reset();
	TestFalse(TEXT("Dependency cycle is rejected"), FDataForgeDependencyGraph::BuildExecutionOrder(Roots, Order, Diagnostics));
	TestTrue(TEXT("DF1302 cycle diagnostic is emitted"), Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1302") && Diagnostic.Severity == EDataForgeSeverity::Error;
	}));
	TestEqual(TEXT("Failed graph returns no partial execution order"), Order.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeCsvQuotedCellTest,
	"DataForge.Core.Csv.QuotedCells",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeCsvQuotedCellTest::RunTest(const FString& Parameters)
{
	FDataForgeDataSet DataSet;
	TArray<FDataForgeDiagnostic> Diagnostics;
	const bool bParsed = FDataForgeCsvSourceAdapter::Parse(
		TEXT("ItemId,DisplayName,Price\nFireSword,\"Sword, Fire\",1200\n"),
		INDEX_NONE,
		DataSet,
		Diagnostics);

	TestTrue(TEXT("CSV parses successfully"), bParsed);
	TestEqual(TEXT("Three columns are detected"), DataSet.Columns.Num(), 3);
	TestEqual(TEXT("One data row is detected"), DataSet.Rows.Num(), 1);
	if (DataSet.Rows.Num() == 1)
	{
		TestEqual(TEXT("Quoted comma is preserved"), DataSet.Rows[0].Values.FindRef(TEXT("DisplayName")), FString(TEXT("Sword, Fire")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeCsvDuplicateHeaderTest,
	"DataForge.Core.Csv.DuplicateHeader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeCsvDuplicateHeaderTest::RunTest(const FString& Parameters)
{
	FDataForgeDataSet DataSet;
	TArray<FDataForgeDiagnostic> Diagnostics;
	const bool bParsed = FDataForgeCsvSourceAdapter::Parse(
		TEXT("Id,Id\nA,B\n"),
		INDEX_NONE,
		DataSet,
		Diagnostics);

	TestFalse(TEXT("Duplicate headers are rejected"), bParsed);
	TestTrue(TEXT("DF1006 diagnostic is emitted"), Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1006") && Diagnostic.Severity == EDataForgeSeverity::Error;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeJsonObjectArrayTest,
	"DataForge.Core.Json.ObjectArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeJsonObjectArrayTest::RunTest(const FString& Parameters)
{
	FDataForgeDataSet DataSet;
	TArray<FDataForgeDiagnostic> Diagnostics;
	const bool bParsed = FDataForgeJsonSourceAdapter::Parse(
		TEXT("[{\"Id\":\"One\",\"Price\":1200,\"Enabled\":true}]"),
		INDEX_NONE,
		DataSet,
		Diagnostics);

	TestTrue(TEXT("JSON parses successfully"), bParsed);
	TestEqual(TEXT("JSON object fields become canonical columns"), DataSet.Columns.Num(), 3);
	TestEqual(TEXT("JSON creates one canonical row"), DataSet.Rows.Num(), 1);
	if (DataSet.Rows.Num() == 1)
	{
		TestEqual(TEXT("Number is normalized as source text"), DataSet.Rows[0].Values.FindRef(TEXT("Price")), FString(TEXT("1200")));
		TestEqual(TEXT("Boolean is normalized as source text"), DataSet.Rows[0].Values.FindRef(TEXT("Enabled")), FString(TEXT("true")));
	}

	FDataForgeDataSet NormalizedDataSet;
	TArray<FDataForgeDiagnostic> NormalizedDiagnostics;
	TestTrue(
		TEXT("Normalized headers/rows JSON parses successfully"),
		FDataForgeJsonSourceAdapter::Parse(
			TEXT("{\"headers\":[\"Id\",\"DisplayName\"],\"rows\":[[\"One\",\"Normalized\"]]}"),
			INDEX_NONE,
			NormalizedDataSet,
			NormalizedDiagnostics));
	TestEqual(TEXT("Normalized JSON row reaches canonical data"), NormalizedDataSet.Rows.Num(), 1);

	FDataForgeDataSet InvalidDataSet;
	TArray<FDataForgeDiagnostic> InvalidDiagnostics;
	TestFalse(
		TEXT("Non-object JSON array elements are rejected"),
		FDataForgeJsonSourceAdapter::Parse(TEXT("[1]"), INDEX_NONE, InvalidDataSet, InvalidDiagnostics));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeRegisteredParsedDataAdapterTest,
	"DataForge.Core.Source.RegisteredParsedDataAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeRegisteredParsedDataAdapterTest::RunTest(const FString& Parameters)
{
	FDataForgeSourceAdapterRegistry& Registry = FDataForgeSourceAdapterRegistry::Get();
	TestTrue(TEXT("Custom parsed-data adapter registers"), Registry.Register(MakeShared<DataForgeTests::FParsedDataAdapter>()));

	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.AdapterId = TEXT("AutomationParsedData");
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Output.RowStruct = FDataForgeTestRow::StaticStruct();
	RuleSet->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_ParsedDataAdapter");

	FCompiledDataForgeRuleSet Compiled;
	TArray<FDataForgeDiagnostic> Diagnostics;
	const bool bCompiled = FDataForgeCompiler::Compile(*RuleSet, Compiled, Diagnostics);
	TestTrue(TEXT("Compiler consumes canonical data without knowing parser implementation"), bCompiled);
	TestEqual(TEXT("External parser row reaches compiler"), Compiled.DataSet.Rows.Num(), 1);
	TestEqual(TEXT("External parser revision is preserved"), Compiled.DataSet.SourceRevision, FString(TEXT("parsed-data-revision")));

	Registry.Unregister(TEXT("AutomationParsedData"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeGeneratedAssetPlanTest,
	"DataForge.Core.GeneratedAsset.PlanAndBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeGeneratedAssetPlanTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeGeneratedAssetPlan.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("ItemId,DisplayName\nFireSword,Flame Sword\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the DataForge test CSV."));
		return false;
	}

	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.File.FilePath = CsvFilename;
	RuleSet->Schema.PrimaryKey = TEXT("ItemId");
	RuleSet->Output.RowStruct = FDataForgeTestRow::StaticStruct();
	RuleSet->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_GeneratedAssetPlan");
	RuleSet->Output.bSaveAfterApply = false;

	FDataForgeAssetRule& ManagedRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	ManagedRule.RuleId = TEXT("ItemData");
	ManagedRule.Ownership = EDataForgeAssetOwnership::Managed;
	ManagedRule.BaseFolder = TEXT("/Game/DataForgeTests/Items");
	ManagedRule.AssetNamePattern = TEXT("DA_{ItemId}");

	FDataForgeGeneratedAssetOutputRule& GeneratedOutput = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	GeneratedOutput.OutputName = TEXT("data");
	GeneratedOutput.Type = EDataForgeGeneratedAssetType::PrimaryDataAsset;
	GeneratedOutput.AssetClass = UDataForgeTestPrimaryAsset::StaticClass();
	GeneratedOutput.AssetRuleId = ManagedRule.RuleId;

	FDataForgeBindingRule& AssetBinding = RuleSet->Bindings.AddDefaulted_GetRef();
	AssetBinding.Source = EDataForgeBindingSource::SourceValue;
	AssetBinding.SourceColumn = TEXT("DisplayName");
	AssetBinding.Target = EDataForgeBindingTarget::GeneratedOutput;
	AssetBinding.TargetOutput = GeneratedOutput.OutputName;
	AssetBinding.TargetProperty = TEXT("DisplayName");

	FDataForgeBindingRule& RowBinding = RuleSet->Bindings.AddDefaulted_GetRef();
	RowBinding.Source = EDataForgeBindingSource::GeneratedOutput;
	RowBinding.SourceOutput = GeneratedOutput.OutputName;
	RowBinding.Target = EDataForgeBindingTarget::DataTableRow;
	RowBinding.TargetProperty = TEXT("DataAsset");

	FCompiledDataForgeRuleSet Compiled;
	TArray<FDataForgeDiagnostic> Diagnostics;
	const bool bCompiled = FDataForgeCompiler::Compile(*RuleSet, Compiled, Diagnostics);
	TestTrue(TEXT("Generated output RuleSet compiles"), bCompiled);

	const FDataForgeApplyPlan Plan = FDataForgeCompiler::BuildPlan(Compiled);
	TestFalse(TEXT("Generated output plan has no errors"), Plan.HasErrors());
	TestEqual(TEXT("One managed asset is planned"), Plan.AssetCreateCount, 1);
	TestEqual(TEXT("One DataTable row is planned"), Plan.CreateCount, 1);
	if (Plan.ManagedAssets.Num() == 1)
	{
		TestEqual(TEXT("Managed asset path is deterministic"), Plan.ManagedAssets[0].ObjectPath, FString(TEXT("/Game/DataForgeTests/Items/DA_FireSword.DA_FireSword")));
		UDataForgeTestPrimaryAsset* Materialized = NewObject<UDataForgeTestPrimaryAsset>(GetTransientPackage());
		TArray<FDataForgeDiagnostic> ApplyDiagnostics;
		TestTrue(TEXT("Planned DataAsset properties materialize"), FDataForgeCompiler::ApplyPlannedProperties(*Materialized, Plan.ManagedAssets[0], ApplyDiagnostics));
		TestEqual(TEXT("CSV value is bound to generated DataAsset"), Materialized->DisplayName, FString(TEXT("Flame Sword")));
	}
	if (Plan.Rows.Num() == 1 && Plan.Rows[0].DesiredData.IsValid())
	{
		const FDataForgeTestRow* DesiredRow = reinterpret_cast<const FDataForgeTestRow*>(Plan.Rows[0].DesiredData->GetStructMemory());
		TestEqual(TEXT("Generated output is bound to row soft reference"), DesiredRow->DataAsset.ToSoftObjectPath().ToString(), FString(TEXT("/Game/DataForgeTests/Items/DA_FireSword.DA_FireSword")));
	}

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgePrimaryAssetClassValidationTest,
	"DataForge.Core.GeneratedAsset.PrimaryAssetClassValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgePrimaryAssetClassValidationTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgePrimaryAssetValidation.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("Id\nOne\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the DataForge validation CSV."));
		return false;
	}

	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.File.FilePath = CsvFilename;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Output.RowStruct = FDataForgeTestRow::StaticStruct();
	RuleSet->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_InvalidPrimaryAsset");

	FDataForgeAssetRule& ManagedRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	ManagedRule.RuleId = TEXT("Managed");
	ManagedRule.Ownership = EDataForgeAssetOwnership::Managed;
	ManagedRule.BaseFolder = TEXT("/Game/DataForgeTests/Invalid");
	ManagedRule.AssetNamePattern = TEXT("DA_{Id}");

	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("data");
	Output.Type = EDataForgeGeneratedAssetType::PrimaryDataAsset;
	Output.AssetClass = UDataForgeTestAsset::StaticClass();
	Output.AssetRuleId = ManagedRule.RuleId;

	FCompiledDataForgeRuleSet Compiled;
	TArray<FDataForgeDiagnostic> Diagnostics;
	TestFalse(TEXT("PrimaryDataAsset output rejects a plain DataAsset class"), FDataForgeCompiler::Compile(*RuleSet, Compiled, Diagnostics));
	TestTrue(TEXT("DF1117 diagnostic is emitted"), Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1117") && Diagnostic.Severity == EDataForgeSeverity::Error;
	}));

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

#endif
