#include "DataForgeFolderSource.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeNamingPolicy.h"
#include "DataForgePipeline.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "Tests/DataForgeTestTypes.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace DataForgeFolderSourceTests
{
	UObject* CreateAsset(const FString& PackageName, UClass* AssetClass)
	{
		UPackage* Package = CreatePackage(*PackageName);
		UObject* Asset = NewObject<UObject>(Package, AssetClass, *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}

	const FDataForgeRow* FindRow(const FDataForgeDataSet& DataSet, const TCHAR* AssetName)
	{
		return DataSet.Rows.FindByPredicate([AssetName](const FDataForgeRow& Row)
		{
			const FString* Value = Row.Values.Find(TEXT("AssetName"));
			return Value && *Value == AssetName;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAssetRegistryFolderSourceTest,
	"DataForge.Core.Source.AssetRegistryFolder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssetRegistryFolderSourceTest::RunTest(const FString& Parameters)
{
	const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Root = TEXT("/Game/DataForgeTests/FolderInventory/") + Unique;
	DataForgeFolderSourceTests::CreateAsset(Root + TEXT("/Aria/Texture/T_CMAriaIcon_1"), UTexture2D::StaticClass());
	DataForgeFolderSourceTests::CreateAsset(Root + TEXT("/Aria/Material/M_CMAriaBody"), UMaterial::StaticClass());
	DataForgeFolderSourceTests::CreateAsset(Root + TEXT("/Aria/Texture/T_AriaBroken"), UTexture2D::StaticClass());
	DataForgeFolderSourceTests::CreateAsset(Root + TEXT("/Generated/T_CMAriaGenerated"), UTexture2D::StaticClass());
	UObject* Managed = DataForgeFolderSourceTests::CreateAsset(Root + TEXT("/Aria/Material/M_CMAriaManaged"), UDataForgeTestAsset::StaticClass());
	Managed->GetOutermost()->GetMetaData().SetValue(Managed, TEXT("DataForge.Managed"), TEXT("true"));

	UDataForgeNamingPolicy* NamingPolicy = NewObject<UDataForgeNamingPolicy>(GetTransientPackage());
	FDataForgeNamingPolicyResolver::ConfigureChimeraDefaults(*NamingPolicy);
	UDataForgeAssetLayoutRecipe* Recipe = NewObject<UDataForgeAssetLayoutRecipe>(GetTransientPackage());
	FDataForgeAssetLayoutRecipeLibrary::ConfigureChimeraCharacter(*Recipe);
	Recipe->NamingPolicy = NamingPolicy;
	UDataForgeFolderSourceConfig* Config = NewObject<UDataForgeFolderSourceConfig>(GetTransientPackage());
	Config->RootFolder = Root;
	Config->ExcludedFolders = { Root + TEXT("/Generated") };
	Config->LayoutRecipe = Recipe;

	FDataForgeSourceConfig Source;
	Source.AdapterId = TEXT("AssetRegistryFolder");
	Source.SourceAsset = Config;
	FDataForgeAssetRegistryFolderSourceAdapter Adapter;
	FDataForgeDataSet Initial;
	TArray<FDataForgeDiagnostic> InitialDiagnostics;
	TestTrue(TEXT("Folder inventory Fetch succeeds"), Adapter.Fetch(Source, Initial, InitialDiagnostics));
	TestEqual(TEXT("Folder inventory includes valid and invalid project assets only"), Initial.Rows.Num(), 3);
	TestFalse(TEXT("Folder inventory has a deterministic revision"), Initial.SourceRevision.IsEmpty());
	TestNull(TEXT("Excluded output folder is not inventoried"), DataForgeFolderSourceTests::FindRow(Initial, TEXT("T_CMAriaGenerated")));
	TestNull(TEXT("DataForge Managed asset is not inventoried"), DataForgeFolderSourceTests::FindRow(Initial, TEXT("M_CMAriaManaged")));

	const FDataForgeRow* Texture = DataForgeFolderSourceTests::FindRow(Initial, TEXT("T_CMAriaIcon_1"));
	TestNotNull(TEXT("Texture row is inventoried"), Texture);
	if (Texture)
	{
		TestEqual(TEXT("Character folder supplies Subject"), Texture->Values.FindRef(TEXT("Subject")), FString(TEXT("Aria")));
		TestEqual(TEXT("Remaining name supplies Role"), Texture->Values.FindRef(TEXT("Role")), FString(TEXT("Icon")));
		TestEqual(TEXT("Numeric suffix is normalized"), Texture->Values.FindRef(TEXT("Numbering")), FString(TEXT("1")));
		TestEqual(TEXT("Recipe supplies Domain"), Texture->Values.FindRef(TEXT("Domain")), FString(TEXT("Character")));
		TestEqual(TEXT("Matching name and folder are valid"), Texture->Values.FindRef(TEXT("ConventionStatus")), FString(TEXT("Valid")));
	}
	const FDataForgeRow* Invalid = DataForgeFolderSourceTests::FindRow(Initial, TEXT("T_AriaBroken"));
	TestNotNull(TEXT("Convention violation remains visible in inventory"), Invalid);
	if (Invalid)
	{
		TestEqual(TEXT("Known prefix still identifies invalid asset kind"), Invalid->Values.FindRef(TEXT("AssetKind")), FString(TEXT("Texture")));
		TestEqual(TEXT("Convention violation is marked invalid"), Invalid->Values.FindRef(TEXT("ConventionStatus")), FString(TEXT("Invalid")));
	}
	TestTrue(TEXT("Convention violation is reported as a non-blocking warning"), InitialDiagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1807") && Diagnostic.Severity == EDataForgeSeverity::Warning;
	}));

	Source.ProbeRowLimit = 1;
	FDataForgeDataSet Sample;
	TArray<FDataForgeDiagnostic> SampleDiagnostics;
	TestTrue(TEXT("Folder inventory Probe succeeds"), Adapter.Probe(Source, Sample, SampleDiagnostics));
	TestEqual(TEXT("Probe respects its row limit"), Sample.Rows.Num(), 1);
	TestEqual(TEXT("Probe revision covers the complete inventory"), Sample.SourceRevision, Initial.SourceRevision);

	DataForgeFolderSourceTests::CreateAsset(Root + TEXT("/Aria/Niagara/NS_CMAriaHit"), UDataForgeTestAsset::StaticClass());
	FDataForgeDataSet Expanded;
	TArray<FDataForgeDiagnostic> ExpandedDiagnostics;
	TestTrue(TEXT("Expanded folder inventory Fetch succeeds"), Adapter.Fetch(Source, Expanded, ExpandedDiagnostics));
	TestEqual(TEXT("New project asset expands the inventory"), Expanded.Rows.Num(), 4);
	TestNotEqual(TEXT("Adding an asset changes the inventory revision"), Expanded.SourceRevision, Initial.SourceRevision);
	const FDataForgeRow* Niagara = DataForgeFolderSourceTests::FindRow(Expanded, TEXT("NS_CMAriaHit"));
	TestNotNull(TEXT("New Niagara asset is inventoried"), Niagara);
	if (Niagara)
	{
		TestEqual(TEXT("Niagara kind is inferred"), Niagara->Values.FindRef(TEXT("AssetKind")), FString(TEXT("NiagaraSystem")));
		TestEqual(TEXT("Niagara folder matches the Recipe"), Niagara->Values.FindRef(TEXT("ConventionStatus")), FString(TEXT("Valid")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeChimeraLayoutRecipeTest,
	"DataForge.Core.LayoutRecipe.ChimeraDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeChimeraLayoutRecipeTest::RunTest(const FString& Parameters)
{
	UDataForgeAssetLayoutRecipe* Recipe = NewObject<UDataForgeAssetLayoutRecipe>(GetTransientPackage());
	FDataForgeAssetLayoutRecipeLibrary::ConfigureChimeraCharacter(*Recipe);
	TestEqual(TEXT("Character subject comes from the first folder"), Recipe->SubjectSource, EDataForgeLayoutSubjectSource::FolderSegment);
	TestEqual(TEXT("Character kind comes from the second folder"), Recipe->KindFolderIndex, 1);

	FDataForgeAssetLayoutRecipeLibrary::ConfigureChimeraObstacle(*Recipe);
	TestEqual(TEXT("Obstacle uses a fixed semantic subject"), Recipe->FixedSubject, FString(TEXT("Obstacle")));
	TestEqual(TEXT("Obstacle kind comes from the first folder"), Recipe->KindFolderIndex, 0);

	FDataForgeAssetLayoutRecipeLibrary::ConfigureChimeraAbility(*Recipe);
	TestEqual(TEXT("Ability derives the subject from the asset name"), Recipe->SubjectSource, EDataForgeLayoutSubjectSource::AssetName);
	TestEqual(TEXT("Ability GA and GE folders identify the kind"), Recipe->KindFolderIndex, 0);

	FDataForgeAssetLayoutRecipeLibrary::ConfigureChimeraUI(*Recipe);
	TestEqual(TEXT("UI does not force type folders onto its area hierarchy"), Recipe->KindFolderIndex, INDEX_NONE);
	return true;
}

#endif
