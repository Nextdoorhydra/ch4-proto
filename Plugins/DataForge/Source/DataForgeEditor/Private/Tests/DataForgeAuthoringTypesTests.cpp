#include "DataForgeAuthoringTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataForgeFolderSource.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace DataForgeAuthoringTypesTests
{
	FDataForgeAuthoringPlan MakePlan(bool bReverse)
	{
		FDataForgeAuthoringPlan Plan;
		Plan.Intent.Source.AdapterId = TEXT("Csv");
		Plan.Intent.Source.File.FilePath = TEXT("Content/DataForgeExamples/Source/Items.csv");
		Plan.Intent.Source.Parameters.Add(TEXT("Delimiter"), TEXT(","));
		Plan.Intent.PreferredPrimaryKey = TEXT("Id");
		Plan.Intent.AssetSearchRoots = bReverse
			? TArray<FString>{ TEXT("/Game/Chimera/Character/Body/"), TEXT("/Game/Chimera/Character/Body") }
			: TArray<FString>{ TEXT("/Game/Chimera/Character/Body"), TEXT("/Game/Chimera/Character/Body/") };
		Plan.Intent.DataTablePath = TEXT("/Game/Data/Body/DT_Body/");

		FDataForgeRequestedOutput& Output = Plan.Intent.Outputs.AddDefaulted_GetRef();
		Output.OutputName = TEXT("BodyData");
		Output.OutputFolder = TEXT("/Game/Data/Body");
		FDataForgeAssignmentHint& Hint = Plan.Intent.AssignmentHints.AddDefaulted_GetRef();
		Hint.AssetKind = TEXT("Texture");
		Hint.OutputName = Output.OutputName;
		Hint.TargetProperty = TEXT("Textures");

		FDataForgeAuthoringDecision PrimaryKey;
		PrimaryKey.DecisionId = TEXT("PrimaryKey");
		PrimaryKey.Disposition = EDataForgeInferenceDisposition::Exact;
		PrimaryKey.SelectedValue = TEXT("Id");
		PrimaryKey.Alternatives = { TEXT("Name"), TEXT("Id"), TEXT("Name") };
		PrimaryKey.Evidence = {
			{ TEXT("UniqueValues"), TEXT("100%"), TEXT("Probe") },
			{ TEXT("ColumnName"), TEXT("Id"), TEXT("Schema") }
		};

		FDataForgeAuthoringDecision FolderLayout;
		FolderLayout.DecisionId = TEXT("FolderLayout");
		FolderLayout.Disposition = EDataForgeInferenceDisposition::Recommended;
		FolderLayout.SelectedValue = TEXT("{Subject}/{AssetKind}");
		FolderLayout.Evidence = {
			{ TEXT("KeyCoverage"), TEXT("100%"), TEXT("AssetRegistry") }
		};
		Plan.Decisions = bReverse
			? TArray<FDataForgeAuthoringDecision>{ FolderLayout, PrimaryKey }
			: TArray<FDataForgeAuthoringDecision>{ PrimaryKey, FolderLayout };

		Plan.Artifacts = bReverse
			? TArray<FDataForgeAuthoringArtifact>{
				{ TEXT("RuleSet"), EDataForgeAuthoringArtifactAction::Create, TEXT("/Game/Data/Body/RS_Body") },
				{ TEXT("NamingPolicy"), EDataForgeAuthoringArtifactAction::Reuse, TEXT("/Game/DataForge/NP_CM") } }
			: TArray<FDataForgeAuthoringArtifact>{
				{ TEXT("NamingPolicy"), EDataForgeAuthoringArtifactAction::Reuse, TEXT("/Game/DataForge/NP_CM") },
				{ TEXT("RuleSet"), EDataForgeAuthoringArtifactAction::Create, TEXT("/Game/Data/Body/RS_Body") } };
		return Plan;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAuthoringPlanDeterminismTest,
	"DataForge.Editor.Authoring.Plan.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAuthoringPlanDeterminismTest::RunTest(const FString& Parameters)
{
	FDataForgeAuthoringPlan Forward = DataForgeAuthoringTypesTests::MakePlan(false);
	FDataForgeAuthoringPlan Reverse = DataForgeAuthoringTypesTests::MakePlan(true);
	TestEqual(TEXT("Equivalent plans have the same stable signature"),
		Forward.MakeStableSignature(), Reverse.MakeStableSignature());

	Forward.Normalize();
	TestEqual(TEXT("Duplicate normalized search roots collapse"), Forward.Intent.AssetSearchRoots.Num(), 1);
	TestEqual(TEXT("Folder paths lose trailing separators"),
		Forward.Intent.AssetSearchRoots[0], FString(TEXT("/Game/Chimera/Character/Body")));
	TestEqual(TEXT("Decisions use stable semantic ordering"), Forward.Decisions[0].DecisionId, FName(TEXT("FolderLayout")));
	TestEqual(TEXT("Duplicate alternatives collapse"), Forward.Decisions[1].Alternatives.Num(), 2);
	TestFalse(TEXT("Exact and recommended required decisions are resolved"), Forward.HasBlockingIssues());

	Forward.Decisions[0].Disposition = EDataForgeInferenceDisposition::Ambiguous;
	TestTrue(TEXT("A required ambiguous decision blocks materialization"), Forward.HasBlockingIssues());
	Forward.Decisions[0].bRequired = false;
	TestFalse(TEXT("An optional ambiguous decision does not block materialization"), Forward.HasBlockingIssues());
	FDataForgeDiagnostic& Error = Forward.Diagnostics.AddDefaulted_GetRef();
	Error.Severity = EDataForgeSeverity::Error;
	Error.Code = TEXT("DF2000");
	TestTrue(TEXT("An error diagnostic blocks materialization"), Forward.HasBlockingIssues());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAuthoringPlanMutationTest,
	"DataForge.Editor.Authoring.Plan.MutationFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAuthoringPlanMutationTest::RunTest(const FString& Parameters)
{
	UPackage* ReferencedPackage = CreatePackage(TEXT("/Game/DataForgeTests/Authoring/DA_ReferencedSource"));
	UDataForgeFolderSourceConfig* ReferencedSource = NewObject<UDataForgeFolderSourceConfig>(
		ReferencedPackage, TEXT("DA_ReferencedSource"), RF_Public | RF_Standalone);
	ReferencedPackage->SetDirtyFlag(false);

	FDataForgeAuthoringPlan Plan = DataForgeAuthoringTypesTests::MakePlan(false);
	Plan.Intent.Source.SourceAsset = ReferencedSource;
	Plan.Normalize();
	const FString Signature = Plan.MakeStableSignature();
	TestFalse(TEXT("A normalized plan has a stable signature"), Signature.IsEmpty());
	TestFalse(TEXT("Plan normalization and hashing do not dirty referenced packages"), ReferencedPackage->IsDirty());
	return true;
}

#endif
