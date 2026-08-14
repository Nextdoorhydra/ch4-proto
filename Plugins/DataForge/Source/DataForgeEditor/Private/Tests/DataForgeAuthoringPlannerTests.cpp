#include "DataForgeAuthoringPlanner.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataForgeEditorTestTypes.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace DataForgeAuthoringPlannerTests
{
	FDataForgeAuthoringPlannerRequest MakeRequest(bool bReverse)
	{
		FDataForgeAuthoringPlannerRequest Request;
		Request.Intent.PreferredPrimaryKey = TEXT("Id");
		Request.Intent.AssetSearchRoots = { TEXT("/Game/Test") };
		Request.Intent.RowStruct = FDataForgeEditorManyPresetRow::StaticStruct();
		Request.Intent.DataTablePath = TEXT("/Game/Test/DT_AuthoringPlannerMissing");

		FDataForgeRequestedOutput& Output = Request.Intent.Outputs.AddDefaulted_GetRef();
		Output.OutputName = TEXT("Data");
		Output.AssetClass = UDataForgeEditorManyPresetAsset::StaticClass();
		Output.OutputFolder = TEXT("/Game/Test/Generated");

		Request.PrimaryData.Columns = { TEXT("Id"), TEXT("DisplayName") };
		for (const TCHAR* Id : { TEXT("Armor"), TEXT("Robot") })
		{
			FDataForgeRow& Row = Request.PrimaryData.Rows.AddDefaulted_GetRef();
			Row.Values.Add(TEXT("Id"), Id);
			Row.Values.Add(TEXT("DisplayName"), FString(Id) + TEXT(" Set"));
		}

		FDataForgeObservedAssetRoot& Root = Request.AssetRoots.AddDefaulted_GetRef();
		Root.RootFolder = TEXT("/Game/Test/");
		for (const TCHAR* Id : { TEXT("Armor"), TEXT("Robot") })
		{
			FDataForgeFolderAssetObservation& Observation = Root.Observations.AddDefaulted_GetRef();
			Observation.ObjectPath = FString::Printf(TEXT("/Game/Test/%s/Texture/T_CM%s.T_CM%s"), Id, Id, Id);
			Observation.PackagePath = FString::Printf(TEXT("/Game/Test/%s/Texture"), Id);
			Observation.AssetKind = TEXT("Texture");
		}
		if (bReverse)
		{
			Algo::Reverse(Request.PrimaryData.Rows);
			Algo::Reverse(Root.Observations);
		}

		FDataForgeAssociationSchema& Association = Request.AssociationSchemas.AddDefaulted_GetRef();
		Association.SourceId = TEXT("FolderAssets");
		Association.AdapterId = TEXT("AssetRegistryFolder");
		Association.Columns = bReverse
			? TArray<FName>{ TEXT("Role"), TEXT("AssetKind"), TEXT("ObjectPath"), TEXT("Subject") }
			: TArray<FName>{ TEXT("Subject"), TEXT("ObjectPath"), TEXT("AssetKind"), TEXT("Role") };
		return Request;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAuthoringPlannerHappyPathTest,
	"DataForge.Editor.Authoring.Planner.HappyPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAuthoringPlannerHappyPathTest::RunTest(const FString& Parameters)
{
	const FString PlannedPackage = TEXT("/Game/Test/DT_AuthoringPlannerMissing");
	TestNull(TEXT("Planner fixture package does not exist before analysis"), FindPackage(nullptr, *PlannedPackage));
	const FDataForgeAuthoringPlannerResult Result =
		FDataForgeAuthoringPlanner::BuildPlan(DataForgeAuthoringPlannerTests::MakeRequest(false));
	TestNull(TEXT("Planning does not create the DataTable package"), FindPackage(nullptr, *PlannedPackage));
	TestFalse(TEXT("Fully inferred request is not blocked"), Result.Plan.HasBlockingIssues());
	TestEqual(TEXT("One folder is analyzed"), Result.FolderLayouts.Num(), 1);
	TestEqual(TEXT("One output is reflected"), Result.Outputs.Num(), 1);
	TestEqual(TEXT("One association is resolved"), Result.Associations.Num(), 1);
	TestTrue(TEXT("Primary Key decision is present"), Result.Plan.Decisions.ContainsByPredicate([](const FDataForgeAuthoringDecision& Decision)
	{
		return Decision.DecisionId == TEXT("PrimaryKey") && Decision.SelectedValue == TEXT("Id");
	}));
	TestTrue(TEXT("Reflected array slot is present"), Result.Plan.Decisions.ContainsByPredicate([](const FDataForgeAuthoringDecision& Decision)
	{
		return Decision.DecisionId == TEXT("Output.Data.Slot.Textures")
			&& Decision.SelectedValue.Contains(TEXT("Texture|Texture|Textures"));
	}));
	TestEqual(TEXT("Missing DataTable is planned for creation"), Result.Plan.Artifacts.Num(), 1);
	if (Result.Plan.Artifacts.Num() == 1)
	{
		TestEqual(TEXT("DataTable action is Create"), Result.Plan.Artifacts[0].Action, EDataForgeAuthoringArtifactAction::Create);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAuthoringPlannerDeterminismTest,
	"DataForge.Editor.Authoring.Planner.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAuthoringPlannerDeterminismTest::RunTest(const FString& Parameters)
{
	const FDataForgeAuthoringPlannerResult Forward =
		FDataForgeAuthoringPlanner::BuildPlan(DataForgeAuthoringPlannerTests::MakeRequest(false));
	const FDataForgeAuthoringPlannerResult Reverse =
		FDataForgeAuthoringPlanner::BuildPlan(DataForgeAuthoringPlannerTests::MakeRequest(true));
	TestEqual(TEXT("Input ordering does not change the plan signature"),
		Forward.Plan.MakeStableSignature(), Reverse.Plan.MakeStableSignature());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAuthoringPlannerConflictTest,
	"DataForge.Editor.Authoring.Planner.Conflict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAuthoringPlannerConflictTest::RunTest(const FString& Parameters)
{
	FDataForgeAuthoringPlannerRequest Request = DataForgeAuthoringPlannerTests::MakeRequest(false);
	Request.AssociationSchemas[0].Columns.Add(TEXT("Id"));
	const FDataForgeAuthoringPlannerResult Result = FDataForgeAuthoringPlanner::BuildPlan(Request);
	TestTrue(TEXT("Ambiguous Association semantics block materialization"), Result.Plan.HasBlockingIssues());
	TestTrue(TEXT("Ambiguous Association decision is retained"), Result.Plan.Decisions.ContainsByPredicate([](const FDataForgeAuthoringDecision& Decision)
	{
		return Decision.DecisionId == TEXT("Association.FolderAssets.Columns")
			&& Decision.Disposition == EDataForgeInferenceDisposition::Ambiguous;
	}));

	Request = DataForgeAuthoringPlannerTests::MakeRequest(false);
	Request.Intent.Outputs[0].RowReferenceProperty = TEXT("MissingProperty");
	const FDataForgeAuthoringPlannerResult InvalidRow = FDataForgeAuthoringPlanner::BuildPlan(Request);
	TestTrue(TEXT("Invalid explicit Row Reference blocks materialization"), InvalidRow.Plan.HasBlockingIssues());
	TestTrue(TEXT("Invalid explicit Row Reference emits stable diagnostic"), InvalidRow.Plan.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF2050");
	}));
	return true;
}

#endif
