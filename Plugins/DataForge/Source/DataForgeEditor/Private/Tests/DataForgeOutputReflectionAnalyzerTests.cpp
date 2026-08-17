#include "DataForgeOutputReflectionAnalyzer.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataForgeEditorTestTypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeOutputReflectionAnalyzerTest,
	"DataForge.Editor.Authoring.Analysis.OutputReflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeOutputReflectionAnalyzerTest::RunTest(const FString& Parameters)
{
	const FDataForgeOutputReflectionAnalysis Analysis = FDataForgeOutputReflectionAnalyzer::Analyze(
		UDataForgeEditorManyPresetAsset::StaticClass(), TEXT("Data"), FDataForgeEditorManyPresetRow::StaticStruct());
	TestEqual(TEXT("One array reference becomes one slot"), Analysis.Slots.Num(), 1);
	if (Analysis.Slots.Num() == 1)
	{
		const FDataForgeReflectedSlotCandidate& Slot = Analysis.Slots[0];
		TestEqual(TEXT("Property path is reflected"), Slot.TargetProperty, FString(TEXT("Textures")));
		TestEqual(TEXT("Texture class becomes Texture kind"), Slot.AssetKind, FName(TEXT("Texture")));
		TestEqual(TEXT("Plural property becomes Texture role"), Slot.Role, FName(TEXT("Texture")));
		TestEqual(TEXT("Array reference is Many"), Slot.Cardinality, EDataForgeBindingCardinality::Many);
		TestEqual(TEXT("Managed array safely replaces managed values"), Slot.Reconcile, EDataForgeBindingReconcileMode::ReplaceManaged);
		TestFalse(TEXT("Required cannot be inferred from reflection"), Slot.bRequired);
	}
	TestEqual(TEXT("Output-name row reference is exact"), Analysis.RowReferenceDecision.Disposition, EDataForgeInferenceDisposition::Exact);
	TestEqual(TEXT("Exact row reference keeps spelling"), Analysis.RowReferenceDecision.SelectedValue, FString(TEXT("Data")));

	const FDataForgeOutputReflectionAnalysis Ambiguous = FDataForgeOutputReflectionAnalyzer::Analyze(
		UDataForgeEditorManyPresetAsset::StaticClass(), TEXT("Data"), FDataForgeEditorAmbiguousManyPresetRow::StaticStruct());
	TestEqual(TEXT("Multiple row references remain ambiguous"), Ambiguous.RowReferenceDecision.Disposition, EDataForgeInferenceDisposition::Ambiguous);
	TestTrue(TEXT("Ambiguous row reference emits stable diagnostic"), Ambiguous.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF2032");
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAssociationSemanticsAnalyzerTest,
	"DataForge.Editor.Authoring.Analysis.AssociationSemantics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssociationSemanticsAnalyzerTest::RunTest(const FString& Parameters)
{
	const FDataForgeAssociationSemanticMapping Canonical = FDataForgeOutputReflectionAnalyzer::ResolveAssociationSemantics(
		TEXT("AssetRegistryFolder"), { TEXT("Subject"), TEXT("ObjectPath"), TEXT("AssetKind"), TEXT("Role") });
	TestEqual(TEXT("Canonical folder association is exact"), Canonical.Disposition, EDataForgeInferenceDisposition::Exact);
	TestEqual(TEXT("Canonical match column is preserved"), Canonical.MatchColumn, FName(TEXT("Subject")));

	const FDataForgeAssociationSemanticMapping Aliases = FDataForgeOutputReflectionAnalyzer::ResolveAssociationSemantics(
		TEXT("CSV"), { TEXT("Id"), TEXT("AssetPath"), TEXT("Kind"), TEXT("Slot") });
	TestEqual(TEXT("Unique aliases are recommendations"), Aliases.Disposition, EDataForgeInferenceDisposition::Recommended);
	TestEqual(TEXT("Alias spelling is preserved"), Aliases.AssetPathColumn, FName(TEXT("AssetPath")));

	const FDataForgeAssociationSemanticMapping Ambiguous = FDataForgeOutputReflectionAnalyzer::ResolveAssociationSemantics(
		TEXT("CSV"), { TEXT("Subject"), TEXT("Id"), TEXT("ObjectPath"), TEXT("AssetKind"), TEXT("Role") });
	TestEqual(TEXT("Competing match columns stay ambiguous"), Ambiguous.Disposition, EDataForgeInferenceDisposition::Ambiguous);
	TestTrue(TEXT("Ambiguous semantics emit stable diagnostic"), Ambiguous.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF2041");
	}));
	return true;
}

#endif
