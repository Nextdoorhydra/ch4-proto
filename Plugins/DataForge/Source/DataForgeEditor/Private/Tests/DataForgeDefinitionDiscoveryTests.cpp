#include "DataForgeDefinitionDiscovery.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeFolderSource.h"
#include "DataForgeNamingPolicy.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace DataForgeDefinitionDiscoveryTests
{
	void Release(UObject* Asset)
	{
		if (!Asset) return;
		FAssetRegistryModule::AssetDeleted(Asset);
		Asset->ClearFlags(RF_Public | RF_Standalone);
		Asset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		Asset->MarkAsGarbage();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeDefinitionDiscoveryExactTest,
	"DataForge.Editor.Authoring.Discovery.ExactFolderSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeDefinitionDiscoveryExactTest::RunTest(const FString& Parameters)
{
	const FDataForgeDefinitionDiscoveryResult Exact = FDataForgeDefinitionDiscovery::Discover(
		TEXT("/Game/DataForgeExamples/MultiAssetRefs/Inventory"));
	TestTrue(TEXT("Exact example root resolves a reusable chain"), Exact.IsResolved());
	TestEqual(TEXT("Exact root match is deterministic"), Exact.Decision.Disposition, EDataForgeInferenceDisposition::Exact);
	TestNotNull(TEXT("Folder Source is discovered"), Exact.FolderSource.Get());
	TestNotNull(TEXT("Layout Recipe is discovered"), Exact.LayoutRecipe.Get());
	TestNotNull(TEXT("Naming Policy is discovered"), Exact.NamingPolicy.Get());

	UDataForgeNamingPolicy* Override = LoadObject<UDataForgeNamingPolicy>(nullptr,
		TEXT("/Game/DataForgeExamples/RenameAudit/Definitions/NP_RenameAuditExample.NP_RenameAuditExample"));
	TestNotNull(TEXT("Explicit override fixture exists"), Override);
	if (Override)
	{
		const FDataForgeDefinitionDiscoveryResult Overridden = FDataForgeDefinitionDiscovery::Discover(
			TEXT("/Game/DataForgeExamples/MultiAssetRefs/Inventory"), Override);
		TestTrue(TEXT("Explicit valid Naming Policy is accepted"), Overridden.IsResolved());
		TestTrue(TEXT("Explicit policy wins"), Overridden.NamingPolicy.Get() == Override);
		TestNull(TEXT("Incompatible exact FSC is not silently reused"), Overridden.FolderSource.Get());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeDefinitionDiscoveryAmbiguityTest,
	"DataForge.Editor.Authoring.Discovery.AmbiguousFolderSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeDefinitionDiscoveryAmbiguityTest::RunTest(const FString& Parameters)
{
	UDataForgeNamingPolicy* Policy = LoadObject<UDataForgeNamingPolicy>(nullptr,
		TEXT("/Game/DataForgeExamples/MultiAssetRefs/Definitions/NP_MultiAssetRefs.NP_MultiAssetRefs"));
	if (!Policy) return false;
	const FString Root = TEXT("/Game/DataForgeTests/DiscoveryRoot_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UPackage* RecipePackage = CreatePackage(*(Root + TEXT("/ALR_Test")));
	UDataForgeAssetLayoutRecipe* Recipe = NewObject<UDataForgeAssetLayoutRecipe>(RecipePackage, TEXT("ALR_Test"), RF_Public | RF_Standalone);
	Recipe->NamingPolicy = Policy;
	Recipe->SubjectSource = EDataForgeLayoutSubjectSource::FolderSegment;
	Recipe->SubjectFolderIndex = 0;
	Recipe->KindFolderIndex = 1;
	FAssetRegistryModule::AssetCreated(Recipe);
	TArray<UDataForgeFolderSourceConfig*> Configs;
	for (const TCHAR* Name : { TEXT("FSC_A"), TEXT("FSC_B") })
	{
		UPackage* Package = CreatePackage(*(Root + TEXT("/") + Name));
		UDataForgeFolderSourceConfig* Config = NewObject<UDataForgeFolderSourceConfig>(Package, FName(Name), RF_Public | RF_Standalone);
		Config->RootFolder = Root;
		Config->LayoutRecipe = Recipe;
		FAssetRegistryModule::AssetCreated(Config);
		Configs.Add(Config);
	}

	const FDataForgeDefinitionDiscoveryResult Ambiguous = FDataForgeDefinitionDiscovery::Discover(Root);
	TestFalse(TEXT("Multiple exact FSCs are not auto-selected"), Ambiguous.IsResolved());
	TestEqual(TEXT("Multiple exact FSCs remain ambiguous"), Ambiguous.Decision.Disposition, EDataForgeInferenceDisposition::Ambiguous);
	TestEqual(TEXT("Every candidate path is reported"), Ambiguous.Decision.Alternatives.Num(), 2);
	TestTrue(TEXT("Ambiguity emits a stable diagnostic"), Ambiguous.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF2101");
	}));

	for (UDataForgeFolderSourceConfig* Config : Configs) DataForgeDefinitionDiscoveryTests::Release(Config);
	DataForgeDefinitionDiscoveryTests::Release(Recipe);
	return true;
}

#endif
