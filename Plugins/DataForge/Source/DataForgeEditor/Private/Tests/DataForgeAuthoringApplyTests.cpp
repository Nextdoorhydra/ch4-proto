#include "DataForgeAuthoringApply.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeBindingPreset.h"
#include "DataForgeEditorTestTypes.h"
#include "DataForgeFolderSource.h"
#include "DataForgeNamingPolicy.h"
#include "DataForgeRuleSet.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace DataForgeAuthoringApplyTests
{
	FDataForgeAuthoringPlannerRequest MakePlannerRequest()
	{
		FDataForgeAuthoringPlannerRequest Request;
		Request.Intent.Source.AdapterId = TEXT("Csv");
		Request.Intent.Source.File.FilePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("DataForgeExamples/Source/Items.csv"));
		Request.Intent.PreferredPrimaryKey = TEXT("Id");
		Request.Intent.AssetSearchRoots = { TEXT("/Game/Test") };
		Request.Intent.RowStruct = FDataForgeEditorManyPresetRow::StaticStruct();
		Request.Intent.DataTablePath = TEXT("/Game/Test/DT_AuthoringApplyPreview");
		FDataForgeRequestedOutput& Output = Request.Intent.Outputs.AddDefaulted_GetRef();
		Output.OutputName = TEXT("Data");
		Output.AssetClass = UDataForgeEditorManyPresetAsset::StaticClass();
		Output.OutputFolder = TEXT("/Game/Test/Generated");

		Request.PrimaryData.Columns = { TEXT("Id"), TEXT("DisplayName"), TEXT("TextureId"), TEXT("Category") };
		for (const TCHAR* Id : { TEXT("1001"), TEXT("1002") })
		{
			FDataForgeRow& Row = Request.PrimaryData.Rows.AddDefaulted_GetRef();
			Row.Values.Add(TEXT("Id"), Id);
			Row.Values.Add(TEXT("DisplayName"), Id);
			Row.Values.Add(TEXT("TextureId"), Id);
			Row.Values.Add(TEXT("Category"), TEXT("Test"));
		}

		FDataForgeObservedAssetRoot& Root = Request.AssetRoots.AddDefaulted_GetRef();
		Root.RootFolder = TEXT("/Game/Test");
		for (const TCHAR* Id : { TEXT("1001"), TEXT("1002") })
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

	void ReleaseCreatedAssets(const TArray<TWeakObjectPtr<UObject>>& Assets)
	{
		for (int32 Index = Assets.Num() - 1; Index >= 0; --Index)
		{
			if (UObject* Asset = Assets[Index].Get())
			{
				FAssetRegistryModule::AssetDeleted(Asset);
				Asset->ClearFlags(RF_Public | RF_Standalone);
				Asset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
				Asset->MarkAsGarbage();
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAuthoringApplyPromotionTest,
	"DataForge.Editor.Authoring.Apply.Promotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAuthoringApplyPromotionTest::RunTest(const FString& Parameters)
{
	UDataForgeNamingPolicy* Policy = LoadObject<UDataForgeNamingPolicy>(nullptr,
		TEXT("/Game/DataForgeExamples/MultiAssetRefs/Definitions/NP_MultiAssetRefs.NP_MultiAssetRefs"));
	TestNotNull(TEXT("Example reusable Naming Policy is available"), Policy);
	if (!Policy) return false;

	FDataForgeAuthoringMaterializationRequest Materialize;
	Materialize.Planned = FDataForgeAuthoringPlanner::BuildPlan(DataForgeAuthoringApplyTests::MakePlannerRequest());
	Materialize.NamingPolicy = Policy;
	FDataForgeAuthoringDraft Draft = FDataForgeAuthoringMaterializer::BuildDraft(Materialize);
	TestTrue(TEXT("Fixture Draft materializes"), Draft.bSuccess);
	if (!Draft.bSuccess) return false;

	const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Root = TEXT("/Game/DataForgeTests/AuthoringApply_") + Unique;
	FDataForgeAuthoringApplyRequest Request;
	Request.Draft = &Draft;
	Request.RuleSetPath = Root + TEXT("/Rules/RS_AutoApply");
	Request.DefinitionFolder = Root + TEXT("/Definitions");
	Request.bSaveAssets = false;
	const FDataForgeAuthoringApplyResult Applied = FDataForgeAuthoringApply::Apply(Request);
	TestTrue(TEXT("Previewed Draft promotes successfully"), Applied.bSuccess);
	TestEqual(TEXT("RuleSet plus three definition assets are promoted"), Applied.CreatedAssets.Num(), 4);
	UDataForgeRuleSet* RuleSet = Applied.RuleSet.Get();
	TestNotNull(TEXT("Promoted RuleSet is returned"), RuleSet);
	if (RuleSet)
	{
		UDataForgeBindingPreset* Preset = RuleSet->BindingPreset.LoadSynchronous();
		TestNotNull(TEXT("RuleSet references the promoted Binding Preset"), Preset);
		TestTrue(TEXT("Binding Preset is persistent rather than transient"), Preset && Preset->GetPackage() != GetTransientPackage());
		TestEqual(TEXT("One promoted folder Association is retained"), RuleSet->AssociationSources.Num(), 1);
		if (RuleSet->AssociationSources.Num() == 1)
		{
			UDataForgeFolderSourceConfig* Folder = Cast<UDataForgeFolderSourceConfig>(RuleSet->AssociationSources[0].Source.SourceAsset.LoadSynchronous());
			TestNotNull(TEXT("Association references the promoted Folder Source"), Folder);
			TestTrue(TEXT("Folder Source is persistent rather than transient"), Folder && Folder->GetPackage() != GetTransientPackage());
			UDataForgeAssetLayoutRecipe* Recipe = Folder ? Folder->LayoutRecipe.LoadSynchronous() : nullptr;
			TestNotNull(TEXT("Folder Source references the promoted Layout Recipe"), Recipe);
			TestTrue(TEXT("Layout Recipe keeps the reusable Naming Policy"), Recipe && Recipe->NamingPolicy.Get() == Policy);
		}
	}
	DataForgeAuthoringApplyTests::ReleaseCreatedAssets(Applied.CreatedAssets);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAuthoringApplyCollisionTest,
	"DataForge.Editor.Authoring.Apply.Collision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAuthoringApplyCollisionTest::RunTest(const FString& Parameters)
{
	UDataForgeNamingPolicy* Policy = LoadObject<UDataForgeNamingPolicy>(nullptr,
		TEXT("/Game/DataForgeExamples/MultiAssetRefs/Definitions/NP_MultiAssetRefs.NP_MultiAssetRefs"));
	if (!Policy) return false;
	FDataForgeAuthoringMaterializationRequest Materialize;
	Materialize.Planned = FDataForgeAuthoringPlanner::BuildPlan(DataForgeAuthoringApplyTests::MakePlannerRequest());
	Materialize.NamingPolicy = Policy;
	FDataForgeAuthoringDraft Draft = FDataForgeAuthoringMaterializer::BuildDraft(Materialize);
	if (!Draft.bSuccess) return false;

	const FString Root = TEXT("/Game/DataForgeTests/AuthoringApplyCollision_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString RuleSetPath = Root + TEXT("/RS_Collision");
	UPackage* OccupiedPackage = CreatePackage(*RuleSetPath);
	UDataForgeRuleSet* Occupied = NewObject<UDataForgeRuleSet>(OccupiedPackage, TEXT("RS_Collision"), RF_Public | RF_Standalone);

	FDataForgeAuthoringApplyRequest Request;
	Request.Draft = &Draft;
	Request.RuleSetPath = RuleSetPath;
	Request.DefinitionFolder = Root + TEXT("/Definitions");
	Request.bSaveAssets = false;
	const FDataForgeAuthoringApplyResult Applied = FDataForgeAuthoringApply::Apply(Request);
	TestFalse(TEXT("Existing target blocks Apply"), Applied.bSuccess);
	TestEqual(TEXT("Collision creates no partial definition assets"), Applied.CreatedAssets.Num(), 0);
	TestTrue(TEXT("Collision emits a stable diagnostic"), Applied.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF2083");
	}));

	Occupied->ClearFlags(RF_Public | RF_Standalone);
	Occupied->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	Occupied->MarkAsGarbage();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAuthoringApplyExistingTargetTest,
	"DataForge.Editor.Authoring.Apply.ExistingWizardTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAuthoringApplyExistingTargetTest::RunTest(const FString& Parameters)
{
	UDataForgeNamingPolicy* Policy = LoadObject<UDataForgeNamingPolicy>(nullptr,
		TEXT("/Game/DataForgeExamples/MultiAssetRefs/Definitions/NP_MultiAssetRefs.NP_MultiAssetRefs"));
	if (!Policy) return false;
	FDataForgeAuthoringMaterializationRequest Materialize;
	Materialize.Planned = FDataForgeAuthoringPlanner::BuildPlan(DataForgeAuthoringApplyTests::MakePlannerRequest());
	Materialize.NamingPolicy = Policy;
	FDataForgeAuthoringDraft Draft = FDataForgeAuthoringMaterializer::BuildDraft(Materialize);
	if (!Draft.bSuccess) return false;

	const FString Root = TEXT("/Game/DataForgeTests/ExistingWizardTarget_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString RuleSetPath = Root + TEXT("/RS_Wizard");
	UPackage* Package = CreatePackage(*RuleSetPath);
	UDataForgeRuleSet* Existing = NewObject<UDataForgeRuleSet>(Package, TEXT("RS_Wizard"), RF_Public | RF_Standalone | RF_Transactional);
	Existing->RuleSetId = FGuid::NewGuid();
	const FGuid ExistingId = Existing->RuleSetId;

	FDataForgeAuthoringApplyRequest Request;
	Request.Draft = &Draft;
	Request.ExistingRuleSet = Existing;
	Request.RuleSetPath = RuleSetPath;
	Request.DefinitionFolder = Root + TEXT("/Definitions");
	Request.bSaveAssets = false;
	const FDataForgeAuthoringApplyResult Applied = FDataForgeAuthoringApply::Apply(Request);
	TestTrue(TEXT("Wizard-owned RuleSet can receive the approved automatic Draft"), Applied.bSuccess);
	TestEqual(TEXT("Only three new Definition assets are created"), Applied.CreatedAssets.Num(), 3);
	TestTrue(TEXT("The existing RuleSet object is preserved"), Applied.RuleSet.Get() == Existing);
	TestEqual(TEXT("The existing RuleSet identity is preserved"), Existing->RuleSetId, ExistingId);
	TestTrue(TEXT("The existing RuleSet references a persistent Binding Preset"),
		Existing->BindingPreset.IsValid() && Existing->BindingPreset.Get()->GetPackage() != GetTransientPackage());

	DataForgeAuthoringApplyTests::ReleaseCreatedAssets(Applied.CreatedAssets);
	Existing->ClearFlags(RF_Public | RF_Standalone);
	Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	Existing->MarkAsGarbage();
	return true;
}

#endif
