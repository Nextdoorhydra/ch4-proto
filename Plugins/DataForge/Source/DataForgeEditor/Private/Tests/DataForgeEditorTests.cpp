#include "DataForgeEditorService.h"
#include "DataForgeAutoReconciler.h"
#include "DataForgeBindingGraph.h"
#include "DataForgeBindingPresetAuthoring.h"
#include "DataForgeBindingPresetFactory.h"
#include "DataForgeRuleCreationWorkflow.h"
#include "DataForgeRenameImpact.h"
#include "DataForgeRenameAdvisor.h"
#include "DataForgeRenameRecovery.h"
#include "DataForgeRuleSetSnapshot.h"
#include "DataForgeRuleSetSemanticDiff.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeBindingPreset.h"
#include "DataForgeFolderSource.h"
#include "DataForgeNamingPolicy.h"
#include "DataForgeRuleSet.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/DataForgeEditorTestTypes.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
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
	FDataForgeBindingPresetMaterializationTest,
	"DataForge.Editor.Authoring.BindingPreset.Materialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeBindingPresetMaterializationTest::RunTest(const FString& Parameters)
{
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Output.RowStruct = FDataForgeEditorPresetRow::StaticStruct();

	UDataForgeBindingPreset* Preset = NewObject<UDataForgeBindingPreset>(GetTransientPackage());
	Preset->OutputName = TEXT("Data");
	Preset->TargetClass = UDataForgeEditorPresetAsset::StaticClass();
	Preset->AssetNamePrefix = TEXT("DA");
	FDataForgeBindingPresetSlot& Slot = Preset->Slots.AddDefaulted_GetRef();
	Slot.SlotId = TEXT("Icon");
	Slot.AssetKind = TEXT("Texture");
	Slot.ExpectedAssetClass = UTexture::StaticClass();

	const FDataForgeBindingPresetMaterialization First = FDataForgeBindingPresetAuthoring::Materialize(
		*RuleSet, *Preset, TEXT("/Game/DataForgeTests/Preset"));
	TestTrue(TEXT("Compatible slot and row reference materialize"), First.bSuccess);
	TestEqual(TEXT("One relationship slot is resolved"), First.ResolvedSlotCount, 1);
	TestEqual(TEXT("No slot is ambiguous"), First.AmbiguousSlotCount, 0);
	TestEqual(TEXT("One Managed rule is generated"), RuleSet->AssetRules.Num(), 1);
	TestEqual(TEXT("Managed rule id is deterministic"), RuleSet->AssetRules[0].RuleId, FName(TEXT("Data_Managed")));
	TestEqual(TEXT("Managed output name follows primary key"), RuleSet->AssetRules[0].AssetNamePattern, FString(TEXT("DA_{Id}")));
	TestEqual(TEXT("One generated output is configured"), RuleSet->GeneratedOutputs.Num(), 1);
	TestEqual(TEXT("Generated output targets preset class"), RuleSet->GeneratedOutputs[0].AssetClass.Get(), UDataForgeEditorPresetAsset::StaticClass());
	TestEqual(TEXT("Generated output uses managed rule"), RuleSet->GeneratedOutputs[0].AssetRuleId, FName(TEXT("Data_Managed")));
	TestEqual(TEXT("One generated-object row binding is inferred"), RuleSet->Bindings.Num(), 1);
	TestEqual(TEXT("Row binding targets the matching Data property"), RuleSet->Bindings[0].TargetProperty, FString(TEXT("Data")));
	TestEqual(TEXT("RuleSet tracks preset provenance"), RuleSet->BindingPreset.Get(), Preset);

	const FDataForgeBindingPresetMaterialization Second = FDataForgeBindingPresetAuthoring::Materialize(
		*RuleSet, *Preset, TEXT("/Game/DataForgeTests/Preset"));
	TestTrue(TEXT("Repeated materialization succeeds"), Second.bSuccess);
	TestEqual(TEXT("Repeated materialization keeps one rule"), RuleSet->AssetRules.Num(), 1);
	TestEqual(TEXT("Repeated materialization keeps one output"), RuleSet->GeneratedOutputs.Num(), 1);
	TestEqual(TEXT("Repeated materialization keeps one row binding"), RuleSet->Bindings.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeBindingPresetAmbiguityTest,
	"DataForge.Editor.Authoring.BindingPreset.AmbiguityAndCardinality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeBindingPresetAmbiguityTest::RunTest(const FString& Parameters)
{
	UDataForgeBindingPreset* Ambiguous = NewObject<UDataForgeBindingPreset>(GetTransientPackage());
	Ambiguous->TargetClass = UDataForgeEditorAmbiguousPresetAsset::StaticClass();
	FDataForgeBindingPresetSlot& AmbiguousSlot = Ambiguous->Slots.AddDefaulted_GetRef();
	AmbiguousSlot.SlotId = TEXT("Texture");
	AmbiguousSlot.ExpectedAssetClass = UTexture::StaticClass();
	const FDataForgeBindingPresetMaterialization AmbiguousResult = FDataForgeBindingPresetAuthoring::Validate(*Ambiguous);
	TestTrue(TEXT("Ambiguity is non-destructive and requires an explicit property"), AmbiguousResult.bSuccess);
	TestEqual(TEXT("Ambiguous slot is counted"), AmbiguousResult.AmbiguousSlotCount, 1);
	TestEqual(TEXT("Ambiguous slot is not arbitrarily resolved"), AmbiguousResult.ResolvedSlotCount, 0);
	TestTrue(TEXT("Ambiguity has a stable diagnostic"), AmbiguousResult.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1908") && Diagnostic.Severity == EDataForgeSeverity::Warning;
	}));

	UDataForgeBindingPreset* InvalidMany = NewObject<UDataForgeBindingPreset>(GetTransientPackage());
	InvalidMany->TargetClass = UDataForgeEditorManyPresetAsset::StaticClass();
	FDataForgeBindingPresetSlot& ManySlot = InvalidMany->Slots.AddDefaulted_GetRef();
	ManySlot.SlotId = TEXT("Textures");
	ManySlot.TargetProperty = TEXT("Textures");
	ManySlot.ExpectedAssetClass = UTexture::StaticClass();
	ManySlot.Cardinality = EDataForgeBindingCardinality::Many;
	ManySlot.Reconcile = EDataForgeBindingReconcileMode::Assign;
	const FDataForgeBindingPresetMaterialization InvalidManyResult = FDataForgeBindingPresetAuthoring::Validate(*InvalidMany);
	TestFalse(TEXT("Many cannot use scalar Assign semantics"), InvalidManyResult.bSuccess);
	TestTrue(TEXT("Invalid cardinality has a stable diagnostic"), InvalidManyResult.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1909") && Diagnostic.Severity == EDataForgeSeverity::Error;
	}));

	UDataForgeRuleSet* InvalidRowRuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	InvalidRowRuleSet->Schema.PrimaryKey = TEXT("Id");
	InvalidRowRuleSet->Output.RowStruct = FDataForgeEditorPresetRow::StaticStruct();
	UDataForgeBindingPreset* InvalidRowPreset = NewObject<UDataForgeBindingPreset>(GetTransientPackage());
	InvalidRowPreset->TargetClass = UDataForgeEditorPresetAsset::StaticClass();
	InvalidRowPreset->RowReferenceProperty = TEXT("MissingProperty");
	const FDataForgeBindingPresetMaterialization InvalidRowResult = FDataForgeBindingPresetAuthoring::Materialize(
		*InvalidRowRuleSet, *InvalidRowPreset, TEXT("/Game/DataForgeTests/Preset"));
	TestFalse(TEXT("Invalid explicit row reference fails"), InvalidRowResult.bSuccess);
	TestEqual(TEXT("Failed materialization does not leave an Asset Rule"), InvalidRowRuleSet->AssetRules.Num(), 0);
	TestEqual(TEXT("Failed materialization does not leave a Generated Output"), InvalidRowRuleSet->GeneratedOutputs.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeBindingPresetFactoryTest,
	"DataForge.Editor.Authoring.BindingPreset.Factory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeBindingPresetFactoryTest::RunTest(const FString& Parameters)
{
	UDataForgeBindingPresetFactory* Factory = NewObject<UDataForgeBindingPresetFactory>();
	UDataForgeBindingPreset* Preset = Cast<UDataForgeBindingPreset>(Factory->FactoryCreateNew(
		UDataForgeBindingPreset::StaticClass(), GetTransientPackage(), TEXT("BindingPresetFactoryTest"), RF_Transient, nullptr, GWarn));
	TestNotNull(TEXT("Factory creates a Binding Preset"), Preset);
	if (Preset)
	{
		TestTrue(TEXT("Factory assigns stable provenance id"), Preset->PresetId.IsValid());
		TestTrue(TEXT("Factory creates a transactional asset"), Preset->HasAnyFlags(RF_Transactional));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeRenameImpactTest,
	"DataForge.Editor.Automation.RenameImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeRenameImpactTest::RunTest(const FString& Parameters)
{
	const FString TestId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString TestRoot = TEXT("/Game/DataForgeTests/Rename_") + TestId;
	const FString SourceFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("RenameSource_") + TestId + TEXT(".csv"));
	const FString AssociationFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("RenameAssociation_") + TestId + TEXT(".csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SourceFile), true);
	FFileHelper::SaveStringToFile(TEXT("Id\nHero\n"), *SourceFile);

	auto CreateAsset = [](const FString& PackageName, UClass* AssetClass) -> UDataAsset*
	{
		UPackage* Package = CreatePackage(*PackageName);
		UDataAsset* Asset = NewObject<UDataAsset>(Package, AssetClass, *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone | RF_Transient);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	};
	UDataForgeEditorManagedAsset* OldCandidate = CastChecked<UDataForgeEditorManagedAsset>(CreateAsset(TestRoot + TEXT("/OldCandidate"), UDataForgeEditorManagedAsset::StaticClass()));
	UDataForgeEditorManagedAsset* NewCandidate = CastChecked<UDataForgeEditorManagedAsset>(CreateAsset(TestRoot + TEXT("/NewCandidate"), UDataForgeEditorManagedAsset::StaticClass()));
	UDataForgeEditorManagedAsset* ExtraCandidate = CastChecked<UDataForgeEditorManagedAsset>(CreateAsset(TestRoot + TEXT("/ExtraCandidate"), UDataForgeEditorManagedAsset::StaticClass()));
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("Subject,ObjectPath,AssetKind,Role\nHero,%s,Test,Visual\n"), *NewCandidate->GetPathName()), *AssociationFile);

	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.AdapterId = TEXT("Csv");
	RuleSet->Source.File.FilePath = SourceFile;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Schema.bWarnOnUnmappedColumns = false;
	RuleSet->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
	RuleSet->Output.AssetPath = TestRoot + TEXT("/DT_Rename");
	RuleSet->Output.bSaveAfterApply = false;
	FDataForgeAssetRule& ManagedRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	ManagedRule.RuleId = TEXT("Data_Managed");
	ManagedRule.Ownership = EDataForgeAssetOwnership::Managed;
	ManagedRule.BaseFolder = TestRoot + TEXT("/Generated");
	ManagedRule.AssetNamePattern = TEXT("DA_{Id}");
	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("Data");
	Output.AssetClass = UDataForgeEditorRenameTargetAsset::StaticClass();
	Output.AssetRuleId = ManagedRule.RuleId;
	FDataForgeAssociationSourceRule& Association = RuleSet->AssociationSources.AddDefaulted_GetRef();
	Association.SourceId = TEXT("Inventory");
	Association.Source.AdapterId = TEXT("Csv");
	Association.Source.File.FilePath = AssociationFile;

	UDataForgeBindingPreset* Preset = NewObject<UDataForgeBindingPreset>(GetTransientPackage());
	Preset->OutputName = Output.OutputName;
	Preset->TargetClass = UDataForgeEditorRenameTargetAsset::StaticClass();
	FDataForgeBindingPresetSlot& Slot = Preset->Slots.AddDefaulted_GetRef();
	Slot.SlotId = TEXT("Visual");
	Slot.AssetKind = TEXT("Test");
	Slot.Role = TEXT("Visual");
	Slot.TargetProperty = TEXT("Asset");
	Slot.ExpectedAssetClass = UDataForgeEditorManagedAsset::StaticClass();
	RuleSet->BindingPreset = Preset;

	UDataForgeEditorRenameTargetAsset* Existing = CastChecked<UDataForgeEditorRenameTargetAsset>(CreateAsset(ManagedRule.BaseFolder + TEXT("/DA_Hero"), UDataForgeEditorRenameTargetAsset::StaticClass()));
	Existing->Asset = OldCandidate;
	FMetaData& MetaData = Existing->GetPackage()->GetMetaData();
	MetaData.SetValue(Existing, TEXT("DataForge.Managed"), TEXT("true"));
	MetaData.SetValue(Existing, TEXT("DataForge.RuleSetId"), *RuleSet->RuleSetId.ToString(EGuidFormats::Digits));
	MetaData.SetValue(Existing, TEXT("DataForge.RecordId"), TEXT("Hero"));
	MetaData.SetValue(Existing, TEXT("DataForge.Role"), TEXT("Data"));
	MetaData.SetValue(Existing, TEXT("DataForge.RuleVersion"), TEXT("1"));
	MetaData.SetValue(Existing, TEXT("DataForge.Association.Asset"), *OldCandidate->GetPathName());
	MetaData.SetValue(Existing, TEXT("DataForge.Association.Keys"), TEXT("Asset"));

	const FDataForgeRenameImpactEntry Ready = FDataForgeRenameImpactAnalyzer::AnalyzeRuleSet(*RuleSet, FSoftObjectPath(OldCandidate), FSoftObjectPath(NewCandidate));
	TestTrue(TEXT("Convention-compatible rename impact is safe"), Ready.bSuccess);
	TestEqual(TEXT("Rename impact identifies one PDA/DA rebind"), Ready.Rebinds.Num(), 1);
	if (!Ready.Rebinds.IsEmpty()) TestEqual(TEXT("Rename impact identifies the target property"), Ready.Rebinds[0].PropertyPath, FString(TEXT("Asset")));
	TestEqual(TEXT("Impact analysis is mutation-free"), Existing->Asset.Get(), OldCandidate);

	FFileHelper::SaveStringToFile(FString::Printf(TEXT("Subject,ObjectPath,AssetKind,Role\nHero,%s,Test,Visual\nHero,%s,Test,Visual\n"), *NewCandidate->GetPathName(), *ExtraCandidate->GetPathName()), *AssociationFile);
	const FDataForgeRenameImpactEntry Blocked = FDataForgeRenameImpactAnalyzer::AnalyzeRuleSet(*RuleSet, FSoftObjectPath(OldCandidate), FSoftObjectPath(NewCandidate));
	TestFalse(TEXT("Ambiguous One rename impact is blocked"), Blocked.bSuccess);
	TestTrue(TEXT("Blocked rename exposes cardinality diagnostic"), Blocked.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic) { return Diagnostic.Code == TEXT("DF1923"); }));
	TestEqual(TEXT("Blocked analysis preserves the existing binding"), Existing->Asset.Get(), OldCandidate);

	FAssetRegistryModule::AssetDeleted(Existing);
	FAssetRegistryModule::AssetDeleted(ExtraCandidate);
	FAssetRegistryModule::AssetDeleted(NewCandidate);
	FAssetRegistryModule::AssetDeleted(OldCandidate);
	IFileManager::Get().Delete(*AssociationFile, false, true);
	IFileManager::Get().Delete(*SourceFile, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeRenameAdvisorTest,
	"DataForge.Editor.Automation.RenameAdvisor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeRenameAdvisorTest::RunTest(const FString& Parameters)
{
	const FString TestId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString TestRoot = TEXT("/Game/DataForgeTests/Advisor_") + TestId;
	const FString SourceFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("RenameAdvisor_") + TestId + TEXT(".csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SourceFile), true);
	FFileHelper::SaveStringToFile(TEXT("Id\nHero\nVillain\n"), *SourceFile);

	auto CreateManagedAsset = [](const FString& PackageName) -> UDataForgeEditorManagedAsset*
	{
		UPackage* Package = CreatePackage(*PackageName);
		UDataForgeEditorManagedAsset* Asset = NewObject<UDataForgeEditorManagedAsset>(
			Package, *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone | RF_Transient);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	};
	UDataForgeEditorManagedAsset* Selected = CreateManagedAsset(TestRoot + TEXT("/Hero/DataAsset/LegacyName"));

	UDataForgeNamingPolicy* Policy = NewObject<UDataForgeNamingPolicy>(GetTransientPackage());
	Policy->ProjectPrefix = TEXT("CM");
	FDataForgeAssetKindNamingRule& Kind = Policy->AssetKinds.AddDefaulted_GetRef();
	Kind.AssetKind = TEXT("DataAsset");
	Kind.TypePrefix = TEXT("DA");
	Kind.FolderName = TEXT("DataAsset");
	Kind.ExpectedAssetClass = UDataForgeEditorManagedAsset::StaticClass();

	UDataForgeAssetLayoutRecipe* Recipe = NewObject<UDataForgeAssetLayoutRecipe>(GetTransientPackage());
	Recipe->NamingPolicy = Policy;
	Recipe->SubjectSource = EDataForgeLayoutSubjectSource::FolderSegment;
	Recipe->SubjectFolderIndex = 0;
	Recipe->KindFolderIndex = 1;

	UDataForgeFolderSourceConfig* FolderConfig = NewObject<UDataForgeFolderSourceConfig>(GetTransientPackage());
	FolderConfig->RootFolder = TestRoot;
	FolderConfig->LayoutRecipe = Recipe;

	UDataForgeBindingPreset* Preset = NewObject<UDataForgeBindingPreset>(GetTransientPackage());
	FDataForgeBindingPresetSlot& Slot = Preset->Slots.AddDefaulted_GetRef();
	Slot.SlotId = TEXT("IconData");
	Slot.AssetKind = Kind.AssetKind;
	Slot.Role = TEXT("Icon");
	Slot.AssociationSourceId = TEXT("Inventory");
	Slot.TargetProperty = TEXT("Asset");
	Slot.ExpectedAssetClass = UDataForgeEditorManagedAsset::StaticClass();

	UPackage* RuleSetPackage = CreatePackage(*(TestRoot + TEXT("/RS_Advisor")));
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(
		RuleSetPackage, TEXT("RS_Advisor"), RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(RuleSet);
	RuleSet->Source.AdapterId = TEXT("Csv");
	RuleSet->Source.File.FilePath = SourceFile;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->BindingPreset = Preset;
	FDataForgeAssetRule& ManagedRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	ManagedRule.RuleId = TEXT("Data_Managed");
	ManagedRule.Ownership = EDataForgeAssetOwnership::Managed;
	ManagedRule.BaseFolder = TestRoot + TEXT("/Generated");
	ManagedRule.AssetNamePattern = TEXT("DA_{Id}");
	FDataForgeGeneratedAssetOutputRule& GeneratedOutput = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	GeneratedOutput.OutputName = TEXT("Data");
	GeneratedOutput.AssetClass = UDataForgeEditorRenameTargetAsset::StaticClass();
	GeneratedOutput.AssetRuleId = ManagedRule.RuleId;
	FDataForgeAssociationSourceRule& Association = RuleSet->AssociationSources.AddDefaulted_GetRef();
	Association.SourceId = Slot.AssociationSourceId;
	Association.Source.AdapterId = TEXT("AssetRegistryFolder");
	Association.Source.SourceAsset = FolderConfig;

	const TArray<FDataForgeRenameCandidate> Candidates =
		FDataForgeRenameAdvisor::BuildCandidatesForRuleSet(FAssetData(Selected), *RuleSet);
	TestEqual(TEXT("One source record produces one semantic rename candidate"), Candidates.Num(), 1);
	if (Candidates.Num() != 1)
	{
		FAssetRegistryModule::AssetDeleted(Selected);
		IFileManager::Get().Delete(*SourceFile, false, true);
		return false;
	}
	const FDataForgeRenameCandidate& Candidate = Candidates[0];
	const FString ExpectedPath = TestRoot + TEXT("/Hero/DataAsset/DA_CMHeroIcon.DA_CMHeroIcon");
	TestEqual(TEXT("Naming Policy and folder recipe produce the destination"), Candidate.GetSuggestedObjectPath(), ExpectedPath);
	TestEqual(TEXT("Folder evidence selects Hero instead of producing a row cross-product"), Candidate.RecordId, FName(TEXT("Hero")));
	TestTrue(TEXT("A unique evidence-backed candidate is recommended"), Candidate.bRecommended);
	TestTrue(TEXT("A fresh unoccupied proposal passes apply validation"), FDataForgeRenameAdvisor::ValidateForApply(Candidate).bSuccess);

	UDataForgeEditorManagedAsset* SecondSelected = CreateManagedAsset(TestRoot + TEXT("/Hero/DataAsset/AnotherLegacyName"));
	const TArray<FDataForgeRenameCandidate> SecondCandidates =
		FDataForgeRenameAdvisor::BuildCandidatesForRuleSet(FAssetData(SecondSelected), *RuleSet);
	TestEqual(TEXT("The second compatible asset receives the same semantic proposal"), SecondCandidates.Num(), 1);
	if (SecondCandidates.Num() == 1)
	{
		const FDataForgeResult DuplicateDestination = FDataForgeRenameAdvisor::ValidateBatchForApply({ Candidate, SecondCandidates[0] });
		TestFalse(TEXT("A batch cannot send two assets to one destination"), DuplicateDestination.bSuccess);
		TestTrue(TEXT("Duplicate batch destination has a stable diagnostic"), DuplicateDestination.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == TEXT("DF1950");
		}));
	}
	const TArray<FDataForgeRenameCandidate> AuditCandidates =
		FDataForgeRenameAdvisor::BuildCandidates({ FAssetData(Selected), FAssetData(SecondSelected) });
	TestTrue(TEXT("Folder audit marks cross-asset destination claims"), AuditCandidates.ContainsByPredicate([](const FDataForgeRenameCandidate& AuditCandidate)
	{
		return AuditCandidate.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == TEXT("DF1947");
		});
	}));
	FDataForgeRenameCandidate AlternateForSameAsset = Candidate;
	AlternateForSameAsset.SuggestedPackageName = TestRoot + TEXT("/Hero/DataAsset/DA_CMHeroAlternate");
	AlternateForSameAsset.SuggestedAssetName = TEXT("DA_CMHeroAlternate");
	const FDataForgeResult DuplicateSource = FDataForgeRenameAdvisor::ValidateBatchForApply({ Candidate, AlternateForSameAsset });
	TestFalse(TEXT("A batch cannot select two proposals for one asset"), DuplicateSource.bSuccess);
	TestTrue(TEXT("Duplicate batch source has a stable diagnostic"), DuplicateSource.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1949");
	}));

	UDataForgeEditorManagedAsset* ManifestSelected = CreateManagedAsset(TestRoot + TEXT("/Villain/DataAsset/LegacyManifestName"));
	UPackage* ManagedPackage = CreatePackage(*(ManagedRule.BaseFolder + TEXT("/DA_Hero")));
	UDataForgeEditorRenameTargetAsset* ManagedOutput = NewObject<UDataForgeEditorRenameTargetAsset>(
		ManagedPackage, TEXT("DA_Hero"), RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(ManagedOutput);
	FMetaData& ManagedMetaData = ManagedPackage->GetMetaData();
	ManagedMetaData.SetValue(ManagedOutput, TEXT("DataForge.Managed"), TEXT("true"));
	ManagedMetaData.SetValue(ManagedOutput, TEXT("DataForge.RuleSetId"), *RuleSet->RuleSetId.ToString(EGuidFormats::Digits));
	ManagedMetaData.SetValue(ManagedOutput, TEXT("DataForge.RecordId"), TEXT("Hero"));
	ManagedMetaData.SetValue(ManagedOutput, TEXT("DataForge.Association.Asset"), *ManifestSelected->GetPathName());
	ManagedMetaData.SetValue(ManagedOutput, TEXT("DataForge.Association.Keys"), TEXT("Asset"));
	const TArray<FDataForgeRenameCandidate> ManifestCandidates =
		FDataForgeRenameAdvisor::BuildCandidatesForRuleSet(FAssetData(ManifestSelected), *RuleSet);
	TestEqual(TEXT("Association Manifest resolves one candidate despite a conflicting folder subject"), ManifestCandidates.Num(), 1);
	if (ManifestCandidates.Num() == 1)
	{
		TestEqual(TEXT("Association Manifest has priority over folder evidence"), ManifestCandidates[0].RecordId, FName(TEXT("Hero")));
		TestTrue(TEXT("Manifest-backed candidate is recommended"), ManifestCandidates[0].bRecommended);
		TestTrue(TEXT("Manifest evidence is exposed"), ManifestCandidates[0].MatchEvidence.Contains(TEXT("Existing Association Manifest")));
	}

	FFileHelper::SaveStringToFile(TEXT("Id\nHero\nHero\n"), *SourceFile);
	const TArray<FDataForgeRenameCandidate> AmbiguousCandidates =
		FDataForgeRenameAdvisor::BuildCandidatesForRuleSet(FAssetData(Selected), *RuleSet);
	TestEqual(TEXT("Only tied highest-score candidates are retained"), AmbiguousCandidates.Num(), 2);
	TestTrue(TEXT("A tied highest score is blocked instead of guessed"), AmbiguousCandidates.ContainsByPredicate([](const FDataForgeRenameCandidate& Ambiguous)
	{
		return Ambiguous.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == TEXT("DF1955");
		});
	}));
	FFileHelper::SaveStringToFile(TEXT("Id\nHero\nVillain\n"), *SourceFile);
	FString LargeSource = TEXT("Id\n");
	for (int32 Index = 0; Index < 1000; ++Index)
	{
		LargeSource += FString::Printf(TEXT("Other%04d\n"), Index);
	}
	LargeSource += TEXT("Hero\n");
	FFileHelper::SaveStringToFile(LargeSource, *SourceFile);
	const double LargeStart = FPlatformTime::Seconds();
	const TArray<FDataForgeRenameCandidate> LargeCandidates =
		FDataForgeRenameAdvisor::BuildCandidatesForRuleSet(FAssetData(Selected), *RuleSet);
	const double LargeElapsed = FPlatformTime::Seconds() - LargeStart;
	TestEqual(TEXT("One thousand unrelated rows collapse to one evidence-backed candidate"), LargeCandidates.Num(), 1);
	if (LargeCandidates.Num() == 1)
	{
		TestEqual(TEXT("Large-source matching keeps the folder subject"), LargeCandidates[0].RecordId, FName(TEXT("Hero")));
	}
	TestTrue(TEXT("Large-source candidate ranking remains interactive"), LargeElapsed < 2.0);
	FFileHelper::SaveStringToFile(TEXT("Id\nHero\nVillain\n"), *SourceFile);

	UDataForgeEditorManagedAsset* Collision = CreateManagedAsset(TestRoot + TEXT("/Hero/DataAsset/DA_CMHeroIcon"));
	const FDataForgeResult CollisionResult = FDataForgeRenameAdvisor::ValidateForApply(Candidate);
	TestFalse(TEXT("A destination occupied after discovery blocks apply"), CollisionResult.bSuccess);
	TestTrue(TEXT("Late collision has a stable diagnostic"), CollisionResult.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1943");
	}));
	FAssetRegistryModule::AssetDeleted(Collision);

	FFileHelper::SaveStringToFile(TEXT("Id\nOther\n"), *SourceFile);
	const FDataForgeResult ChangedSourceResult = FDataForgeRenameAdvisor::ValidateForApply(Candidate);
	TestFalse(TEXT("A source-of-truth change invalidates an existing proposal"), ChangedSourceResult.bSuccess);
	TestTrue(TEXT("Changed semantics have a stable diagnostic"), ChangedSourceResult.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1946");
	}));

	FDataForgeRenameCandidate Stale = Candidate;
	Stale.AssetPath = FSoftObjectPath(TestRoot + TEXT("/Missing.Missing"));
	const FDataForgeResult StaleResult = FDataForgeRenameAdvisor::ValidateForApply(Stale);
	TestFalse(TEXT("A stale source path blocks apply"), StaleResult.bSuccess);
	TestTrue(TEXT("Stale candidate has a stable diagnostic"), StaleResult.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1941");
	}));

	FAssetRegistryModule::AssetDeleted(Selected);
	FAssetRegistryModule::AssetDeleted(SecondSelected);
	FAssetRegistryModule::AssetDeleted(ManifestSelected);
	FAssetRegistryModule::AssetDeleted(ManagedOutput);
	FAssetRegistryModule::AssetDeleted(RuleSet);
	IFileManager::Get().Delete(*SourceFile, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeRenameRecoveryTest,
	"DataForge.Editor.Automation.RenameRecoveryManifest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeRenameRecoveryTest::RunTest(const FString& Parameters)
{
	const FString TestRoot = TEXT("/Game/DataForgeTests/Recovery_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FDataForgeRenameCandidate Candidate;
	Candidate.AssetPath = FSoftObjectPath(TestRoot + TEXT("/Legacy.Legacy"));
	Candidate.RuleSetPath = FSoftObjectPath(TestRoot + TEXT("/RS_Test.RS_Test"));
	Candidate.RecordId = TEXT("Hero");
	Candidate.SlotId = TEXT("Icon");
	Candidate.SuggestedPackageName = TestRoot + TEXT("/T_CMHeroIcon");
	Candidate.SuggestedAssetName = TEXT("T_CMHeroIcon");

	FDataForgeRenameRecoveryRecord Record;
	FString Error;
	TestTrue(TEXT("Recovery manifest is persisted before mutation"), FDataForgeRenameRecovery::Begin({ Candidate }, Record, Error));
	TestTrue(TEXT("Recovery manifest file exists"), FPaths::FileExists(Record.Filename));

	auto ReadState = [this, &Record, &Candidate](const FString& ExpectedState) -> bool
	{
		FString Json;
		TSharedPtr<FJsonObject> Root;
		const bool bRead = FFileHelper::LoadFileToString(Json, *Record.Filename)
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
			&& Root.IsValid();
		TestTrue(TEXT("Recovery manifest remains valid JSON"), bRead);
		if (!bRead) return false;
		TestEqual(TEXT("Recovery state is updated in place"), Root->GetStringField(TEXT("state")), ExpectedState);
		const TArray<TSharedPtr<FJsonValue>>& Entries = Root->GetArrayField(TEXT("entries"));
		TestEqual(TEXT("Recovery manifest records every rename"), Entries.Num(), 1);
		if (Entries.Num() == 1)
		{
			TestEqual(TEXT("Recovery manifest preserves the source path"),
				Entries[0]->AsObject()->GetStringField(TEXT("sourceObjectPath")),
				Candidate.AssetPath.ToString());
		}
		return true;
	};
	ReadState(TEXT("Planned"));
	TestTrue(TEXT("Recovery manifest records completion"), FDataForgeRenameRecovery::Update(
		Record, TEXT("Succeeded"), false, false, TEXT("Renamed 1 asset(s)."), Error));
	ReadState(TEXT("Succeeded"));

	TArray<FDataForgeDiagnostic> LoadDiagnostics;
	const TArray<FDataForgeRenameRecoveryManifest> Loaded = FDataForgeRenameRecovery::LoadRecent(100, LoadDiagnostics);
	const FDataForgeRenameRecoveryManifest* LoadedManifest = Loaded.FindByPredicate([&Record](const FDataForgeRenameRecoveryManifest& Manifest)
	{
		return Manifest.Filename == Record.Filename;
	});
	TestNotNull(TEXT("Recovery Center discovers the recorded rename"), LoadedManifest);
	if (LoadedManifest)
	{
		TestEqual(TEXT("Recovery Center preserves manifest state"), LoadedManifest->State, FString(TEXT("Succeeded")));
		TestEqual(TEXT("Recovery Center preserves manifest entries"), LoadedManifest->Candidates.Num(), 1);
	}

	UPackage* DestinationPackage = CreatePackage(*Candidate.SuggestedPackageName);
	UDataForgeEditorManagedAsset* Destination = NewObject<UDataForgeEditorManagedAsset>(
		DestinationPackage, *Candidate.SuggestedAssetName, RF_Public | RF_Standalone | RF_Transient);
	FAssetRegistryModule::AssetCreated(Destination);
	FDataForgeRenameRecoveryManifest RestoreManifest;
	RestoreManifest.Filename = Record.Filename;
	RestoreManifest.State = TEXT("Succeeded");
	RestoreManifest.CreatedUtc = Record.CreatedUtc;
	RestoreManifest.Candidates = { Candidate };
	TestTrue(TEXT("A destination-only recorded asset is restorable"), FDataForgeRenameRecovery::ValidateRestore(RestoreManifest).bSuccess);

	UPackage* SourcePackage = CreatePackage(*Candidate.AssetPath.GetLongPackageName());
	UDataForgeEditorManagedAsset* Collision = NewObject<UDataForgeEditorManagedAsset>(
		SourcePackage, *FPackageName::ObjectPathToObjectName(Candidate.AssetPath.ToString()), RF_Public | RF_Standalone | RF_Transient);
	FAssetRegistryModule::AssetCreated(Collision);
	const FDataForgeResult CollisionValidation = FDataForgeRenameRecovery::ValidateRestore(RestoreManifest);
	TestFalse(TEXT("Recovery blocks an occupied original path"), CollisionValidation.bSuccess);
	TestTrue(TEXT("Recovery collision has a stable diagnostic"), CollisionValidation.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1959");
	}));
	FAssetRegistryModule::AssetDeleted(Collision);
	FAssetRegistryModule::AssetDeleted(Destination);
	IFileManager::Get().Delete(*Record.Filename, false, true);
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
	FDataForgeAssociationSourceRule& Association = After->AssociationSources.AddDefaulted_GetRef();
	Association.SourceId = TEXT("Inventory");
	Association.Source.AdapterId = TEXT("Json");
	Association.Source.File.FilePath = TEXT("Inventory.json");
	UDataForgeRuleSet* Dependency = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	After->Dependencies.AddDefaulted_GetRef().RuleSet = Dependency;

	const TArray<FDataForgeSemanticDiffEntry> Entries = FDataForgeRuleSetSemanticDiff::Compare(*Before, *After);
	TestTrue(TEXT("Source file change has a stable semantic path"), Entries.ContainsByPredicate([](const FDataForgeSemanticDiffEntry& Entry) { return Entry.Path == TEXT("Source.File"); }));
	TestTrue(TEXT("Asset rule field change is keyed by RuleId"), Entries.ContainsByPredicate([](const FDataForgeSemanticDiffEntry& Entry) { return Entry.Path == TEXT("AssetRules[B].BaseFolder"); }));
	TestTrue(TEXT("Binding addition is keyed by target"), Entries.ContainsByPredicate([](const FDataForgeSemanticDiffEntry& Entry) { return Entry.Path == TEXT("Bindings[row.Price]") && Entry.Kind == EDataForgeSemanticDiffKind::Added; }));
	TestTrue(TEXT("Association source changes have a stable semantic path"), Entries.ContainsByPredicate([](const FDataForgeSemanticDiffEntry& Entry) { return Entry.Path == TEXT("AssociationSources"); }));
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

	FDataForgeApplyPlan SettledPlan;
	TestTrue(TEXT("Preview after Move succeeds"), FDataForgeEditorService::Preview(*RuleSet, &SettledPlan).bSuccess);
	TestEqual(TEXT("Settled Preview plans no additional Move"), SettledPlan.AssetMoveCount, 0);
	TestEqual(TEXT("Settled Preview plans no generated asset update"), SettledPlan.AssetUpdateCount, 0);
	TestEqual(TEXT("Settled Preview recognizes the managed asset as unchanged"), SettledPlan.AssetUnchangedCount, 1);

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
	RuleSet->ProfileOrigin.ProfileId = FGuid(101, 102, 103, 104);
	RuleSet->ProfileOrigin.MaterializedVersion = 7;
	RuleSet->ProfileOrigin.MaterializedHash = TEXT("profile-revision-hash");
	RuleSet->ProfileOrigin.ParameterValues.Add(TEXT("Zulu"), TEXT("last"));
	RuleSet->ProfileOrigin.ParameterValues.Add(TEXT("Alpha"), TEXT("first"));
	FDataForgeMaterializedRuleOrigin& ProfileRule = RuleSet->ProfileOrigin.Rules.AddDefaulted_GetRef();
	ProfileRule.GroupTemplateId = FGuid(111, 112, 113, 114);
	ProfileRule.RuleTemplateId = FGuid(121, 122, 123, 124);
	ProfileRule.BaselineRule = RuleA;
	RuleA.SubfolderPattern = TEXT("LocalOverride");
	FDataForgeAssociationSourceRule& SnapshotAssociation = RuleSet->AssociationSources.AddDefaulted_GetRef();
	SnapshotAssociation.SourceId = TEXT("Inventory");
	SnapshotAssociation.Source.AdapterId = TEXT("Json");
	SnapshotAssociation.Source.File.FilePath = TEXT("Inventory.json");
	SnapshotAssociation.Source.Parameters.Add(TEXT("Root"), TEXT("/Game/Art"));

	const FString JsonBeforeReorder = FDataForgeRuleSetSnapshot::SerializeJson(*RuleSet);
	const FString YamlBeforeReorder = FDataForgeRuleSetSnapshot::SerializeYaml(*RuleSet);
	RuleSet->Schema.RequiredColumns.Swap(0, 1);
	RuleSet->AssetRules.Swap(0, 1);
	TestEqual(TEXT("JSON snapshot ignores semantically irrelevant array order"), FDataForgeRuleSetSnapshot::SerializeJson(*RuleSet), JsonBeforeReorder);
	TestEqual(TEXT("YAML snapshot ignores semantically irrelevant array order"), FDataForgeRuleSetSnapshot::SerializeYaml(*RuleSet), YamlBeforeReorder);
	TestTrue(TEXT("JSON snapshot contains its schema version"), JsonBeforeReorder.Contains(TEXT("\"snapshotVersion\"")));
	TestTrue(TEXT("JSON snapshot contains Profile provenance"), JsonBeforeReorder.Contains(TEXT("\"profileOrigin\""))
		&& JsonBeforeReorder.Contains(TEXT("profile-revision-hash"))
		&& JsonBeforeReorder.Contains(TEXT("SubfolderPattern")));
	TestTrue(TEXT("YAML snapshot contains Profile provenance"), YamlBeforeReorder.Contains(TEXT("profileOrigin:"))
		&& YamlBeforeReorder.Contains(TEXT("materializedHash: \"profile-revision-hash\""))
		&& YamlBeforeReorder.Contains(TEXT("- \"SubfolderPattern\"")));
	TestTrue(TEXT("JSON snapshot contains Association Sources"), JsonBeforeReorder.Contains(TEXT("\"associationSources\"")) && JsonBeforeReorder.Contains(TEXT("Inventory.json")));
	TestTrue(TEXT("YAML snapshot contains Association Sources"), YamlBeforeReorder.Contains(TEXT("associationSources:")) && YamlBeforeReorder.Contains(TEXT("sourceId: \"Inventory\"")));
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
	TestTrue(TEXT("Valid output advances to Asset Layout"), Workflow.Next(Reason));
	TestEqual(TEXT("Asset Layout step follows Output"), Workflow.GetStep(), EDataForgeWizardStep::AssetLayout);
	TestTrue(TEXT("Manual Asset Layout advances to Asset Rules"), Workflow.Next(Reason));
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
	RuleSet->Output.bRemoveRowsMissingFromSource = true;
	RuleSet->Output.bSaveAfterApply = false;
	FDataForgeAssetRule& ManagedRule = RuleSet->AssetRules.AddDefaulted_GetRef();
	ManagedRule.RuleId = TEXT("Data");
	ManagedRule.Ownership = EDataForgeAssetOwnership::Managed;
	ManagedRule.BaseFolder = TEXT("/Game/DataForgeTests/Automation/Generated/") + Unique;
	ManagedRule.AssetNamePattern = TEXT("DA_{DisplayName}");
	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("data");
	Output.AssetClass = UDataForgeEditorManagedAsset::StaticClass();
	Output.AssetRuleId = ManagedRule.RuleId;
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

	FDataForgeApplyPlan SettledPlan;
	TestTrue(TEXT("Preview after expanded Apply succeeds"), FDataForgeEditorService::Preview(*RuleSet, &SettledPlan).bSuccess);
	TestEqual(TEXT("Repeated Preview plans no row creation"), SettledPlan.CreateCount, 0);
	TestEqual(TEXT("Repeated Preview plans no row update"), SettledPlan.UpdateCount, 0);
	TestEqual(TEXT("Repeated Preview recognizes both rows as unchanged"), SettledPlan.UnchangedCount, 2);
	TestEqual(TEXT("Repeated Preview plans no generated asset creation"), SettledPlan.AssetCreateCount, 0);
	TestEqual(TEXT("Repeated Preview plans no generated asset update"), SettledPlan.AssetUpdateCount, 0);
	TestEqual(TEXT("Repeated Preview recognizes both generated assets as unchanged"), SettledPlan.AssetUnchangedCount, 2);

	TestTrue(TEXT("Contracted source CSV is written"),
		FFileHelper::SaveStringToFile(TEXT("DisplayName,Price\nShield,800\n"), *CsvFilename));
	FDataForgeApplyPlan ContractedPlan;
	TestTrue(TEXT("Contracted source Preview succeeds"), FDataForgeEditorService::Preview(*RuleSet, &ContractedPlan).bSuccess);
	TestEqual(TEXT("Removed source row is planned as one row orphan"), ContractedPlan.OrphanCount, 1);
	TestEqual(TEXT("Remaining source row stays unchanged"), ContractedPlan.UnchangedCount, 1);
	TestEqual(TEXT("Removed source record is planned as one generated asset orphan"), ContractedPlan.AssetOrphanCount, 1);
	TestEqual(TEXT("Remaining generated asset stays unchanged"), ContractedPlan.AssetUnchangedCount, 1);
	TestTrue(TEXT("Row orphan identifies the removed Sword record"), ContractedPlan.Rows.ContainsByPredicate([](const FDataForgePlannedRow& Row)
	{
		return Row.RowName == TEXT("Sword") && Row.Change == EDataForgeRowChange::Orphan;
	}));
	TestTrue(TEXT("Generated asset orphan identifies the removed Sword record"), ContractedPlan.ManagedAssets.ContainsByPredicate([](const FDataForgePlannedAsset& Asset)
	{
		return Asset.RecordId == TEXT("Sword") && Asset.Change == EDataForgeManagedAssetChange::Orphan;
	}));

	Reconciler.Request(*RuleSet, TEXT("source row removed"), 0.0);
	const FDataForgeReconcileBatchResult Contracted = Reconciler.FlushPending(true);
	TestEqual(TEXT("Contracted source processes one RuleSet"), Contracted.ProcessedCount, 1);
	TestEqual(TEXT("Contracted source applies successfully"), Contracted.AppliedCount, 1);
	if (Table)
	{
		TestEqual(TEXT("DataTable removes the missing source row"), Table->GetRowMap().Num(), 1);
		TestNull(TEXT("Removed source row no longer exists"),
			Table->FindRow<FDataForgeEditorAutoMapRow>(TEXT("Sword"), TEXT("source reconcile contraction test")));
		TestNotNull(TEXT("Remaining source row is preserved"),
			Table->FindRow<FDataForgeEditorAutoMapRow>(TEXT("Shield"), TEXT("source reconcile contraction test")));
	}

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgePersistentRenameAuditExampleTest,
	"DataForge.Examples.PersistentRenameAuditAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgePersistentRenameAuditExampleTest::RunTest(const FString& Parameters)
{
	const FString Root = TEXT("/Game/DataForgeExamples/RenameAudit");
	const FString Definitions = Root / TEXT("Definitions");
	const FString Inventory = Root / TEXT("Inventory");
	const FString CsvFile = TEXT("Content/DataForgeExamples/RenameAudit/Source/Characters.csv");

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

	const auto LoadOrCreate = [](UClass* AssetClass, const FString& PackageName) -> UObject*
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		const FString ObjectPath = PackageName + TEXT(".") + AssetName;
		UObject* Asset = StaticFindObject(AssetClass, nullptr, *ObjectPath);
		if (!Asset && FPackageName::DoesPackageExist(PackageName))
		{
			Asset = StaticLoadObject(AssetClass, nullptr, *ObjectPath);
		}
		if (!Asset)
		{
			UPackage* Package = CreatePackage(*PackageName);
			Asset = NewObject<UObject>(Package, AssetClass, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Asset);
		}
		return Asset;
	};

	const auto CreateExampleTexture = [&LoadOrCreate, &SaveAsset](
		const FString& PackageName, const FColor PrimaryColor) -> UTexture2D*
	{
		const bool bAlreadyExists = FPackageName::DoesPackageExist(PackageName);
		UTexture2D* Texture = Cast<UTexture2D>(LoadOrCreate(UTexture2D::StaticClass(), PackageName));
		if (!Texture || bAlreadyExists) return Texture;

		constexpr int32 Size = 8;
		TArray<uint8> Pixels;
		Pixels.SetNumUninitialized(Size * Size * 4);
		for (int32 PixelIndex = 0; PixelIndex < Size * Size; ++PixelIndex)
		{
			const bool bBright = ((PixelIndex % Size) + (PixelIndex / Size)) % 2 == 0;
			const FColor Pixel = bBright ? PrimaryColor : PrimaryColor.ReinterpretAsLinear().Desaturate(0.5f).ToFColor(true);
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
		SaveAsset(Texture, TEXT("Rename Audit example texture"));
		return Texture;
	};

	UDataForgeNamingPolicy* Policy = Cast<UDataForgeNamingPolicy>(LoadOrCreate(
		UDataForgeNamingPolicy::StaticClass(), Definitions / TEXT("NP_RenameAuditExample")));
	Policy->ProjectPrefix = TEXT("CM");
	Policy->AssetKinds.Reset();
	FDataForgeAssetKindNamingRule& Kind = Policy->AssetKinds.AddDefaulted_GetRef();
	Kind.AssetKind = TEXT("Texture");
	Kind.TypePrefix = TEXT("T");
	Kind.FolderName = TEXT("Texture");
	Kind.ExpectedAssetClass = UTexture2D::StaticClass();
	SaveAsset(Policy, TEXT("Rename Audit naming policy"));

	UDataForgeAssetLayoutRecipe* Recipe = Cast<UDataForgeAssetLayoutRecipe>(LoadOrCreate(
		UDataForgeAssetLayoutRecipe::StaticClass(), Definitions / TEXT("ALR_RenameAuditExample")));
	Recipe->RecipeId = TEXT("RenameAuditExample");
	Recipe->Domain = TEXT("Example");
	Recipe->NamingPolicy = Policy;
	Recipe->SubjectSource = EDataForgeLayoutSubjectSource::FolderSegment;
	Recipe->SubjectFolderIndex = 0;
	Recipe->KindFolderIndex = 1;
	Recipe->bRequireKindFolderMatch = true;
	SaveAsset(Recipe, TEXT("Rename Audit layout recipe"));

	UDataForgeFolderSourceConfig* FolderConfig = Cast<UDataForgeFolderSourceConfig>(LoadOrCreate(
		UDataForgeFolderSourceConfig::StaticClass(), Definitions / TEXT("FSC_RenameAuditExample")));
	FolderConfig->RootFolder = Inventory;
	FolderConfig->bRecursive = true;
	FolderConfig->AllowedAssetKinds = { TEXT("Texture") };
	FolderConfig->ExcludedFolders.Reset();
	FolderConfig->bExcludeDataForgeManagedAssets = true;
	FolderConfig->LayoutRecipe = Recipe;
	SaveAsset(FolderConfig, TEXT("Rename Audit folder source"));

	UDataForgeBindingPreset* Preset = Cast<UDataForgeBindingPreset>(LoadOrCreate(
		UDataForgeBindingPreset::StaticClass(), Definitions / TEXT("BP_RenameAuditExample")));
	if (!Preset->PresetId.IsValid()) Preset->PresetId = FGuid::NewGuid();
	Preset->OutputName = TEXT("CharacterData");
	Preset->Slots.Reset();
	FDataForgeBindingPresetSlot& Slot = Preset->Slots.AddDefaulted_GetRef();
	Slot.SlotId = TEXT("Portrait");
	Slot.AssetKind = Kind.AssetKind;
	Slot.Role = TEXT("Portrait");
	Slot.AssociationSourceId = TEXT("Inventory");
	Slot.SourceKeyColumn = TEXT("Id");
	Slot.ExpectedAssetClass = UTexture2D::StaticClass();
	Slot.Cardinality = EDataForgeBindingCardinality::One;
	SaveAsset(Preset, TEXT("Rename Audit binding preset"));

	UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(LoadOrCreate(
		UDataForgeRuleSet::StaticClass(), Definitions / TEXT("RS_RenameAuditExample")));
	if (!RuleSet->RuleSetId.IsValid()) RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source = FDataForgeSourceConfig();
	RuleSet->Source.AdapterId = TEXT("Csv");
	RuleSet->Source.File.FilePath = CsvFile;
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Schema.RequiredColumns = { TEXT("Id") };
	RuleSet->BindingPreset = Preset;
	RuleSet->AssociationSources.Reset();
	FDataForgeAssociationSourceRule& Association = RuleSet->AssociationSources.AddDefaulted_GetRef();
	Association.SourceId = Slot.AssociationSourceId;
	Association.Source.AdapterId = TEXT("AssetRegistryFolder");
	Association.Source.SourceAsset = FolderConfig;
	RuleSet->Output = FDataForgeDataTableOutputRule();
	RuleSet->AssetRules.Reset();
	RuleSet->GeneratedOutputs.Reset();
	RuleSet->Bindings.Reset();
	SaveAsset(RuleSet, TEXT("Rename Audit RuleSet"));

	const FString HeroLegacyPackage = Inventory / TEXT("Hero/Texture/Legacy_HeroPortrait");
	const FString HeroCompliantPackage = Inventory / TEXT("Hero/Texture/T_CMHeroPortrait");
	UTexture2D* HeroTexture = nullptr;
	if (FPackageName::DoesPackageExist(HeroLegacyPackage))
	{
		HeroTexture = LoadObject<UTexture2D>(nullptr, *(HeroLegacyPackage + TEXT(".Legacy_HeroPortrait")));
	}
	else if (FPackageName::DoesPackageExist(HeroCompliantPackage))
	{
		HeroTexture = LoadObject<UTexture2D>(nullptr, *(HeroCompliantPackage + TEXT(".T_CMHeroPortrait")));
	}
	else
	{
		HeroTexture = CreateExampleTexture(HeroLegacyPackage, FColor(45, 135, 255));
	}
	UTexture2D* VillainTexture = CreateExampleTexture(
		Inventory / TEXT("Villain/Texture/T_CMVillainPortrait"), FColor(220, 55, 85));
	TestNotNull(TEXT("Hero example texture exists"), HeroTexture);
	TestNotNull(TEXT("Villain example texture exists"), VillainTexture);

	if (HeroTexture)
	{
		const TArray<FDataForgeRenameCandidate> HeroCandidates =
			FDataForgeRenameAdvisor::BuildCandidatesForRuleSet(FAssetData(HeroTexture), *RuleSet);
		TestEqual(TEXT("Hero has one evidence-backed candidate"), HeroCandidates.Num(), 1);
		if (HeroCandidates.Num() == 1)
		{
			TestEqual(TEXT("Hero target follows the project naming policy"), HeroCandidates[0].GetSuggestedObjectPath(),
				HeroCompliantPackage + TEXT(".T_CMHeroPortrait"));
			TestTrue(TEXT("Hero candidate is recommended"), HeroCandidates[0].bRecommended);
		}
	}
	if (VillainTexture)
	{
		const TArray<FDataForgeRenameCandidate> VillainCandidates =
			FDataForgeRenameAdvisor::BuildCandidatesForRuleSet(FAssetData(VillainTexture), *RuleSet);
		TestEqual(TEXT("Compliant Villain has one evidence-backed candidate"), VillainCandidates.Num(), 1);
		if (VillainCandidates.Num() == 1)
		{
			TestFalse(TEXT("Compliant Villain does not require a rename"), VillainCandidates[0].IsChange());
		}
	}
	if (HeroTexture && VillainTexture)
	{
		const TArray<FDataForgeRenameCandidate> AuditCandidates = FDataForgeRenameAdvisor::BuildCandidates(
			{ FAssetData(HeroTexture), FAssetData(VillainTexture) });
		TestTrue(TEXT("Folder audit discovers the persisted RuleSet for Hero"), AuditCandidates.ContainsByPredicate(
			[HeroTexture](const FDataForgeRenameCandidate& Candidate)
			{
				return Candidate.AssetPath == FSoftObjectPath(HeroTexture) && Candidate.bRecommended;
			}));
		TestTrue(TEXT("Folder audit discovers the persisted RuleSet for Villain"), AuditCandidates.ContainsByPredicate(
			[VillainTexture](const FDataForgeRenameCandidate& Candidate)
			{
				return Candidate.AssetPath == FSoftObjectPath(VillainTexture) && Candidate.bRecommended;
			}));
	}

	return !HasAnyErrors();
}

#endif
