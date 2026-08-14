#include "DataForgeRuleCreationWorkflow.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataForgeEditorTestTypes.h"
#include "DataForgeNamingPolicy.h"
#include "DataForgeRuleSet.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAutomaticSetupWorkflowTest,
	"DataForge.Editor.Authoring.AutomaticSetup.Workflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAutomaticSetupWorkflowTest::RunTest(const FString& Parameters)
{
	UDataForgeNamingPolicy* Policy = LoadObject<UDataForgeNamingPolicy>(nullptr,
		TEXT("/Game/DataForgeExamples/MultiAssetRefs/Definitions/NP_MultiAssetRefs.NP_MultiAssetRefs"));
	TestNotNull(TEXT("Reusable example Naming Policy is available"), Policy);
	if (!Policy) return false;

	UDataForgeRuleSet* Target = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	Target->RuleSetId = FGuid::NewGuid();
	Target->Source.AdapterId = TEXT("Csv");
	Target->Source.File.FilePath = FPaths::Combine(
		FPaths::ProjectContentDir(), TEXT("DataForgeExamples/MultiAssetRefs/Source/Products.csv"));
	Target->Output.RowStruct = FDataForgeEditorManyPresetRow::StaticStruct();
	Target->Output.AssetPath = TEXT("/Game/DataForgeTests/AutomaticSetup/DT_Products");
	FDataForgeRuleCreationWorkflow Workflow(*Target);

	const FDataForgeResult Result = Workflow.ConfigureAutomatically(
		TEXT("/Game/DataForgeExamples/MultiAssetRefs/Inventory"),
		nullptr,
		UDataForgeEditorManyPresetAsset::StaticClass(),
		TEXT("/Game/DataForgeTests/AutomaticSetup/Generated"),
		TEXT("/Game/DataForgeTests/AutomaticSetup/Definitions"));
	TestTrue(TEXT("One-click automatic analysis succeeds"), Result.bSuccess);
	TestTrue(TEXT("Workflow retains the automatic Draft"), Workflow.HasAutomaticSetup());
	TestTrue(TEXT("Existing folder definitions are reused"), Result.Summary.Contains(TEXT("folder definitions=reused")));
	TestEqual(TEXT("Primary Key is inferred from the real CSV"), Workflow.GetDraft().Schema.PrimaryKey, FName(TEXT("Id")));
	TestEqual(TEXT("One folder Association Source is generated"), Workflow.GetDraft().AssociationSources.Num(), 1);
	if (Workflow.GetDraft().AssociationSources.Num() == 1)
	{
		TestEqual(TEXT("Association points to the discovered FSC"),
			Workflow.GetDraft().AssociationSources[0].Source.SourceAsset.ToSoftObjectPath().ToString(),
			FString(TEXT("/Game/DataForgeExamples/MultiAssetRefs/Definitions/FSC_MultiAssetRefs.FSC_MultiAssetRefs")));
	}
	TestEqual(TEXT("One generated output is configured"), Workflow.GetDraft().GeneratedOutputs.Num(), 1);
	if (Workflow.GetDraft().GeneratedOutputs.Num() == 1)
	{
		TestTrue(TEXT("Automatic Setup can adopt a compatible legacy unowned PDA"),
			Workflow.GetDraft().GeneratedOutputs[0].bAdoptCompatibleUnownedAsset);
	}
	TestEqual(TEXT("One Managed Asset Rule is configured"), Workflow.GetDraft().AssetRules.Num(), 1);
	const FDataForgeAutomaticSetupDefaults Remembered = Workflow.GetAutomaticSetupDefaults();
	TestEqual(TEXT("Committed folder root can prefill a reopened Wizard"), Remembered.AssetSearchRoot,
		FString(TEXT("/Game/DataForgeExamples/MultiAssetRefs/Inventory")));
	TestEqual(TEXT("Managed output folder can prefill a reopened Wizard"), Remembered.GeneratedOutputFolder,
		FString(TEXT("/Game/DataForgeTests/AutomaticSetup/Generated")));
	TestEqual(TEXT("Generated class can prefill a reopened Wizard"), Remembered.GeneratedOutputClass.Get(),
		UDataForgeEditorManyPresetAsset::StaticClass());
	TestNotNull(TEXT("Discovered Naming Policy can prefill a reopened Wizard"), Remembered.NamingPolicy.Get());
	TestTrue(TEXT("Automatic inspection exposes the inferred result"),
		Workflow.GetAutomaticSetupInspection().Contains(TEXT("Automatic Setup Review")));
	const FString ReviewBeforePreview = Workflow.GetAutomaticSetupInspection();
	TestTrue(TEXT("Review exposes inference decisions"), ReviewBeforePreview.Contains(TEXT("Inferred Decisions")));
	TestTrue(TEXT("Review exposes the DataTable target"), ReviewBeforePreview.Contains(TEXT("DT_Products")));
	TestTrue(TEXT("Review identifies reused folder definitions"), ReviewBeforePreview.Contains(
		TEXT("Reuse - /Game/DataForgeExamples/MultiAssetRefs/Definitions/FSC_MultiAssetRefs")));
	TestTrue(TEXT("Review exposes inferred assignment cardinality"), ReviewBeforePreview.Contains(TEXT("(Many)")));
	FString AdvanceReason;
	for (int32 StepIndex = 0;
		StepIndex < 8 && Workflow.GetStep() != EDataForgeWizardStep::Preview;
		++StepIndex)
	{
		AdvanceReason.Reset();
		if (!TestTrue(TEXT("Automatically configured workflow advances to Preview"), Workflow.Next(AdvanceReason)))
		{
			AddError(AdvanceReason);
			break;
		}
	}
	TestEqual(TEXT("Automatically configured workflow reaches Preview"), Workflow.GetStep(), EDataForgeWizardStep::Preview);
	const FDataForgeResult Preview = Workflow.Preview();
	TestTrue(TEXT("Automatically configured Draft passes the normal Preview"), Preview.bSuccess);
	const FString ReviewAfterPreview = Workflow.GetAutomaticSetupInspection();
	TestTrue(TEXT("Successful Preview exposes concrete effects"), ReviewAfterPreview.Contains(TEXT("Preview Effects")));
	TestTrue(TEXT("Preview effect separates rows and managed assets"),
		ReviewAfterPreview.Contains(TEXT("DataTable Rows")) && ReviewAfterPreview.Contains(TEXT("Managed Assets")));
	AdvanceReason.Reset();
	TestFalse(TEXT("Automatic Setup cannot finish before explicit review approval"), Workflow.CanAdvance(AdvanceReason));
	TestTrue(TEXT("Blocked Finish explains the review requirement"), AdvanceReason.Contains(TEXT("approve")));
	Workflow.SetAutomaticReviewApproved(true);
	TestTrue(TEXT("Successful Preview can be explicitly approved"), Workflow.IsAutomaticReviewApproved());
	AdvanceReason.Reset();
	TestTrue(TEXT("Approved Automatic Setup can finish"), Workflow.CanAdvance(AdvanceReason));
	return true;
}

#endif
