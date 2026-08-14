#include "DataForgeSourceFolderAnalyzer.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace DataForgeSourceFolderAnalyzerTests
{
	FDataForgeDataSet MakePrimaryData()
	{
		FDataForgeDataSet DataSet;
		DataSet.Columns = { TEXT("Id"), TEXT("DisplayName") };
		FDataForgeRow& Armor = DataSet.Rows.AddDefaulted_GetRef();
		Armor.Values.Add(TEXT("Id"), TEXT("Armor"));
		Armor.Values.Add(TEXT("DisplayName"), TEXT("Armor Set"));
		FDataForgeRow& Robot = DataSet.Rows.AddDefaulted_GetRef();
		Robot.Values.Add(TEXT("Id"), TEXT("Robot"));
		Robot.Values.Add(TEXT("DisplayName"), TEXT("Robot Set"));
		return DataSet;
	}

	FDataForgeFolderAssetObservation Observation(const TCHAR* ObjectPath, const TCHAR* PackagePath, const TCHAR* Kind)
	{
		FDataForgeFolderAssetObservation Result;
		Result.ObjectPath = ObjectPath;
		Result.PackagePath = PackagePath;
		Result.AssetKind = Kind;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgePrimaryKeyAnalyzerTest,
	"DataForge.Editor.Authoring.Analysis.PrimaryKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgePrimaryKeyAnalyzerTest::RunTest(const FString& Parameters)
{
	const FDataForgeDataSet DataSet = DataForgeSourceFolderAnalyzerTests::MakePrimaryData();
	const FDataForgePrimaryKeyAnalysis Analysis = FDataForgeSourceFolderAnalyzer::AnalyzePrimaryKey(DataSet);
	TestEqual(TEXT("Exact Id column is selected"), Analysis.Decision.SelectedValue, FString(TEXT("Id")));
	TestEqual(TEXT("Exact Id column has deterministic disposition"),
		Analysis.Decision.Disposition, EDataForgeInferenceDisposition::Exact);

	FDataForgeDataSet Ambiguous;
	Ambiguous.Columns = { TEXT("Code"), TEXT("Token") };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FDataForgeRow& Row = Ambiguous.Rows.AddDefaulted_GetRef();
		Row.Values.Add(TEXT("Code"), FString::FromInt(Index));
		Row.Values.Add(TEXT("Token"), FString::Printf(TEXT("T%d"), Index));
	}
	const FDataForgePrimaryKeyAnalysis AmbiguousAnalysis = FDataForgeSourceFolderAnalyzer::AnalyzePrimaryKey(Ambiguous);
	TestEqual(TEXT("Equal generic unique columns remain ambiguous"),
		AmbiguousAnalysis.Decision.Disposition, EDataForgeInferenceDisposition::Ambiguous);
	TestTrue(TEXT("Ambiguous Primary Key emits stable diagnostic"), AmbiguousAnalysis.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF2004");
	}));

	const FDataForgePrimaryKeyAnalysis MissingPreferred =
		FDataForgeSourceFolderAnalyzer::AnalyzePrimaryKey(DataSet, TEXT("Missing"));
	TestEqual(TEXT("Missing explicit key is a conflict"),
		MissingPreferred.Decision.Disposition, EDataForgeInferenceDisposition::Conflict);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeFolderLayoutAnalyzerTest,
	"DataForge.Editor.Authoring.Analysis.FolderLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeFolderLayoutAnalyzerTest::RunTest(const FString& Parameters)
{
	const FDataForgeDataSet DataSet = DataForgeSourceFolderAnalyzerTests::MakePrimaryData();
	const TArray<FDataForgeFolderAssetObservation> Observations = {
		DataForgeSourceFolderAnalyzerTests::Observation(TEXT("/Game/Test/Armor/Texture/T_CMArmor_1.T_CMArmor_1"), TEXT("/Game/Test/Armor/Texture"), TEXT("Texture")),
		DataForgeSourceFolderAnalyzerTests::Observation(TEXT("/Game/Test/Armor/Material/M_CMArmor_1.M_CMArmor_1"), TEXT("/Game/Test/Armor/Material"), TEXT("Material")),
		DataForgeSourceFolderAnalyzerTests::Observation(TEXT("/Game/Test/Robot/Texture/T_CMRobot_1.T_CMRobot_1"), TEXT("/Game/Test/Robot/Texture"), TEXT("Texture")),
		DataForgeSourceFolderAnalyzerTests::Observation(TEXT("/Game/Test/Robot/Material/M_CMRobot_1.M_CMRobot_1"), TEXT("/Game/Test/Robot/Material"), TEXT("Material"))
	};
	const FDataForgeFolderLayoutAnalysis Analysis =
		FDataForgeSourceFolderAnalyzer::AnalyzeFolderLayout(TEXT("/Game/Test"), DataSet, TEXT("Id"), Observations);
	TestEqual(TEXT("The conventional layout is exact"),
		Analysis.Decision.Disposition, EDataForgeInferenceDisposition::Exact);
	TestEqual(TEXT("The semantic pattern hides folder indexes"),
		Analysis.Decision.SelectedValue, FString(TEXT("{Subject}/{AssetKind}")));
	TestEqual(TEXT("One viable layout is detected"), Analysis.Candidates.Num(), 1);
	if (Analysis.Candidates.Num() == 1)
	{
		TestEqual(TEXT("Subject index is materialization detail zero"), Analysis.Candidates[0].SubjectFolderIndex, 0);
		TestEqual(TEXT("Kind index is materialization detail one"), Analysis.Candidates[0].KindFolderIndex, 1);
	}

	TArray<FDataForgeFolderAssetObservation> Mixed = Observations;
	Mixed.Add(DataForgeSourceFolderAnalyzerTests::Observation(
		TEXT("/Game/Test/Shared/Misc/T_Unrelated.T_Unrelated"), TEXT("/Game/Test/Shared/Misc"), TEXT("Texture")));
	const FDataForgeFolderLayoutAnalysis MixedAnalysis =
		FDataForgeSourceFolderAnalyzer::AnalyzeFolderLayout(TEXT("/Game/Test"), DataSet, TEXT("Id"), Mixed);
	TestEqual(TEXT("A partially mixed root is recommended rather than exact"),
		MixedAnalysis.Decision.Disposition, EDataForgeInferenceDisposition::Recommended);
	return true;
}

#endif
