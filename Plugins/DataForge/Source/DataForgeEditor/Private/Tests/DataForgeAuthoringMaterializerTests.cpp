#include "DataForgeAuthoringMaterializer.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataForgeEditorTestTypes.h"
#include "DataForgeBindingPreset.h"
#include "DataForgeFolderSource.h"
#include "DataForgeNamingPolicy.h"
#include "DataForgeRuleSet.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace DataForgeAuthoringMaterializerTests
{
	FDataForgeAuthoringPlannerRequest MakePlannerRequest()
	{
		FDataForgeAuthoringPlannerRequest Request;
		Request.Intent.PreferredPrimaryKey = TEXT("Id");
		Request.Intent.AssetSearchRoots = { TEXT("/Game/Test") };
		Request.Intent.RowStruct = FDataForgeEditorManyPresetRow::StaticStruct();
		Request.Intent.DataTablePath = TEXT("/Game/Test/DT_AuthoringMaterializerMissing");
		FDataForgeRequestedOutput& Output = Request.Intent.Outputs.AddDefaulted_GetRef();
		Output.OutputName = TEXT("Data");
		Output.AssetClass = UDataForgeEditorManyPresetAsset::StaticClass();
		Output.OutputFolder = TEXT("/Game/Test/Generated");

		Request.PrimaryData.Columns = { TEXT("DisplayName"), TEXT("Id") };
		for (const TCHAR* Id : { TEXT("Armor"), TEXT("Robot") })
		{
			FDataForgeRow& Row = Request.PrimaryData.Rows.AddDefaulted_GetRef();
			Row.Values.Add(TEXT("Id"), Id);
			Row.Values.Add(TEXT("DisplayName"), FString(Id) + TEXT(" Set"));
		}

		FDataForgeObservedAssetRoot& Root = Request.AssetRoots.AddDefaulted_GetRef();
		Root.RootFolder = TEXT("/Game/Test");
		for (const TCHAR* Id : { TEXT("Armor"), TEXT("Robot") })
		{
			FDataForgeFolderAssetObservation& Observation = Root.Observations.AddDefaulted_GetRef();
			Observation.ObjectPath = FString::Printf(TEXT("/Game/Test/%s/Texture/T_CM%s.T_CM%s"), Id, Id, Id);
			Observation.PackagePath = FString::Printf(TEXT("/Game/Test/%s/Texture"), Id);
			Observation.AssetKind = TEXT("Texture");
		}

		FDataForgeAssociationSchema& Association = Request.AssociationSchemas.AddDefaulted_GetRef();
		Association.SourceId = TEXT("FolderAssets");
		Association.AdapterId = TEXT("AssetRegistryFolder");
		Association.Columns = { TEXT("Subject"), TEXT("ObjectPath"), TEXT("AssetKind"), TEXT("Role") };
		return Request;
	}

	FDataForgeAuthoringMaterializationRequest MakeMaterializationRequest(UDataForgeNamingPolicy& Policy)
	{
		FDataForgeAuthoringMaterializationRequest Request;
		Request.Planned = FDataForgeAuthoringPlanner::BuildPlan(MakePlannerRequest());
		Request.NamingPolicy = &Policy;
		return Request;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAuthoringMaterializerDraftTest,
	"DataForge.Editor.Authoring.Materializer.Draft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAuthoringMaterializerDraftTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UDataForgeNamingPolicy> Policy(NewObject<UDataForgeNamingPolicy>(GetTransientPackage()));
	FDataForgeNamingPolicyResolver::ConfigureChimeraDefaults(*Policy);
	const FString PlannedPackage = TEXT("/Game/Test/DT_AuthoringMaterializerMissing");
	TestNull(TEXT("Persistent output package is absent before materialization"), FindPackage(nullptr, *PlannedPackage));

	FDataForgeAuthoringDraft Draft = FDataForgeAuthoringMaterializer::BuildDraft(
		DataForgeAuthoringMaterializerTests::MakeMaterializationRequest(*Policy));
	TestTrue(TEXT("Approved plan creates a reviewable draft"), Draft.bSuccess);
	TestNotNull(TEXT("Draft owns a transient RuleSet"), Draft.RuleSet.Get());
	TestNull(TEXT("Draft materialization does not create the persistent output package"), FindPackage(nullptr, *PlannedPackage));
	if (!Draft.RuleSet) return false;

	TestEqual(TEXT("Primary Key is materialized"), Draft.RuleSet->Schema.PrimaryKey, FName(TEXT("Id")));
	TestEqual(TEXT("All probed columns are preserved"), Draft.RuleSet->Schema.RequiredColumns.Num(), 2);
	TestEqual(TEXT("One inferred folder source is materialized"), Draft.FolderSources.Num(), 1);
	TestEqual(TEXT("One inferred layout recipe is materialized"), Draft.LayoutRecipes.Num(), 1);
	TestEqual(TEXT("One Binding Preset is materialized"), Draft.BindingPresets.Num(), 1);
	TestEqual(TEXT("One Association Source is configured"), Draft.RuleSet->AssociationSources.Num(), 1);
	TestEqual(TEXT("One Generated Output is configured"), Draft.RuleSet->GeneratedOutputs.Num(), 1);
	TestEqual(TEXT("One Managed Asset Rule is configured"), Draft.RuleSet->AssetRules.Num(), 1);
	if (Draft.LayoutRecipes.Num() == 1)
	{
		TestEqual(TEXT("Subject folder index comes from analysis"), Draft.LayoutRecipes[0]->SubjectFolderIndex, 0);
		TestEqual(TEXT("Kind folder index comes from analysis"), Draft.LayoutRecipes[0]->KindFolderIndex, 1);
		TestEqual(TEXT("Existing Naming Policy is reused"), Draft.LayoutRecipes[0]->NamingPolicy.Get(), Policy.Get());
	}
	if (Draft.BindingPresets.Num() == 1 && Draft.BindingPresets[0]->Slots.Num() == 1)
	{
		const FDataForgeBindingPresetSlot& Slot = Draft.BindingPresets[0]->Slots[0];
		TestEqual(TEXT("Inferred array slot targets Textures"), Slot.TargetProperty, FString(TEXT("Textures")));
		TestEqual(TEXT("Slot selects the only Association Source"), Slot.AssociationSourceId, FName(TEXT("FolderAssets")));
		TestEqual(TEXT("Slot uses the inferred Primary Key"), Slot.SourceKeyColumn, FName(TEXT("Id")));
	}
	const bool bHasAllDraftObjects = Draft.FolderSources.Num() == 1
		&& Draft.LayoutRecipes.Num() == 1
		&& Draft.BindingPresets.Num() == 1;
	TestTrue(TEXT("All expected draft objects exist"), bHasAllDraftObjects);
	if (bHasAllDraftObjects)
	{
		TestTrue(TEXT("Every generated object remains transient"), Draft.RuleSet->GetPackage() == GetTransientPackage()
			&& Draft.FolderSources[0]->GetPackage() == GetTransientPackage()
			&& Draft.LayoutRecipes[0]->GetPackage() == GetTransientPackage()
			&& Draft.BindingPresets[0]->GetPackage() == GetTransientPackage());
	}

	FDataForgeAuthoringDraft Repeated = FDataForgeAuthoringMaterializer::BuildDraft(
		DataForgeAuthoringMaterializerTests::MakeMaterializationRequest(*Policy));
	TestTrue(TEXT("Repeated materialization also succeeds"), Repeated.bSuccess);
	if (Repeated.RuleSet && Draft.BindingPresets.Num() == 1 && Repeated.BindingPresets.Num() == 1)
	{
		TestEqual(TEXT("Repeated draft has the same semantic summary"), Repeated.Summary, Draft.Summary);
		TestEqual(TEXT("Repeated draft has the same Primary Key"), Repeated.RuleSet->Schema.PrimaryKey, Draft.RuleSet->Schema.PrimaryKey);
		TestEqual(TEXT("Repeated draft has the same Association count"), Repeated.RuleSet->AssociationSources.Num(), Draft.RuleSet->AssociationSources.Num());
		TestEqual(TEXT("Repeated draft has the same slot count"), Repeated.BindingPresets[0]->Slots.Num(), Draft.BindingPresets[0]->Slots.Num());
		if (Repeated.BindingPresets[0]->Slots.Num() == 1 && Draft.BindingPresets[0]->Slots.Num() == 1)
		{
			TestEqual(TEXT("Repeated draft resolves the same slot property"),
				Repeated.BindingPresets[0]->Slots[0].TargetProperty, Draft.BindingPresets[0]->Slots[0].TargetProperty);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAuthoringMaterializerBlockedTest,
	"DataForge.Editor.Authoring.Materializer.BlockedPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAuthoringMaterializerBlockedTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UDataForgeNamingPolicy> Policy(NewObject<UDataForgeNamingPolicy>(GetTransientPackage()));
	FDataForgeNamingPolicyResolver::ConfigureChimeraDefaults(*Policy);
	FDataForgeAuthoringPlannerRequest PlannerRequest = DataForgeAuthoringMaterializerTests::MakePlannerRequest();
	PlannerRequest.AssociationSchemas[0].Columns.Add(TEXT("Id"));
	FDataForgeAuthoringMaterializationRequest Request;
	Request.Planned = FDataForgeAuthoringPlanner::BuildPlan(PlannerRequest);
	Request.NamingPolicy = Policy.Get();

	FDataForgeAuthoringDraft Draft = FDataForgeAuthoringMaterializer::BuildDraft(Request);
	TestFalse(TEXT("Blocked plan cannot create a draft"), Draft.bSuccess);
	TestNull(TEXT("Blocked plan creates no transient RuleSet"), Draft.RuleSet.Get());
	TestTrue(TEXT("Blocked plan emits the materialization boundary diagnostic"), Draft.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF2060");
	}));

	FDataForgeAuthoringMaterializationRequest MissingPolicy;
	MissingPolicy.Planned = FDataForgeAuthoringPlanner::BuildPlan(DataForgeAuthoringMaterializerTests::MakePlannerRequest());
	FDataForgeAuthoringDraft MissingPolicyDraft = FDataForgeAuthoringMaterializer::BuildDraft(MissingPolicy);
	TestFalse(TEXT("Folder materialization cannot invent a Naming Policy"), MissingPolicyDraft.bSuccess);
	TestNull(TEXT("Missing Naming Policy fails before creating a RuleSet"), MissingPolicyDraft.RuleSet.Get());
	TestTrue(TEXT("Missing Naming Policy emits a stable diagnostic"), MissingPolicyDraft.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF2063");
	}));
	return true;
}

#endif
