#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeAssetLayoutAuthoring.h"
#include "DataForgeAssetLayoutBatchRebase.h"
#include "DataForgeAssetLayoutProfileFactory.h"
#include "DataForgeProfileValidation.h"
#include "DataForgeRuleSet.h"
#include "DataForgeRuleCreationWorkflow.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/DataForgeEditorTestTypes.h"

namespace DataForgeAssetLayoutProfileEditorTests
{
	UDataForgeAssetLayoutProfile* MakeProfile(const FGuid& ProfileId)
	{
		UDataForgeAssetLayoutProfile* Profile = NewObject<UDataForgeAssetLayoutProfile>(GetTransientPackage());
		Profile->ProfileId = ProfileId;
		FDataForgeProfileParameter& Parameter = Profile->Parameters.AddDefaulted_GetRef();
		Parameter.Name = TEXT("Feature");
		Parameter.Type = EDataForgeProfileParameterType::Name;
		Parameter.bRequired = true;
		FDataForgeLayoutRoot& Root = Profile->Roots.AddDefaulted_GetRef();
		Root.RootId = TEXT("Root");
		Root.PathPattern = TEXT("/Game/${Feature}");
		FDataForgeAssetRuleGroupTemplate& Group = Profile->Groups.AddDefaulted_GetRef();
		Group.TemplateId = FGuid(10, 20, 30, 40);
		Group.GroupId = TEXT("Assets");
		Group.RootId = Root.RootId;
		FDataForgeAssetRuleTemplate& Rule = Group.Rules.AddDefaulted_GetRef();
		Rule.TemplateId = FGuid(11, 21, 31, 41);
		Rule.RuleId = TEXT("Icon");
		Rule.RelativeFolderPattern = TEXT("Textures");
		Rule.AssetNamePattern = TEXT("T_{Id}");
		return Profile;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAssetLayoutProfileFactoryTest,
	"DataForge.Editor.AssetLayoutProfile.Factory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssetLayoutProfileFactoryTest::RunTest(const FString& Parameters)
{
	UDataForgeAssetLayoutProfileFactory* Factory = NewObject<UDataForgeAssetLayoutProfileFactory>(GetTransientPackage());
	UObject* Created = Factory->FactoryCreateNew(
		UDataForgeAssetLayoutProfile::StaticClass(),
		GetTransientPackage(),
		TEXT("AutomationAssetLayoutProfile"),
		RF_Transient,
		nullptr,
		nullptr);
	UDataForgeAssetLayoutProfile* Profile = Cast<UDataForgeAssetLayoutProfile>(Created);
	TestNotNull(TEXT("Factory creates an Asset Layout Profile"), Profile);
	if (Profile)
	{
		TestTrue(TEXT("Factory assigns a stable Profile Id"), Profile->ProfileId.IsValid());
		TestEqual(TEXT("New Profile starts at version 1"), Profile->ProfileVersion, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAssetLayoutProfileAuthoringTest,
	"DataForge.Editor.AssetLayoutProfile.MaterializePreservesOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssetLayoutProfileAuthoringTest::RunTest(const FString& Parameters)
{
	UDataForgeAssetLayoutProfile* Profile = DataForgeAssetLayoutProfileEditorTests::MakeProfile(FGuid(1, 2, 3, 4));
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	FDataForgeAssetRule& Custom = RuleSet->AssetRules.AddDefaulted_GetRef();
	Custom.RuleId = TEXT("ManualMesh");
	Custom.BaseFolder = TEXT("/Game/Manual");
	Custom.AssetNamePattern = TEXT("SK_{Id}");
	const TMap<FName, FString> FirstValues = { { TEXT("Feature"), TEXT("Combat") } };
	const TArray<FName> Columns = { TEXT("Id") };

	const FDataForgeResult First = FDataForgeAssetLayoutAuthoring::Materialize(*RuleSet, *Profile, FirstValues, Columns);
	TestTrue(TEXT("Profile materializes into RuleSet"), First.bSuccess);
	TestEqual(TEXT("Profile and manual rules coexist"), RuleSet->AssetRules.Num(), 2);
	TestEqual(TEXT("Only Profile rule has provenance"), RuleSet->ProfileOrigin.Rules.Num(), 1);
	const FDataForgeAssetLayoutAnalysis InitialAnalysis = FDataForgeAssetLayoutAuthoring::Analyze(*RuleSet);
	TestEqual(TEXT("Manual rule is reported as Custom"), InitialAnalysis.CustomRuleCount, 1);
	TestEqual(TEXT("Custom rule makes layout Modified"), InitialAnalysis.Status, EDataForgeAssetLayoutStatus::Modified);
	TestTrue(TEXT("Per-rule analysis identifies the manual rule"), InitialAnalysis.Rules.ContainsByPredicate([](const FDataForgeAssetLayoutRuleAnalysis& Rule)
	{
		return Rule.RuleId == TEXT("ManualMesh") && Rule.Status == EDataForgeAssetLayoutRuleStatus::Custom;
	}));

	FDataForgeAssetRule* Icon = RuleSet->AssetRules.FindByPredicate([](const FDataForgeAssetRule& Rule) { return Rule.RuleId == TEXT("Icon"); });
	if (!TestNotNull(TEXT("Materialized Icon rule exists"), Icon)) return false;
	Icon->SubfolderPattern = TEXT("LocalOverride");
	Profile->Roots[0].PathPattern = TEXT("/Game/${Feature}/Updated");
	Profile->Groups[0].Rules[0].AssetNamePattern = TEXT("T2_{Id}");
	TestEqual(TEXT("Profile edit is detected as Outdated"), FDataForgeAssetLayoutAuthoring::Analyze(*RuleSet).Status, EDataForgeAssetLayoutStatus::Outdated);

	const TMap<FName, FString> SecondValues = { { TEXT("Feature"), TEXT("Live") } };
	const FDataForgeResult Second = FDataForgeAssetLayoutAuthoring::Materialize(*RuleSet, *Profile, SecondValues, Columns);
	TestTrue(TEXT("Same Profile re-materializes"), Second.bSuccess);
	Icon = RuleSet->AssetRules.FindByPredicate([](const FDataForgeAssetRule& Rule) { return Rule.RuleId == TEXT("Icon"); });
	if (!TestNotNull(TEXT("Re-materialized Icon rule exists"), Icon)) return false;
	TestEqual(TEXT("Unmodified root accepts new Profile and parameter value"), Icon->BaseFolder, FString(TEXT("/Game/Live/Updated")));
	TestEqual(TEXT("Local field override is preserved"), Icon->SubfolderPattern, FString(TEXT("LocalOverride")));
	TestEqual(TEXT("Unmodified name accepts new Profile value"), Icon->AssetNamePattern, FString(TEXT("T2_{Id}")));
	TestNotNull(TEXT("Manual custom rule remains"), RuleSet->AssetRules.FindByPredicate([](const FDataForgeAssetRule& Rule) { return Rule.RuleId == TEXT("ManualMesh"); }));
	const FDataForgeAssetLayoutAnalysis RebasedAnalysis = FDataForgeAssetLayoutAuthoring::Analyze(*RuleSet);
	TestEqual(TEXT("Re-materialized override is reported"), RebasedAnalysis.OverrideFieldCount, 1);
	TestTrue(TEXT("Per-rule analysis identifies the overridden rule"), RebasedAnalysis.Rules.ContainsByPredicate([](const FDataForgeAssetLayoutRuleAnalysis& Rule)
	{
		return Rule.RuleId == TEXT("Icon") && Rule.Status == EDataForgeAssetLayoutRuleStatus::Override && Rule.OverrideFieldCount == 1;
	}));

	UDataForgeAssetLayoutProfile* OtherProfile = DataForgeAssetLayoutProfileEditorTests::MakeProfile(FGuid(5, 6, 7, 8));
	const int32 RuleCountBeforeRejectedSwitch = RuleSet->AssetRules.Num();
	TestFalse(TEXT("Direct switch to another Profile is rejected"), FDataForgeAssetLayoutAuthoring::Materialize(*RuleSet, *OtherProfile, FirstValues, Columns).bSuccess);
	TestEqual(TEXT("Rejected Profile switch does not mutate rules"), RuleSet->AssetRules.Num(), RuleCountBeforeRejectedSwitch);

	FDataForgeAssetLayoutAuthoring::Detach(*RuleSet);
	TestEqual(TEXT("Detach retains all concrete rules"), RuleSet->AssetRules.Num(), 2);
	TestEqual(TEXT("Detached RuleSet is Manual"), FDataForgeAssetLayoutAuthoring::Analyze(*RuleSet).Status, EDataForgeAssetLayoutStatus::Manual);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAssetLayoutProfileRebaseCandidateTest,
	"DataForge.Editor.AssetLayoutProfile.RebaseCandidateIsMutationFreeAndStaleSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssetLayoutProfileRebaseCandidateTest::RunTest(const FString& Parameters)
{
	UDataForgeAssetLayoutProfile* Profile = DataForgeAssetLayoutProfileEditorTests::MakeProfile(FGuid(30, 31, 32, 33));
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	const TMap<FName, FString> Values = { { TEXT("Feature"), TEXT("Items") } };
	const TArray<FName> Columns = { TEXT("Id") };
	TestTrue(TEXT("Initial Profile materializes"), FDataForgeAssetLayoutAuthoring::Materialize(*RuleSet, *Profile, Values, Columns).bSuccess);

	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("Data");
	Output.AssetRuleId = TEXT("Icon");
	FDataForgeBindingRule& Binding = RuleSet->Bindings.AddDefaulted_GetRef();
	Binding.Source = EDataForgeBindingSource::ResolvedAsset;
	Binding.AssetRuleId = TEXT("Icon");
	Binding.TargetProperty = TEXT("Icon");
	FDataForgeAssetRule& Current = RuleSet->AssetRules[0];
	Current.SubfolderPattern = TEXT("LocalOverride");
	Profile->Groups[0].Rules[0].RuleId = TEXT("Portrait");
	Profile->Groups[0].Rules[0].AssetNamePattern = TEXT("P_{Id}");

	const FDataForgeAssetLayoutRebaseCandidate Preview = FDataForgeAssetLayoutAuthoring::PreviewRebase(*RuleSet, *Profile, Values, Columns);
	TestTrue(TEXT("Rebase Preview succeeds"), Preview.bSuccess);
	TestEqual(TEXT("Preview does not mutate the current rule id"), RuleSet->AssetRules[0].RuleId, FName(TEXT("Icon")));
	TestEqual(TEXT("Preview candidate accepts Profile rename"), Preview.AssetRules[0].RuleId, FName(TEXT("Portrait")));
	TestEqual(TEXT("Preview candidate preserves local field override"), Preview.AssetRules[0].SubfolderPattern, FString(TEXT("LocalOverride")));
	TestEqual(TEXT("Generated Output reference follows Profile rename"), Preview.GeneratedOutputs[0].AssetRuleId, FName(TEXT("Portrait")));
	TestEqual(TEXT("Binding reference follows Profile rename"), Preview.Bindings[0].AssetRuleId, FName(TEXT("Portrait")));

	RuleSet->AssetRules[0].BaseFolder = TEXT("/Game/ChangedAfterPreview");
	TestFalse(TEXT("Stale candidate is rejected"), FDataForgeAssetLayoutAuthoring::ApplyRebase(*RuleSet, Preview).bSuccess);
	TestEqual(TEXT("Rejected candidate does not replace current rule"), RuleSet->AssetRules[0].RuleId, FName(TEXT("Icon")));

	const FDataForgeAssetLayoutRebaseCandidate Fresh = FDataForgeAssetLayoutAuthoring::PreviewRebase(*RuleSet, *Profile, Values, Columns);
	TestTrue(TEXT("Fresh candidate applies"), FDataForgeAssetLayoutAuthoring::ApplyRebase(*RuleSet, Fresh).bSuccess);
	TestEqual(TEXT("Applied rule uses renamed id"), RuleSet->AssetRules[0].RuleId, FName(TEXT("Portrait")));
	TestEqual(TEXT("Applied Generated Output reference uses renamed id"), RuleSet->GeneratedOutputs[0].AssetRuleId, FName(TEXT("Portrait")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAssetLayoutProfileReferencedRemovalTest,
	"DataForge.Editor.AssetLayoutProfile.RebaseRejectsReferencedRuleRemoval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssetLayoutProfileReferencedRemovalTest::RunTest(const FString& Parameters)
{
	UDataForgeAssetLayoutProfile* Profile = DataForgeAssetLayoutProfileEditorTests::MakeProfile(FGuid(40, 41, 42, 43));
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	const TMap<FName, FString> Values = { { TEXT("Feature"), TEXT("Items") } };
	const TArray<FName> Columns = { TEXT("Id") };
	TestTrue(TEXT("Initial Profile materializes"), FDataForgeAssetLayoutAuthoring::Materialize(*RuleSet, *Profile, Values, Columns).bSuccess);
	FDataForgeGeneratedAssetOutputRule& Output = RuleSet->GeneratedOutputs.AddDefaulted_GetRef();
	Output.OutputName = TEXT("Data");
	Output.AssetRuleId = TEXT("Icon");
	Profile->Groups[0].Rules.Reset();

	const FDataForgeAssetLayoutRebaseCandidate Preview = FDataForgeAssetLayoutAuthoring::PreviewRebase(*RuleSet, *Profile, Values, Columns);
	TestFalse(TEXT("Referenced removal cannot produce an applicable candidate"), Preview.bSuccess);
	TestTrue(TEXT("Referenced removal reports DF1623"), Preview.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1623");
	}));
	TestEqual(TEXT("Rejected preview never mutates current rules"), RuleSet->AssetRules.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAssetLayoutProfileBatchPreviewTest,
	"DataForge.Editor.AssetLayoutProfile.BatchPreviewAndGlobalCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssetLayoutProfileBatchPreviewTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeAssetLayoutBatch.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("Id,DisplayName\nHero,Hero Name\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the Asset Layout Batch test CSV."));
		return false;
	}

	UDataForgeAssetLayoutProfile* Profile = DataForgeAssetLayoutProfileEditorTests::MakeProfile(FGuid(50, 51, 52, 53));
	const TMap<FName, FString> Values = { { TEXT("Feature"), TEXT("Old") } };
	const TArray<FName> Columns = { TEXT("Id"), TEXT("DisplayName") };
	UDataForgeRuleSet* First = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	UDataForgeRuleSet* Second = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	TArray<UDataForgeRuleSet*> RuleSets = { First, Second };
	for (int32 Index = 0; Index < RuleSets.Num(); ++Index)
	{
		UDataForgeRuleSet* RuleSet = RuleSets[Index];
		RuleSet->RuleSetId = FGuid::NewGuid();
		RuleSet->Source.AdapterId = TEXT("Csv");
		RuleSet->Source.File.FilePath = CsvFilename;
		RuleSet->Schema.PrimaryKey = TEXT("Id");
		RuleSet->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
		RuleSet->Output.AssetPath = FString::Printf(TEXT("/Game/DataForgeTests/DT_Batch%d"), Index);
		RuleSet->Output.bSaveAfterApply = false;
		TestTrue(TEXT("Initial Profile materializes for batch member"), FDataForgeAssetLayoutAuthoring::Materialize(*RuleSet, *Profile, Values, Columns).bSuccess);
	}

	Profile->Roots[0].PathPattern = TEXT("/Game/New/${Feature}");
	const FString OriginalBaseFolder = First->AssetRules[0].BaseFolder;
	FDataForgeAssetLayoutBatchPlan Preview = FDataForgeAssetLayoutBatchRebase::Preview(*Profile, RuleSets);
	TestTrue(TEXT("Batch Preview succeeds for all dependents"), Preview.bSuccess);
	TestEqual(TEXT("Batch Preview contains both RuleSets"), Preview.Entries.Num(), 2);
	TestEqual(TEXT("Batch Preview never mutates concrete rules"), First->AssetRules[0].BaseFolder, OriginalBaseFolder);
	TestEqual(TEXT("Candidate contains the new Profile root"), Preview.Entries[0].Candidate.AssetRules[0].BaseFolder, FString(TEXT("/Game/New/Old")));

	First->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_BatchCollision");
	Second->Output.AssetPath = First->Output.AssetPath;
	FDataForgeAssetLayoutBatchPlan Collision = FDataForgeAssetLayoutBatchRebase::Preview(*Profile, RuleSets);
	TestFalse(TEXT("Cross-RuleSet output collision blocks Batch Preview"), Collision.bSuccess);
	TestTrue(TEXT("Cross-RuleSet collision reports DF1630"), Collision.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1630");
	}));

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAssetLayoutProfileValidationTest,
	"DataForge.Editor.AssetLayoutProfile.ProjectValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssetLayoutProfileValidationTest::RunTest(const FString& Parameters)
{
	UDataForgeAssetLayoutProfile* Profile = DataForgeAssetLayoutProfileEditorTests::MakeProfile(FGuid(60, 61, 62, 63));
	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	const TMap<FName, FString> Values = { { TEXT("Feature"), TEXT("Validation") } };
	const TArray<FName> Columns = { TEXT("Id") };
	TestTrue(TEXT("Validation fixture materializes"), FDataForgeAssetLayoutAuthoring::Materialize(*RuleSet, *Profile, Values, Columns).bSuccess);

	TArray<UDataForgeAssetLayoutProfile*> Profiles = { Profile };
	TArray<const UDataForgeRuleSet*> RuleSets = { RuleSet };
	FDataForgeProfileValidationReport Report = FDataForgeProfileValidation::Validate(Profiles, RuleSets);
	TestTrue(TEXT("Current Profile project validation succeeds"), Report.bSuccess);
	TestEqual(TEXT("Current RuleSet is not outdated"), Report.OutdatedRuleSetCount, 0);

	Profile->Roots[0].PathPattern = TEXT("/Game/Changed/${Feature}");
	Report = FDataForgeProfileValidation::Validate(Profiles, RuleSets);
	TestTrue(TEXT("Outdated Profile is a warning by default"), Report.bSuccess);
	TestEqual(TEXT("Outdated Profile-backed RuleSet is counted"), Report.OutdatedRuleSetCount, 1);
	TestTrue(TEXT("Outdated Profile reports DF1643"), Report.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1643") && Diagnostic.Severity == EDataForgeSeverity::Warning;
	}));

	UDataForgeAssetLayoutProfile* Duplicate = DataForgeAssetLayoutProfileEditorTests::MakeProfile(Profile->ProfileId);
	Profiles.Add(Duplicate);
	Report = FDataForgeProfileValidation::Validate(Profiles, RuleSets);
	TestFalse(TEXT("Duplicate project ProfileId fails validation"), Report.bSuccess);
	TestTrue(TEXT("Duplicate ProfileId reports DF1640"), Report.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1640");
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAssetLayoutProfileWizardTest,
	"DataForge.Editor.AssetLayoutProfile.WizardDraftIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssetLayoutProfileWizardTest::RunTest(const FString& Parameters)
{
	const FString CsvFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DataForgeAssetLayoutProfileWizard.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFilename), true);
	if (!FFileHelper::SaveStringToFile(TEXT("Id,DisplayName\nHero,Hero Name\n"), *CsvFilename))
	{
		AddError(TEXT("Could not create the Asset Layout Profile Wizard test CSV."));
		return false;
	}

	UDataForgeRuleSet* Target = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	Target->RuleSetId = FGuid::NewGuid();
	Target->Source.AdapterId = TEXT("Csv");
	Target->Source.File.FilePath = CsvFilename;
	Target->Output.RowStruct = FDataForgeEditorAutoMapRow::StaticStruct();
	Target->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_ProfileWizard");
	Target->Output.bSaveAfterApply = false;
	UDataForgeAssetLayoutProfile* Profile = DataForgeAssetLayoutProfileEditorTests::MakeProfile(FGuid(20, 21, 22, 23));

	FDataForgeRuleCreationWorkflow Workflow(*Target);
	FString Reason;
	TestTrue(TEXT("Source advances to Probe"), Workflow.Next(Reason));
	TestTrue(TEXT("Wizard Probe succeeds"), Workflow.Probe().bSuccess);
	TestTrue(TEXT("Probe advances to Schema"), Workflow.Next(Reason));
	Workflow.GetDraft().Schema.PrimaryKey = TEXT("Id");
	TestTrue(TEXT("Schema advances to Output"), Workflow.Next(Reason));
	TestTrue(TEXT("Output advances to Asset Layout"), Workflow.Next(Reason));
	TestEqual(TEXT("Profile selection occurs in its own step"), Workflow.GetStep(), EDataForgeWizardStep::AssetLayout);

	Workflow.SelectAssetLayoutProfile(Profile);
	Workflow.SetAssetLayoutParameter(TEXT("Feature"), TEXT("Wizard"));
	TestFalse(TEXT("Profile selection alone cannot advance"), Workflow.CanAdvance(Reason));
	TestEqual(TEXT("Selecting a Profile does not mutate the target"), Target->AssetRules.Num(), 0);
	TestTrue(TEXT("Profile materializes into the transient draft"), Workflow.MaterializeAssetLayout().bSuccess);
	TestEqual(TEXT("Draft receives the concrete Profile rule"), Workflow.GetDraft().AssetRules.Num(), 1);
	TestTrue(TEXT("Draft receives Profile provenance"), Workflow.GetDraft().ProfileOrigin.IsSet());
	TestEqual(TEXT("Target remains unchanged before Finish"), Target->AssetRules.Num(), 0);
	TestFalse(TEXT("Target has no provenance before Finish"), Target->ProfileOrigin.IsSet());

	TestTrue(TEXT("Materialized layout advances to Asset Rules"), Workflow.Next(Reason));
	TestTrue(TEXT("Asset Rules advance to Generated Outputs"), Workflow.Next(Reason));
	TestTrue(TEXT("Generated Outputs advance to Bindings"), Workflow.Next(Reason));
	TestTrue(TEXT("Bindings advance to Preview"), Workflow.Next(Reason));
	TestTrue(TEXT("Profile-backed draft Preview succeeds"), Workflow.Preview().bSuccess);
	TestTrue(TEXT("Finish commits and applies the draft"), Workflow.Finish(Reason));
	TestEqual(TEXT("Finish copies concrete Profile rules to target"), Target->AssetRules.Num(), 1);
	TestTrue(TEXT("Finish copies Profile provenance to target"), Target->ProfileOrigin.IsSet());
	TestEqual(TEXT("Committed parameter value is retained"), Target->ProfileOrigin.ParameterValues.FindRef(TEXT("Feature")), FString(TEXT("Wizard")));

	IFileManager::Get().Delete(*CsvFilename, false, true);
	return true;
}

#endif
