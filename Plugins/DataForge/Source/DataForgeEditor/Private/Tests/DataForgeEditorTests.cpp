#include "DataForgeEditorService.h"
#include "DataForgeAutoReconciler.h"
#include "DataForgeBindingGraph.h"
#include "DataForgeRuleCreationWorkflow.h"
#include "DataForgeRuleSetSnapshot.h"
#include "DataForgeRuleSetSemanticDiff.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeRuleSet.h"
#include "Engine/DataAsset.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Tests/DataForgeEditorTestTypes.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAutoMapExactNamesTest,
	"DataForge.Editor.Authoring.AutoMapExactNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAutoMapExactNamesTest::RunTest(const FString& Parameters)
{
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
	const TArray<FName> Columns = { TEXT("DisplayName"), TEXT("Price"), TEXT("Missing"), TEXT("TransientValue") };

	TestEqual(TEXT("Only editable matching fields are auto-mapped"), FDataForgeEditorService::AutoMapExactNames(*RuleSet, Columns), 2);
	TestEqual(TEXT("Two bindings were created"), RuleSet->Bindings.Num(), 2);
	TestEqual(TEXT("Repeated auto-map does not create duplicates"), FDataForgeEditorService::AutoMapExactNames(*RuleSet, Columns), 0);
	TestEqual(TEXT("Binding count remains stable"), RuleSet->Bindings.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeConversionSuggestionTest,
	"DataForge.Editor.Authoring.ConversionSuggestions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeConversionSuggestionTest::RunTest(const FString& Parameters)
{
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();

	FDataForgeDataSet DataSet;
	DataSet.Columns = { TEXT("Price"), TEXT("Ratio"), TEXT("bEnabled"), TEXT("Rarity") };
	FDataForgeRow& Row = DataSet.Rows.AddDefaulted_GetRef();
	Row.Values.Add(TEXT("Price"), TEXT("1200"));
	Row.Values.Add(TEXT("Ratio"), TEXT("1.5"));
	Row.Values.Add(TEXT("bEnabled"), TEXT("true"));
	Row.Values.Add(TEXT("Rarity"), TEXT("Rare"));

	FDataForgeBindingRule Binding;
	Binding.Source = EDataForgeBindingSource::SourceValue;
	Binding.Target = EDataForgeBindingTarget::DataTableRow;

	Binding.SourceColumn = TEXT("Price");
	Binding.TargetProperty = TEXT("Price");
	TestEqual(
		TEXT("Integer to int32 is direct"),
		FDataForgeEditorService::AnalyzeBinding(*RuleSet, Binding, DataSet).Compatibility,
		EDataForgeBindingCompatibility::Direct);

	Binding.SourceColumn = TEXT("Ratio");
	Binding.TargetProperty = TEXT("Price");
	TestEqual(
		TEXT("Fractional number to int32 is risky"),
		FDataForgeEditorService::AnalyzeBinding(*RuleSet, Binding, DataSet).Compatibility,
		EDataForgeBindingCompatibility::Risky);

	Binding.SourceColumn = TEXT("bEnabled");
	Binding.TargetProperty = TEXT("bEnabled");
	TestEqual(
		TEXT("Boolean literal to bool is direct"),
		FDataForgeEditorService::AnalyzeBinding(*RuleSet, Binding, DataSet).Compatibility,
		EDataForgeBindingCompatibility::Direct);

	Binding.SourceColumn = TEXT("Rarity");
	Binding.TargetProperty = TEXT("Rarity");
	TestEqual(
		TEXT("Known enum name is direct"),
		FDataForgeEditorService::AnalyzeBinding(*RuleSet, Binding, DataSet).Compatibility,
		EDataForgeBindingCompatibility::Direct);

	Row.Values[TEXT("Rarity")] = TEXT("Legendary");
	TestEqual(
		TEXT("Unknown enum name is unsupported"),
		FDataForgeEditorService::AnalyzeBinding(*RuleSet, Binding, DataSet).Compatibility,
		EDataForgeBindingCompatibility::Unsupported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeBindingGraphConnectionTest,
	"DataForge.Editor.Authoring.BindingGraphConnections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeBindingGraphConnectionTest::RunTest(const FString& Parameters)
{
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
	FDataForgeDataSet DataSet;
	DataSet.Columns = { TEXT("Price"), TEXT("Ratio") };
	FDataForgeRow& Row = DataSet.Rows.AddDefaulted_GetRef();
	Row.Values.Add(TEXT("Price"), TEXT("1200"));
	Row.Values.Add(TEXT("Ratio"), TEXT("1.5"));

	const TArray<FDataForgeBindingGraphSource> Sources = FDataForgeBindingGraphModel::BuildSources(*RuleSet, DataSet);
	const TArray<FDataForgeBindingGraphTarget> Targets = FDataForgeBindingGraphModel::BuildTargets(*RuleSet);
	const FDataForgeBindingGraphSource* PriceSource = Sources.FindByPredicate([](const FDataForgeBindingGraphSource& Source) { return Source.Name == TEXT("Price"); });
	const FDataForgeBindingGraphSource* RatioSource = Sources.FindByPredicate([](const FDataForgeBindingGraphSource& Source) { return Source.Name == TEXT("Ratio"); });
	const FDataForgeBindingGraphTarget* PriceTarget = Targets.FindByPredicate([](const FDataForgeBindingGraphTarget& Target) { return Target.PropertyPath == TEXT("Price"); });
	TestNotNull(TEXT("Price source exists"), PriceSource);
	TestNotNull(TEXT("Ratio source exists"), RatioSource);
	TestNotNull(TEXT("Price target exists"), PriceTarget);
	if (!PriceSource || !RatioSource || !PriceTarget)
	{
		return false;
	}

	FString Message;
	TestTrue(TEXT("Direct graph connection creates a binding"), FDataForgeBindingGraphModel::Connect(*RuleSet, DataSet, *PriceSource, *PriceTarget, Message));
	TestEqual(TEXT("One graph binding was created"), RuleSet->Bindings.Num(), 1);
	TestFalse(TEXT("Duplicate target connection is rejected"), FDataForgeBindingGraphModel::Connect(*RuleSet, DataSet, *PriceSource, *PriceTarget, Message));

	UDataForgeRuleSet* RiskyRuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RiskyRuleSet->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
	TestFalse(TEXT("Risky number-to-integer graph connection is rejected"), FDataForgeBindingGraphModel::Connect(*RiskyRuleSet, DataSet, *RatioSource, *PriceTarget, Message));
	TestEqual(TEXT("Rejected graph connection creates no binding"), RiskyRuleSet->Bindings.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeSemanticDiffTest,
	"DataForge.Editor.Authoring.SemanticRuleSetDiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeSemanticDiffTest::RunTest(const FString& Parameters)
{
	UDataForgeRuleSet* Before = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	Before->Source.File.FilePath = TEXT("Before.csv");
	FDataForgeAssetRule& RuleA = Before->AssetRules.AddDefaulted_GetRef();
	RuleA.RuleId = TEXT("A");
	RuleA.BaseFolder = TEXT("/Game/A");
	FDataForgeAssetRule& RuleB = Before->AssetRules.AddDefaulted_GetRef();
	RuleB.RuleId = TEXT("B");
	RuleB.BaseFolder = TEXT("/Game/B");

	UDataForgeRuleSet* After = DuplicateObject<UDataForgeRuleSet>(Before, GetTransientPackage());
	After->AssetRules.Swap(0, 1);
	TestEqual(TEXT("Asset rule reorder is semantically unchanged"), FDataForgeRuleSetSemanticDiff::Compare(*Before, *After).Num(), 0);

	After->Source.File.FilePath = TEXT("After.csv");
	After->AssetRules.FindByPredicate([](const FDataForgeAssetRule& Rule) { return Rule.RuleId == TEXT("B"); })->BaseFolder = TEXT("/Game/B2");
	FDataForgeBindingRule& Binding = After->Bindings.AddDefaulted_GetRef();
	Binding.SourceColumn = TEXT("Price");
	Binding.TargetProperty = TEXT("Price");
	UDataForgeRuleSet* Dependency = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	After->Dependencies.AddDefaulted_GetRef().RuleSet = Dependency;

	const TArray<FDataForgeSemanticDiffEntry> Entries = FDataForgeRuleSetSemanticDiff::Compare(*Before, *After);
	TestTrue(TEXT("Source file change has a stable semantic path"), Entries.ContainsByPredicate([](const FDataForgeSemanticDiffEntry& Entry) { return Entry.Path == TEXT("Source.File"); }));
	TestTrue(TEXT("Asset rule field change is keyed by RuleId"), Entries.ContainsByPredicate([](const FDataForgeSemanticDiffEntry& Entry) { return Entry.Path == TEXT("AssetRules[B].BaseFolder"); }));
	TestTrue(TEXT("Binding addition is keyed by target"), Entries.ContainsByPredicate([](const FDataForgeSemanticDiffEntry& Entry) { return Entry.Path == TEXT("Bindings[row.Price]") && Entry.Kind == EDataForgeSemanticDiffKind::Added; }));
	TestTrue(TEXT("Dependency addition is keyed by RuleSet path"), Entries.ContainsByPredicate([Dependency](const FDataForgeSemanticDiffEntry& Entry)
	{
		return Entry.Path == FString::Printf(TEXT("Dependencies[%s]"), *Dependency->GetPathName())
			&& Entry.Kind == EDataForgeSemanticDiffKind::Added;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeDependencyGraphPreviewTest,
	"DataForge.Editor.Dependency.PreviewGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeDependencyGraphPreviewTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeDependencyGraphPreview.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("DisplayName,Price\nSword,1200\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the Dependency Graph Preview test CSV."));
		return false;
	}

	auto Configure = [&CsvFilename](UDataForgeRuleSet& RuleSet, const TCHAR* OutputPath)
	{
		RuleSet.RuleSetId = FGuid::NewGuid();
		RuleSet.Source.File.FilePath = CsvFilename;
		RuleSet.Schema.PrimaryKey = TEXT("DisplayName");
		RuleSet.Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
		RuleSet.Output.AssetPath = OutputPath;
		RuleSet.Output.bSaveAfterApply = false;
	};

	UDataForgeRuleSet* Prerequisite = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	UDataForgeRuleSet* Root = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	Configure(*Prerequisite, TEXT("/Game/DataForgeTests/DT_DependencyPrerequisite"));
	Configure(*Root, TEXT("/Game/DataForgeTests/DT_DependencyRoot"));
	Root->Dependencies.AddDefaulted_GetRef().RuleSet = Prerequisite;

	FDataForgeApplyPlan RootPlan;
	const FDataForgeResult Result = FDataForgeEditorService::PreviewDependencyGraph(*Root, &RootPlan);
	TestTrue(TEXT("Dependency graph Preview validates prerequisites and root"), Result.bSuccess);
	TestTrue(TEXT("Returned plan belongs to the root RuleSet"), RootPlan.RuleSet.Get() == Root);
#if WITH_EDITORONLY_DATA
	TestEqual(TEXT("Root status records graph Preview"), Root->LastStatus, FString(TEXT("Previewed Graph")));
#endif

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeManagedAssetMoveTest,
	"DataForge.Editor.ManagedAssets.MoveOnApply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeManagedAssetMoveTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeManagedAssetMove.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("Id\nSword\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the Managed Asset Move test CSV."));
		return false;
	}

	TStrongObjectPtr<UDataForgeRuleSet> RuleSet(NewObject<UDataForgeRuleSet>(GetTransientPackage()));
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.File.FilePath = CsvFilename;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Schema.bWarnOnUnmappedColumns = false;
	RuleSet->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
	RuleSet->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_ManagedAssetMove");
	RuleSet->Output.bSaveAfterApply = false;
	FDataForgeAssetRule& AssetRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	AssetRule.RuleId = TEXT("Managed");
	AssetRule.Ownership = EDataForgeAssetOwnership::Managed;
	AssetRule.BaseFolder = TEXT("/Game/DataForgeTests/ManagedMove/New");
	AssetRule.AssetNamePattern = TEXT("DA_{Id}");
	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("data");
	Output.AssetClass = UDataForgeEditorManagedAsset::StaticClass();
	Output.AssetRuleId = AssetRule.RuleId;

	UPackage* OldPackage = CreatePackage(TEXT("/Game/DataForgeTests/ManagedMove/Legacy/DA_Sword"));
	TStrongObjectPtr<UDataForgeEditorManagedAsset> ExistingAsset(NewObject<UDataForgeEditorManagedAsset>(OldPackage, TEXT("DA_Sword"), RF_Public | RF_Standalone));
	FAssetRegistryModule::AssetCreated(ExistingAsset.Get());
	FMetaData& MetaData = OldPackage->GetMetaData();
	MetaData.SetValue(ExistingAsset.Get(), TEXT("DataForge.Managed"), TEXT("true"));
	MetaData.SetValue(ExistingAsset.Get(), TEXT("DataForge.RuleSetId"), *RuleSet->RuleSetId.ToString(EGuidFormats::Digits));
	MetaData.SetValue(ExistingAsset.Get(), TEXT("DataForge.RecordId"), TEXT("Sword"));
	MetaData.SetValue(ExistingAsset.Get(), TEXT("DataForge.Role"), TEXT("data"));
	MetaData.SetValue(ExistingAsset.Get(), TEXT("DataForge.RuleVersion"), TEXT("1"));

	FDataForgeApplyPlan Plan;
	TestTrue(TEXT("Move Preview succeeds"), FDataForgeEditorService::Preview(*RuleSet, &Plan).bSuccess);
	TestEqual(TEXT("One managed Move is planned"), Plan.AssetMoveCount, 1);
	const FDataForgePlannedAsset* Move = Plan.ManagedAssets.FindByPredicate([](const FDataForgePlannedAsset& Asset)
	{
		return Asset.Change == EDataForgeManagedAssetChange::Move;
	});
	TestNotNull(TEXT("Move operation is present"), Move);
	if (Move)
	{
		TestEqual(TEXT("Move remembers old path"), Move->PreviousObjectPath, FString(TEXT("/Game/DataForgeTests/ManagedMove/Legacy/DA_Sword.DA_Sword")));
		TestEqual(TEXT("Move calculates desired path"), Move->ObjectPath, FString(TEXT("/Game/DataForgeTests/ManagedMove/New/DA_Sword.DA_Sword")));
	}
	TestTrue(TEXT("Apply performs the managed Move"), FDataForgeEditorService::Apply(*RuleSet).bSuccess);
	TestEqual(TEXT("Asset now has the desired object path"), ExistingAsset->GetPathName(), FString(TEXT("/Game/DataForgeTests/ManagedMove/New/DA_Sword.DA_Sword")));

	const FString MovedAssetFilename = FPackageName::LongPackageNameToFilename(
		TEXT("/Game/DataForgeTests/ManagedMove/New/DA_Sword"),
		FPackageName::GetAssetPackageExtension());
	TestTrue(
		TEXT("Move test removes the package saved internally by AssetTools"),
		!IFileManager::Get().FileExists(*MovedAssetFilename) || IFileManager::Get().Delete(*MovedAssetFilename, false, true));
	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeRuleSetSnapshotTest,
	"DataForge.Editor.Snapshot.DeterminismAndDrift",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeRuleSetSnapshotTest::RunTest(const FString& Parameters)
{
	UPackage* RulePackage = CreatePackage(TEXT("/DataForgeSnapshotTests/RS_Snapshot"));
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(RulePackage, TEXT("RS_Snapshot"));
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->RuleVersion = 3;
	RuleSet->Source.AdapterId = TEXT("ProjectParsed");
	RuleSet->Source.File.FilePath = TEXT("Data/Items.csv");
	RuleSet->Source.Parameters.Add(TEXT("Zulu"), TEXT("last"));
	RuleSet->Source.Parameters.Add(TEXT("Alpha"), TEXT("first"));
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Schema.RequiredColumns = { TEXT("Zulu"), TEXT("Alpha") };
	RuleSet->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
	RuleSet->Output.AssetPath = TEXT("/Game/Data/DT_Items");

	FDataForgeAssetRule& RuleB = RuleSet->AssetRules.AddDefaulted_GetRef();
	RuleB.RuleId = TEXT("B");
	RuleB.BaseFolder = TEXT("/Game/B");
	FDataForgeAssetRule& RuleA = RuleSet->AssetRules.AddDefaulted_GetRef();
	RuleA.RuleId = TEXT("A");
	RuleA.BaseFolder = TEXT("/Game/A");

	const FString JsonBeforeReorder = FDataForgeRuleSetSnapshot::SerializeJson(*RuleSet);
	const FString YamlBeforeReorder = FDataForgeRuleSetSnapshot::SerializeYaml(*RuleSet);
	RuleSet->Schema.RequiredColumns.Swap(0, 1);
	RuleSet->AssetRules.Swap(0, 1);
	TestEqual(TEXT("JSON snapshot ignores semantically irrelevant array order"), FDataForgeRuleSetSnapshot::SerializeJson(*RuleSet), JsonBeforeReorder);
	TestEqual(TEXT("YAML snapshot ignores semantically irrelevant array order"), FDataForgeRuleSetSnapshot::SerializeYaml(*RuleSet), YamlBeforeReorder);
	TestTrue(TEXT("JSON snapshot contains its schema version"), JsonBeforeReorder.Contains(TEXT("\"snapshotVersion\"")));
	TestTrue(TEXT("YAML snapshot is clearly generated"), YamlBeforeReorder.StartsWith(TEXT("# Generated by DataForge")));

	const FString SnapshotDirectory = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation"),
		TEXT("DataForgeSnapshots"),
		RuleSet->RuleSetId.ToString(EGuidFormats::Digits));
	FString JsonFilename;
	FString YamlFilename;
	TestTrue(
		TEXT("Snapshot export writes both review formats"),
		FDataForgeRuleSetSnapshot::Export(*RuleSet, SnapshotDirectory, &JsonFilename, &YamlFilename).bSuccess);
	TestTrue(TEXT("Fresh snapshots verify"), FDataForgeRuleSetSnapshot::Verify(*RuleSet, SnapshotDirectory).bSuccess);

	RuleSet->RuleVersion = 4;
	const FDataForgeResult DriftResult = FDataForgeRuleSetSnapshot::Verify(*RuleSet, SnapshotDirectory);
	TestFalse(TEXT("RuleSet changes make snapshots stale"), DriftResult.bSuccess);
	TestTrue(TEXT("Drift identifies the stale JSON snapshot"), DriftResult.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF5005");
	}));
	TestTrue(TEXT("Drift identifies the stale YAML snapshot"), DriftResult.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF5007");
	}));

	IFileManager::Get().Delete(*JsonFilename, false, true);
	IFileManager::Get().Delete(*YamlFilename, false, true);
	IFileManager::Get().DeleteDirectory(*SnapshotDirectory, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeManagedAssetCleanupTest,
	"DataForge.Editor.ManagedAssets.ExplicitCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeManagedAssetCleanupTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeManagedAssetCleanup.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("Id\nKept\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the Managed Asset Cleanup test CSV."));
		return false;
	}

	TStrongObjectPtr<UDataForgeRuleSet> RuleSet(NewObject<UDataForgeRuleSet>(GetTransientPackage()));
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.File.FilePath = CsvFilename;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Schema.bWarnOnUnmappedColumns = false;
	RuleSet->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
	RuleSet->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_ManagedAssetCleanup");
	RuleSet->Output.bSaveAfterApply = false;
	FDataForgeAssetRule& AssetRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	AssetRule.RuleId = TEXT("Managed");
	AssetRule.Ownership = EDataForgeAssetOwnership::Managed;
	AssetRule.BaseFolder = TEXT("/Game/DataForgeTests/ManagedCleanup");
	AssetRule.AssetNamePattern = TEXT("DA_{Id}");
	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("data");
	Output.AssetClass = UDataForgeEditorManagedAsset::StaticClass();
	Output.AssetRuleId = AssetRule.RuleId;

	UPackage* OrphanPackage = CreatePackage(TEXT("/Game/DataForgeTests/ManagedCleanup/DA_Removed"));
	UDataForgeEditorManagedAsset* OrphanAsset = NewObject<UDataForgeEditorManagedAsset>(OrphanPackage, TEXT("DA_Removed"), RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(OrphanAsset);
	FMetaData& OrphanMetaData = OrphanPackage->GetMetaData();
	OrphanMetaData.SetValue(OrphanAsset, TEXT("DataForge.Managed"), TEXT("true"));
	OrphanMetaData.SetValue(OrphanAsset, TEXT("DataForge.RuleSetId"), *RuleSet->RuleSetId.ToString(EGuidFormats::Digits));
	OrphanMetaData.SetValue(OrphanAsset, TEXT("DataForge.RecordId"), TEXT("Removed"));
	OrphanMetaData.SetValue(OrphanAsset, TEXT("DataForge.Role"), TEXT("data"));
	OrphanMetaData.SetValue(OrphanAsset, TEXT("DataForge.RuleVersion"), TEXT("1"));

	UPackage* ExternalPackage = CreatePackage(TEXT("/Game/DataForgeTests/ManagedCleanup/DA_External"));
	TStrongObjectPtr<UDataForgeEditorManagedAsset> ExternalAsset(NewObject<UDataForgeEditorManagedAsset>(ExternalPackage, TEXT("DA_External"), RF_Public | RF_Standalone));
	FAssetRegistryModule::AssetCreated(ExternalAsset.Get());

	FDataForgeApplyPlan Plan;
	TestTrue(TEXT("Cleanup Preview succeeds"), FDataForgeEditorService::Preview(*RuleSet, &Plan).bSuccess);
	TestEqual(TEXT("Only the owned missing record is an orphan"), Plan.AssetOrphanCount, 1);
	TestTrue(TEXT("Explicit Cleanup succeeds"), FDataForgeEditorService::CleanupOrphans(*RuleSet).bSuccess);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TestFalse(
		TEXT("Managed orphan was removed from the Asset Registry"),
		AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(TEXT("/Game/DataForgeTests/ManagedCleanup/DA_Removed.DA_Removed"))).IsValid());
	TestTrue(
		TEXT("Unowned asset remains in the Asset Registry"),
		AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(TEXT("/Game/DataForgeTests/ManagedCleanup/DA_External.DA_External"))).IsValid());

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeRuleCreationWorkflowTest,
	"DataForge.Editor.Authoring.RuleCreationWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeRuleCreationWorkflowTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeRuleCreationWorkflow.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("DisplayName,Price\nSword,1200\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the Rule Creation Workflow test CSV."));
		return false;
	}

	UDataForgeRuleSet* Target = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	Target->RuleSetId = FGuid::NewGuid();
	Target->Source.File.FilePath = CsvFilename;
	Target->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
	Target->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_RuleCreationWorkflow");
	Target->Output.bSaveAfterApply = false;

	FDataForgeRuleCreationWorkflow Workflow(*Target);
	FString Reason;
	TestTrue(TEXT("Registered source advances to Probe"), Workflow.Next(Reason));
	TestTrue(TEXT("Probe succeeds"), Workflow.Probe().bSuccess);
	TestEqual(TEXT("Probe infers required columns"), Workflow.GetDraft().Schema.RequiredColumns.Num(), 2);
	TestEqual(TEXT("Probe suggests the first available key when no Id exists"), Workflow.GetDraft().Schema.PrimaryKey, FName(TEXT("DisplayName")));
	TestTrue(TEXT("Successful Probe advances to Schema"), Workflow.Next(Reason));
	Workflow.GetDraft().Schema.PrimaryKey = TEXT("DisplayName");
	Workflow.NotifyDraftChanged(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Schema));
	TestTrue(TEXT("Explicit primary key advances to Output"), Workflow.Next(Reason));
	TestTrue(TEXT("Valid output advances to Asset Rules"), Workflow.Next(Reason));
	TestEqual(TEXT("Asset Rules step follows Output"), Workflow.GetStep(), EDataForgeWizardStep::AssetRules);
	TestTrue(TEXT("Empty optional Asset Rules advance to Generated Outputs"), Workflow.Next(Reason));
	TestEqual(TEXT("Generated Outputs step follows Asset Rules"), Workflow.GetStep(), EDataForgeWizardStep::GeneratedOutputs);
	TestTrue(TEXT("Empty optional Generated Outputs advance to Bindings"), Workflow.Next(Reason));
	TestEqual(TEXT("Entering Bindings automatically maps both matching fields"), Workflow.GetDraft().Bindings.Num(), 2);
	TestEqual(TEXT("Repeated Auto Map does not duplicate bindings"), Workflow.AutoMapExactNames(), 0);
	TestTrue(TEXT("Bindings advance to Preview"), Workflow.Next(Reason));
	TestTrue(TEXT("Mutation-free Preview succeeds"), Workflow.Preview().bSuccess);
	TestTrue(TEXT("Finish commits staged draft"), Workflow.Finish(Reason));
	TestEqual(TEXT("Committed primary key reaches target"), Target->Schema.PrimaryKey, FName(TEXT("DisplayName")));
	TestEqual(TEXT("Committed bindings reach target"), Target->Bindings.Num(), 2);
	TestNotNull(TEXT("Finish automatically creates the configured DataTable"),
		FindObject<UDataTable>(nullptr, TEXT("/Game/DataForgeTests/DT_RuleCreationWorkflow.DT_RuleCreationWorkflow")));

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeGeneratedOutputAutoMapTest,
	"DataForge.Editor.Authoring.GeneratedOutputAutoMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeGeneratedOutputAutoMapTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UDataForgeRuleSet> RuleSet(NewObject<UDataForgeRuleSet>(GetTransientPackage()));
	RuleSet->Output.RowStruct = FDataForgeEditorGeneratedOutputRow::StaticStruct();
	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("Data");
	Output.AssetClass = UDataForgeEditorManagedAsset::StaticClass();

	TestEqual(TEXT("Row field, Generated Output property, and same-name output reference are inferred"),
		FDataForgeEditorService::AutoMapExactNames(*RuleSet, { TEXT("DisplayName") }), 3);
	TestTrue(TEXT("Generated Output binds to its same-name soft reference field"),
		RuleSet->Bindings.ContainsByPredicate([](const FDataForgeBindingRule& Binding)
		{
			return Binding.Source == EDataForgeBindingSource::GeneratedOutput
				&& Binding.SourceOutput == TEXT("Data")
				&& Binding.Target == EDataForgeBindingTarget::DataTableRow
				&& Binding.TargetProperty.Equals(TEXT("Data"));
		}));
	TestTrue(TEXT("Source column binds to the matching Generated Output property"),
		RuleSet->Bindings.ContainsByPredicate([](const FDataForgeBindingRule& Binding)
		{
			return Binding.Source == EDataForgeBindingSource::SourceValue
				&& Binding.SourceColumn == TEXT("DisplayName")
				&& Binding.Target == EDataForgeBindingTarget::GeneratedOutput
				&& Binding.TargetOutput == TEXT("Data")
				&& Binding.TargetProperty.Equals(TEXT("DisplayName"));
		}));
	TestEqual(TEXT("Repeated inference remains duplicate-free"),
		FDataForgeEditorService::AutoMapExactNames(*RuleSet, { TEXT("DisplayName") }), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeGeneratedOutputWorkflowSyncTest,
	"DataForge.Editor.Authoring.GeneratedOutputWorkflowSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeGeneratedOutputWorkflowSyncTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeGeneratedOutputSync.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("DisplayName\nSword\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the Generated Output sync test CSV."));
		return false;
	}
	TStrongObjectPtr<UDataForgeRuleSet> Target(NewObject<UDataForgeRuleSet>(GetTransientPackage()));
	Target->Source.AdapterId = TEXT("Csv");
	Target->Source.File.FilePath = CsvFilename;
	Target->Output.RowStruct = FDataForgeEditorGeneratedOutputRow::StaticStruct();
	FDataForgeRuleCreationWorkflow Workflow(*Target);
	TestTrue(TEXT("Generated Output sync source probe succeeds"), Workflow.Probe().bSuccess);
	UDataForgeRuleSet& Draft = Workflow.GetDraft();

	FDataForgeGeneratedAssetOutputRule& Output = Draft.GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("Data");
	Output.AssetClass = UDataForgeEditorManagedAsset::StaticClass();
	Workflow.NotifyDraftChanged(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, GeneratedOutputs));
	TestTrue(TEXT("Generated Output edit immediately adds its DataTable reference binding"),
		Draft.Bindings.ContainsByPredicate([](const FDataForgeBindingRule& Binding)
		{
			return Binding.Source == EDataForgeBindingSource::GeneratedOutput && Binding.SourceOutput == TEXT("Data");
		}));
	TestTrue(TEXT("Generated Output edit immediately binds matching source fields into the PDA/DA"),
		Draft.Bindings.ContainsByPredicate([](const FDataForgeBindingRule& Binding)
		{
			return Binding.Source == EDataForgeBindingSource::SourceValue
				&& Binding.SourceColumn == TEXT("DisplayName")
				&& Binding.Target == EDataForgeBindingTarget::GeneratedOutput
				&& Binding.TargetOutput == TEXT("Data")
				&& Binding.TargetProperty.Equals(TEXT("DisplayName"));
		}));

	Draft.GeneratedOutputs.Reset();
	Workflow.NotifyDraftChanged(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, GeneratedOutputs));
	TestFalse(TEXT("Removing a Generated Output removes bindings that reference it"),
		Draft.Bindings.ContainsByPredicate([](const FDataForgeBindingRule& Binding)
		{
			return Binding.Source == EDataForgeBindingSource::GeneratedOutput || Binding.Target == EDataForgeBindingTarget::GeneratedOutput;
		}));
	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeSourceReconcileTest,
	"DataForge.Editor.Automation.SourceReconcile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeSourceReconcileTest::RunTest(const FString& Parameters)
{
	const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeReconcile_") + Unique + TEXT(".csv"));
	const FString TablePackageName = TEXT("/Game/DataForgeTests/Automation/DT_Reconcile_") + Unique;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("DisplayName,Price\nSword,1200\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the reconciler test CSV."));
		return false;
	}

	TStrongObjectPtr<UDataForgeRuleSet> RuleSet(NewObject<UDataForgeRuleSet>(
		GetTransientPackage(), *FString(TEXT("RS_Reconcile_") + Unique)));
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.AdapterId = TEXT("Csv");
	RuleSet->Source.File.FilePath = CsvFilename;
	RuleSet->Schema.PrimaryKey = TEXT("DisplayName");
	RuleSet->Schema.RequiredColumns = { TEXT("DisplayName"), TEXT("Price") };
	RuleSet->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
	RuleSet->Output.AssetPath = TablePackageName;
	RuleSet->Output.bSaveAfterApply = false;
	FDataForgeEditorService::AutoMapExactNames(*RuleSet, RuleSet->Schema.RequiredColumns);

	FDataForgeAutoReconciler& Reconciler = FDataForgeAutoReconciler::Get();
	Reconciler.Request(*RuleSet, TEXT("initial source"), 0.0);
	const FDataForgeReconcileBatchResult Initial = Reconciler.FlushPending(true);
	TestEqual(TEXT("Initial reconcile processes one RuleSet"), Initial.ProcessedCount, 1);
	TestEqual(TEXT("Initial reconcile applies one RuleSet"), Initial.AppliedCount, 1);

	UDataTable* Table = FindObject<UDataTable>(nullptr, *(TablePackageName + TEXT(".") + FPackageName::GetLongPackageAssetName(TablePackageName)));
	TestNotNull(TEXT("Initial reconcile creates the DataTable"), Table);
	if (Table) TestEqual(TEXT("Initial source creates one row"), Table->GetRowMap().Num(), 1);

	TestTrue(TEXT("Expanded source CSV is written"),
		FFileHelper::SaveStringToFile(TEXT("DisplayName,Price\nSword,1200\nShield,800\n"), *CsvFilename));
	Reconciler.Request(*RuleSet, TEXT("source file changed"), 0.0);
	Reconciler.Request(*RuleSet, TEXT("duplicate source event"), 0.0);
	const FDataForgeReconcileBatchResult Expanded = Reconciler.FlushPending(true);
	TestEqual(TEXT("Duplicate events are coalesced into one reconcile"), Expanded.ProcessedCount, 1);
	TestEqual(TEXT("Expanded source applies successfully"), Expanded.AppliedCount, 1);
	if (Table)
	{
		TestEqual(TEXT("Existing DataTable receives the new source row"), Table->GetRowMap().Num(), 2);
		TestNotNull(TEXT("New source row is materialized"),
			Table->FindRow<FDataForgeEditorAutoMapRow>(TEXT("Shield"), TEXT("source reconcile test")));
	}

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

#endif
