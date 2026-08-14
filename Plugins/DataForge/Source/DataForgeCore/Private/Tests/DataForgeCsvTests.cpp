#include "DataForgeDependencyGraph.h"
#include "DataForgeBindingPreset.h"
#include "DataForgePipeline.h"
#include "DataForgeRuleSet.h"
#include "Tests/DataForgeTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StructOnScope.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

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
			return Supply(Source, OutDataSet);
		}

		virtual bool Fetch(
			const FDataForgeSourceConfig& Source,
			FDataForgeDataSet& OutDataSet,
			TArray<FDataForgeDiagnostic>& OutDiagnostics) const override
		{
			return Supply(Source, OutDataSet);
		}

	private:
		static bool Supply(const FDataForgeSourceConfig& Source, FDataForgeDataSet& OutDataSet)
		{
			OutDataSet = FDataForgeDataSet();
			if (Source.Parameters.FindRef(TEXT("Mode")) == TEXT("Association"))
			{
				OutDataSet.Columns = { TEXT("Subject"), TEXT("ObjectPath"), TEXT("AssetKind"), TEXT("Role") };
				for (int32 Index = 1; Index <= 3; ++Index)
				{
					const FString Path = Source.Parameters.FindRef(FName(*FString::Printf(TEXT("Path%d"), Index)));
					if (Path.IsEmpty()) continue;
					FDataForgeRow& Row = OutDataSet.Rows.AddDefaulted_GetRef();
					Row.SourceRow = Index;
					Row.Values.Add(TEXT("Subject"), TEXT("ExternalOne"));
					Row.Values.Add(TEXT("ObjectPath"), Path);
					Row.Values.Add(TEXT("AssetKind"), TEXT("TestAsset"));
					Row.Values.Add(TEXT("Role"), TEXT("Visual"));
				}
				OutDataSet.SourceRevision = Source.Parameters.FindRef(TEXT("Revision"));
				return true;
			}
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
	FDataForgeCsvFilePickerPathTest,
	"DataForge.Core.Csv.FilePickerProjectPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeCsvFilePickerPathTest::RunTest(const FString& Parameters)
{
	FDataForgeSourceConfig Source;
	Source.File.FilePath = FPaths::ProjectDir() / TEXT("Content/DataForgeExamples/Source/Items.csv");
	const FString PickerResolved = FPaths::ConvertRelativePathToFull(Source.File.FilePath);
	TestTrue(FString::Printf(TEXT("File-picker path resolves from the Unreal base directory: %s"), *PickerResolved),
		FPaths::FileExists(PickerResolved));

	FDataForgeDataSet DataSet;
	TArray<FDataForgeDiagnostic> Diagnostics;
	FDataForgeCsvSourceAdapter Adapter;
	const bool bFetched = Adapter.Fetch(Source, DataSet, Diagnostics);
	const FString DiagnosticText = FString::JoinBy(Diagnostics, TEXT(" | "), [](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code + TEXT(": ") + Diagnostic.Message;
	});
	TestTrue(FString::Printf(TEXT("CSV adapter reads file-picker path. configured='%s' direct='%s' diagnostics='%s'"),
		*Source.File.FilePath, *PickerResolved, *DiagnosticText), bFetched);
	TestEqual(TEXT("Example CSV rows are loaded"), DataSet.Rows.Num(), 2);
	TestFalse(TEXT("DF1002 is not emitted"), Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1002");
	}));
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
	FDataForgeAssociationManifestMergeTest,
	"DataForge.Core.AssociationManifest.MergeByKeyConvergence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssociationManifestMergeTest::RunTest(const FString& Parameters)
{
	FDataForgeSourceAdapterRegistry& Registry = FDataForgeSourceAdapterRegistry::Get();
	Registry.Unregister(TEXT("AutomationParsedData"));
	Registry.Register(MakeShared<DataForgeTests::FParsedDataAdapter>());
	const FString TestRoot = TEXT("/Game/DataForgeTests/Association_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	auto CreateAsset = [this](const FString& PackageName, UClass* Class) -> UDataAsset*
	{
		UPackage* Package = CreatePackage(*PackageName);
		UDataAsset* Asset = NewObject<UDataAsset>(Package, Class, *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone | RF_Transient);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	};

	UDataForgeTestAsset* OldManaged = CastChecked<UDataForgeTestAsset>(CreateAsset(TestRoot + TEXT("/A_OldManaged"), UDataForgeTestAsset::StaticClass()));
	UDataForgeTestAsset* NewManaged = CastChecked<UDataForgeTestAsset>(CreateAsset(TestRoot + TEXT("/A_NewManaged"), UDataForgeTestAsset::StaticClass()));
	UDataForgeTestAsset* Manual = CastChecked<UDataForgeTestAsset>(CreateAsset(TestRoot + TEXT("/A_Manual"), UDataForgeTestAsset::StaticClass()));

	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.AdapterId = TEXT("AutomationParsedData");
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Output.RowStruct = FDataForgeTestRow::StaticStruct();
	RuleSet->Output.AssetPath = TestRoot + TEXT("/DT_Association");
	RuleSet->Output.bSaveAfterApply = false;

	FDataForgeAssetRule& ManagedRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	ManagedRule.RuleId = TEXT("Data_Managed");
	ManagedRule.Ownership = EDataForgeAssetOwnership::Managed;
	ManagedRule.BaseFolder = TestRoot + TEXT("/Generated");
	ManagedRule.AssetNamePattern = TEXT("DA_{Id}");
	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("Data");
	Output.AssetClass = UDataForgeAssociationTestAsset::StaticClass();
	Output.AssetRuleId = ManagedRule.RuleId;

	FDataForgeAssociationSourceRule& AssociationSource = RuleSet->AssociationSources.AddDefaulted_GetRef();
	AssociationSource.SourceId = TEXT("Inventory");
	AssociationSource.Source.AdapterId = TEXT("AutomationParsedData");
	AssociationSource.Source.Parameters.Add(TEXT("Mode"), TEXT("Association"));
	AssociationSource.Source.Parameters.Add(TEXT("Path1"), NewManaged->GetPathName());
	AssociationSource.Source.Parameters.Add(TEXT("Revision"), TEXT("association-revision-2"));

	UDataForgeBindingPreset* Preset = NewObject<UDataForgeBindingPreset>(GetTransientPackage());
	Preset->OutputName = Output.OutputName;
	Preset->TargetClass = UDataForgeAssociationTestAsset::StaticClass();
	FDataForgeBindingPresetSlot& Slot = Preset->Slots.AddDefaulted_GetRef();
	Slot.SlotId = TEXT("Visuals");
	Slot.AssetKind = TEXT("TestAsset");
	Slot.Role = TEXT("Visual");
	Slot.TargetProperty = TEXT("Assets");
	Slot.ExpectedAssetClass = UDataForgeTestAsset::StaticClass();
	Slot.Cardinality = EDataForgeBindingCardinality::Many;
	Slot.Reconcile = EDataForgeBindingReconcileMode::MergeByKey;
	RuleSet->BindingPreset = Preset;

	const FString ExistingPackageName = ManagedRule.BaseFolder + TEXT("/DA_ExternalOne");
	UDataForgeAssociationTestAsset* Existing = Cast<UDataForgeAssociationTestAsset>(CreateAsset(ExistingPackageName, UDataForgeAssociationTestAsset::StaticClass()));
	Existing->Assets = { OldManaged, Manual };
	FMetaData& MetaData = Existing->GetPackage()->GetMetaData();
	MetaData.SetValue(Existing, TEXT("DataForge.Managed"), TEXT("true"));
	MetaData.SetValue(Existing, TEXT("DataForge.RuleSetId"), *RuleSet->RuleSetId.ToString(EGuidFormats::Digits));
	MetaData.SetValue(Existing, TEXT("DataForge.RecordId"), TEXT("ExternalOne"));
	MetaData.SetValue(Existing, TEXT("DataForge.Role"), TEXT("Data"));
	MetaData.SetValue(Existing, TEXT("DataForge.RuleVersion"), TEXT("1"));
	MetaData.SetValue(Existing, TEXT("DataForge.Association.Assets"), *OldManaged->GetPathName());
	MetaData.SetValue(Existing, TEXT("DataForge.Association.Keys"), TEXT("Assets"));

	FCompiledDataForgeRuleSet Compiled;
	TArray<FDataForgeDiagnostic> Diagnostics;
	TestTrue(TEXT("RuleSet with a generic Parsed Data association source compiles"), FDataForgeCompiler::Compile(*RuleSet, Compiled, Diagnostics));
	if (!Compiled.AssociationSlots.IsEmpty()) TestEqual(TEXT("A single Association Source is selected without typing its id"), Compiled.AssociationSlots[0].AssociationSourceId, FName(TEXT("Inventory")));
	TestNotEqual(TEXT("Association revision contributes to Source of Truth revision"), Compiled.DataSet.SourceRevision, FString(TEXT("parsed-data-revision")));
	const FDataForgeApplyPlan Plan = FDataForgeCompiler::BuildPlan(Compiled);
	TestFalse(TEXT("Association plan has no errors"), Plan.HasErrors());
	TestEqual(TEXT("Existing generated asset is updated"), Plan.AssetUpdateCount, 1);
	if (Plan.ManagedAssets.Num() == 1)
	{
		UDataForgeAssociationTestAsset* Materialized = NewObject<UDataForgeAssociationTestAsset>(GetTransientPackage());
		TArray<FDataForgeDiagnostic> ApplyDiagnostics;
		TestTrue(TEXT("Association property write materializes"), FDataForgeCompiler::ApplyPlannedProperties(*Materialized, Plan.ManagedAssets[0], ApplyDiagnostics));
		TArray<FString> Paths;
		for (const TSoftObjectPtr<UDataForgeTestAsset>& Asset : Materialized->Assets) Paths.Add(Asset.ToSoftObjectPath().ToString());
		TestTrue(TEXT("MergeByKey preserves a manual association"), Paths.Contains(Manual->GetPathName()));
		TestTrue(TEXT("MergeByKey adds the current managed association"), Paths.Contains(NewManaged->GetPathName()));
		TestFalse(TEXT("MergeByKey removes the previous managed association"), Paths.Contains(OldManaged->GetPathName()));
		TestEqual(TEXT("Manifest records only current managed paths"), Plan.ManagedAssets[0].ManagedAssociations.FindChecked(TEXT("Assets")), TArray<FString>({ NewManaged->GetPathName() }));
	}
	const FDataForgeBindingPresetSlot SavedSlot = Slot;
	Preset->Slots.Reset();
	FCompiledDataForgeRuleSet RemovedSlotCompiled;
	Diagnostics.Reset();
	TestTrue(TEXT("Removing an association slot still compiles"), FDataForgeCompiler::Compile(*RuleSet, RemovedSlotCompiled, Diagnostics));
	const FDataForgeApplyPlan RemovedSlotPlan = FDataForgeCompiler::BuildPlan(RemovedSlotCompiled);
	TestEqual(TEXT("Removed slot schedules stale ownership metadata cleanup"), RemovedSlotPlan.ManagedAssets[0].RemovedAssociationKeys, TArray<FString>({ TEXT("Assets") }));
	TestEqual(TEXT("Removed slot promotes existing values to manual instead of deleting them"), RemovedSlotPlan.ManagedAssets[0].PropertyWrites.Num(), 0);
	Preset->Slots.Add(SavedSlot);
	FDataForgeBindingPresetSlot& RestoredSlot = Preset->Slots[0];
	AssociationSource.Source.Parameters.Add(TEXT("Path2"), OldManaged->GetPathName());
	AssociationSource.Source.Parameters.Add(TEXT("Revision"), TEXT("association-revision-3"));
	RestoredSlot.TargetProperty = TEXT("PrimaryAsset");
	RestoredSlot.Cardinality = EDataForgeBindingCardinality::One;
	RestoredSlot.Reconcile = EDataForgeBindingReconcileMode::Assign;
	FCompiledDataForgeRuleSet AmbiguousCompiled;
	Diagnostics.Reset();
	TestTrue(TEXT("One-cardinality configuration compiles before row resolution"), FDataForgeCompiler::Compile(*RuleSet, AmbiguousCompiled, Diagnostics));
	const FDataForgeApplyPlan AmbiguousPlan = FDataForgeCompiler::BuildPlan(AmbiguousCompiled);
	TestTrue(TEXT("One cardinality rejects multiple matching assets"), AmbiguousPlan.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1923") && Diagnostic.Severity == EDataForgeSeverity::Error;
	}));

	FAssetRegistryModule::AssetDeleted(Existing);
	FAssetRegistryModule::AssetDeleted(Manual);
	FAssetRegistryModule::AssetDeleted(NewManaged);
	FAssetRegistryModule::AssetDeleted(OldManaged);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeSourceRowExpansionTest,
	"DataForge.Core.SchemaEvolution.SourceRowExpansion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeSourceRowExpansionTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeSourceRowExpansion.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("Id,DisplayName\nOne,First\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the source row expansion CSV."));
		return false;
	}

	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.File.FilePath = CsvFilename;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Output.RowStruct = FDataForgeTestRow::StaticStruct();
	RuleSet->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_SourceRowExpansion");

	FDataForgeAssetRule& ManagedRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	ManagedRule.RuleId = TEXT("Data");
	ManagedRule.Ownership = EDataForgeAssetOwnership::Managed;
	ManagedRule.BaseFolder = TEXT("/Game/DataForgeTests/SourceRowExpansion");
	ManagedRule.AssetNamePattern = TEXT("PDA_{Id}");
	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("data");
	Output.Type = EDataForgeGeneratedAssetType::PrimaryDataAsset;
	Output.AssetClass = UDataForgeTestPrimaryAsset::StaticClass();
	Output.AssetRuleId = ManagedRule.RuleId;

	FCompiledDataForgeRuleSet InitialCompiled;
	TArray<FDataForgeDiagnostic> InitialDiagnostics;
	TestTrue(TEXT("Initial one-row source compiles"), FDataForgeCompiler::Compile(*RuleSet, InitialCompiled, InitialDiagnostics));
	const FDataForgeApplyPlan InitialPlan = FDataForgeCompiler::BuildPlan(InitialCompiled);
	TestEqual(TEXT("Initial source plans one row"), InitialPlan.Rows.Num(), 1);
	TestEqual(TEXT("Initial source plans one generated asset"), InitialPlan.ManagedAssets.Num(), 1);

	TestTrue(TEXT("Expanded source is written"), FFileHelper::SaveStringToFile(TEXT("Id,DisplayName\nOne,First\nTwo,Second\n"), *CsvFilename));
	FCompiledDataForgeRuleSet ExpandedCompiled;
	TArray<FDataForgeDiagnostic> ExpandedDiagnostics;
	TestTrue(TEXT("Expanded two-row source recompiles"), FDataForgeCompiler::Compile(*RuleSet, ExpandedCompiled, ExpandedDiagnostics));
	const FDataForgeApplyPlan ExpandedPlan = FDataForgeCompiler::BuildPlan(ExpandedCompiled);
	TestEqual(TEXT("Expanded source plans both rows"), ExpandedPlan.Rows.Num(), 2);
	TestEqual(TEXT("Expanded source plans both generated assets"), ExpandedPlan.ManagedAssets.Num(), 2);
	TestNotEqual(TEXT("Source revision changes with the added row"), ExpandedPlan.SourceRevision, InitialPlan.SourceRevision);
	TestTrue(TEXT("New row produces its deterministic generated asset"), ExpandedPlan.ManagedAssets.ContainsByPredicate([](const FDataForgePlannedAsset& Asset)
	{
		return Asset.RecordId == TEXT("Two") && Asset.ObjectPath.EndsWith(TEXT("PDA_Two.PDA_Two"));
	}));

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeExpandedTargetSchemaTest,
	"DataForge.Core.SchemaEvolution.ExpandedRowAndGeneratedOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeExpandedTargetSchemaTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeExpandedTargetSchema.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("Id,DisplayName,Description\nOne,First,New field\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the expanded target schema CSV."));
		return false;
	}

	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.File.FilePath = CsvFilename;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Output.RowStruct = FDataForgeExpandedTestRow::StaticStruct();
	RuleSet->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_ExpandedTargetSchema");
	FDataForgeAssetRule& ManagedRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	ManagedRule.RuleId = TEXT("Data");
	ManagedRule.Ownership = EDataForgeAssetOwnership::Managed;
	ManagedRule.BaseFolder = TEXT("/Game/DataForgeTests/ExpandedTargetSchema");
	ManagedRule.AssetNamePattern = TEXT("PDA_{Id}");
	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("data");
	Output.Type = EDataForgeGeneratedAssetType::PrimaryDataAsset;
	Output.AssetClass = UDataForgeExpandedTestPrimaryAsset::StaticClass();
	Output.AssetRuleId = ManagedRule.RuleId;

	auto AddBinding = [RuleSet](EDataForgeBindingTarget Target, const TCHAR* TargetProperty)
	{
		FDataForgeBindingRule& Binding = RuleSet->Bindings.AddDefaulted_GetRef();
		Binding.Source = EDataForgeBindingSource::SourceValue;
		Binding.SourceColumn = TEXT("Description");
		Binding.Target = Target;
		Binding.TargetOutput = Target == EDataForgeBindingTarget::GeneratedOutput ? FName(TEXT("data")) : NAME_None;
		Binding.TargetProperty = TargetProperty;
	};
	AddBinding(EDataForgeBindingTarget::DataTableRow, TEXT("Description"));
	AddBinding(EDataForgeBindingTarget::GeneratedOutput, TEXT("Description"));

	FCompiledDataForgeRuleSet Compiled;
	TArray<FDataForgeDiagnostic> Diagnostics;
	TestTrue(TEXT("Expanded row and generated-output schemas compile"), FDataForgeCompiler::Compile(*RuleSet, Compiled, Diagnostics));
	const FDataForgeApplyPlan Plan = FDataForgeCompiler::BuildPlan(Compiled);
	TestFalse(TEXT("Expanded target plan has no errors"), Plan.HasErrors());
	if (Plan.Rows.Num() == 1 && Plan.Rows[0].DesiredData.IsValid())
	{
		const FDataForgeExpandedTestRow* Row = reinterpret_cast<const FDataForgeExpandedTestRow*>(Plan.Rows[0].DesiredData->GetStructMemory());
		TestEqual(TEXT("New Row Struct property receives its source value"), Row->Description, FString(TEXT("New field")));
	}
	if (Plan.ManagedAssets.Num() == 1)
	{
		TestTrue(TEXT("New generated-output property is included in the plan"), Plan.ManagedAssets[0].PropertyWrites.ContainsByPredicate([](const FDataForgePlannedPropertyWrite& Write)
		{
			return Write.PropertyPath == TEXT("Description") && Write.ExportedValue == TEXT("New field");
		}));
	}

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeMissingRowStructSafetyTest,
	"DataForge.Core.SchemaEvolution.MissingExistingRowStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeMissingRowStructSafetyTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeMissingExistingRowStruct.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("Id,DisplayName\nOne,First\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the missing Row Struct CSV."));
		return false;
	}

	const FString AssetName = TEXT("DT_MissingExistingRowStruct_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString TablePackageName = TEXT("/Game/DataForgeTests/") + AssetName;
	UPackage* TablePackage = CreatePackage(*TablePackageName);
	UDataTable* ExistingTable = NewObject<UDataTable>(TablePackage, *AssetName, RF_Public | RF_Standalone);
	TestNull(TEXT("Regression fixture starts with no Row Struct"), ExistingTable->GetRowStruct());

	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.File.FilePath = CsvFilename;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Output.RowStruct = FDataForgeTestRow::StaticStruct();
	RuleSet->Output.AssetPath = TablePackageName;

	FCompiledDataForgeRuleSet Compiled;
	TArray<FDataForgeDiagnostic> Diagnostics;
	TestTrue(TEXT("RuleSet compiles before inspecting the existing table"), FDataForgeCompiler::Compile(*RuleSet, Compiled, Diagnostics));
	const FDataForgeApplyPlan Plan = FDataForgeCompiler::BuildPlan(Compiled);
	TestTrue(TEXT("Missing existing Row Struct becomes an error, not a null dereference"), Plan.HasErrors());
	TestTrue(TEXT("DF1202 identifies the incompatible existing table"), Plan.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1202") && Diagnostic.Severity == EDataForgeSeverity::Error;
	}));
	TestEqual(TEXT("No rows are read from an incompatible table"), Plan.Rows.Num(), 0);

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeMissingGeneratedOutputClassSafetyTest,
	"DataForge.Core.SchemaEvolution.MissingGeneratedOutputClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeMissingGeneratedOutputClassSafetyTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeMissingGeneratedOutputClass.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("Id\nOne\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the missing generated-output class CSV."));
		return false;
	}

	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.File.FilePath = CsvFilename;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Output.RowStruct = FDataForgeTestRow::StaticStruct();
	RuleSet->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_MissingGeneratedOutputClass");
	FDataForgeAssetRule& ManagedRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	ManagedRule.RuleId = TEXT("Data");
	ManagedRule.Ownership = EDataForgeAssetOwnership::Managed;
	ManagedRule.BaseFolder = TEXT("/Game/DataForgeTests/MissingGeneratedOutputClass");
	ManagedRule.AssetNamePattern = TEXT("PDA_{Id}");
	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("data");
	Output.Type = EDataForgeGeneratedAssetType::PrimaryDataAsset;
	Output.AssetClass = nullptr;
	Output.AssetRuleId = ManagedRule.RuleId;

	FCompiledDataForgeRuleSet Compiled;
	TArray<FDataForgeDiagnostic> Diagnostics;
	TestFalse(TEXT("Missing generated-output class is rejected"), FDataForgeCompiler::Compile(*RuleSet, Compiled, Diagnostics));
	TestTrue(TEXT("DF1116 reports the missing generated-output class"), Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1116") && Diagnostic.Severity == EDataForgeSeverity::Error;
	}));

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeCompatibleUnownedAssetAdoptionTest,
	"DataForge.Core.GeneratedAsset.CompatibleUnownedAdoption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeCompatibleUnownedAssetAdoptionTest::RunTest(const FString& Parameters)
{
	const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeAdoption_") + Unique + TEXT(".csv"));
	const FString Root = TEXT("/Game/DataForgeTests/Adoption_") + Unique;
	const FString AssetPackageName = Root + TEXT("/DA_One");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("Id\nOne\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the adoption test CSV."));
		return false;
	}

	UPackage* LegacyPackage = CreatePackage(*AssetPackageName);
	UDataForgeTestAsset* LegacyAsset = NewObject<UDataForgeTestAsset>(
		LegacyPackage, TEXT("DA_One"), RF_Public | RF_Standalone | RF_Transient);
	FAssetRegistryModule::AssetCreated(LegacyAsset);

	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.File.FilePath = CsvFilename;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Output.RowStruct = FDataForgeTestRow::StaticStruct();
	RuleSet->Output.AssetPath = Root + TEXT("/DT_Adoption");
	FDataForgeAssetRule& ManagedRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	ManagedRule.RuleId = TEXT("Data");
	ManagedRule.Ownership = EDataForgeAssetOwnership::Managed;
	ManagedRule.BaseFolder = Root;
	ManagedRule.AssetNamePattern = TEXT("DA_{Id}");
	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("data");
	Output.AssetClass = UDataForgeTestAsset::StaticClass();
	Output.AssetRuleId = ManagedRule.RuleId;
	TestTrue(TEXT("Compatible unowned asset adoption is the migration-friendly default"), Output.bAdoptCompatibleUnownedAsset);
	Output.bAdoptCompatibleUnownedAsset = false;

	FCompiledDataForgeRuleSet Compiled;
	TArray<FDataForgeDiagnostic> Diagnostics;
	TestTrue(TEXT("Compatible legacy fixture compiles"), FDataForgeCompiler::Compile(*RuleSet, Compiled, Diagnostics));
	const FDataForgeApplyPlan Refused = FDataForgeCompiler::BuildPlan(Compiled);
	TestTrue(TEXT("Strict ownership mode can still reject adoption"), Refused.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1213");
	}));

	Output.bAdoptCompatibleUnownedAsset = true;
	Diagnostics.Reset();
	TestTrue(TEXT("Adoption-enabled fixture recompiles"), FDataForgeCompiler::Compile(*RuleSet, Compiled, Diagnostics));
	const FDataForgeApplyPlan Adopted = FDataForgeCompiler::BuildPlan(Compiled);
	TestFalse(TEXT("Compatible unowned asset no longer blocks Preview"), Adopted.HasErrors());
	TestTrue(TEXT("Adoption is reported for review"), Adopted.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1219") && Diagnostic.Severity == EDataForgeSeverity::Warning;
	}));
	TestTrue(TEXT("Existing asset is updated instead of recreated"), Adopted.ManagedAssets.ContainsByPredicate([LegacyAsset](const FDataForgePlannedAsset& Asset)
	{
		return Asset.ExistingAsset.Get() == LegacyAsset && Asset.Change == EDataForgeManagedAssetChange::Update;
	}));

	FAssetRegistryModule::AssetDeleted(LegacyAsset);
	LegacyAsset->ClearFlags(RF_Public | RF_Standalone);
	LegacyAsset->MarkAsGarbage();
	LegacyPackage->MarkAsGarbage();
	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

#endif
