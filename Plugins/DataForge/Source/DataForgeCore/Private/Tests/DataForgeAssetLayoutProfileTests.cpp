#include "DataForgeAssetLayoutProfile.h"
#include "DataForgePipeline.h"
#include "DataForgeRuleSet.h"
#include "Tests/DataForgeTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace DataForgeAssetLayoutProfileTests
{
	class FProfileSourceAdapter final : public IDataForgeSourceAdapter
	{
	public:
		virtual FDataForgeSourceDescriptor Describe() const override
		{
			return { TEXT("AutomationLayoutProfile"), FText::FromString(TEXT("Automation Layout Profile")), TEXT("Test adapter"), FString() };
		}

		virtual bool Probe(const FDataForgeSourceConfig&, FDataForgeDataSet& OutDataSet, TArray<FDataForgeDiagnostic>&) const override
		{
			return Supply(OutDataSet);
		}

		virtual bool Fetch(const FDataForgeSourceConfig&, FDataForgeDataSet& OutDataSet, TArray<FDataForgeDiagnostic>&) const override
		{
			return Supply(OutDataSet);
		}

	private:
		static bool Supply(FDataForgeDataSet& OutDataSet)
		{
			OutDataSet.Columns = { TEXT("Id"), TEXT("Category") };
			FDataForgeRow& Row = OutDataSet.Rows.AddDefaulted_GetRef();
			Row.SourceRow = 2;
			Row.Values.Add(TEXT("Id"), TEXT("Hero"));
			Row.Values.Add(TEXT("Category"), TEXT("Body"));
			OutDataSet.SourceRevision = TEXT("layout-profile-source");
			return true;
		}
	};

	UDataForgeAssetLayoutProfile* MakeValidProfile()
	{
		UDataForgeAssetLayoutProfile* Profile = NewObject<UDataForgeAssetLayoutProfile>(GetTransientPackage());
		Profile->ProfileId = FGuid(1, 2, 3, 4);
		Profile->ProfileVersion = 3;

		FDataForgeProfileParameter& Parameter = Profile->Parameters.AddDefaulted_GetRef();
		Parameter.Name = TEXT("Feature");
		Parameter.Type = EDataForgeProfileParameterType::Name;
		Parameter.bRequired = true;

		FDataForgeLayoutRoot& Root = Profile->Roots.AddDefaulted_GetRef();
		Root.RootId = TEXT("CharacterRoot");
		Root.PathPattern = TEXT("/Game/${Feature}/Characters");

		FDataForgeAssetRuleGroupTemplate& Group = Profile->Groups.AddDefaulted_GetRef();
		Group.TemplateId = FGuid(10, 20, 30, 40);
		Group.GroupId = TEXT("CharacterAssets");
		Group.RootId = Root.RootId;
		Group.GroupSubfolderPattern = TEXT("{Category}");

		FDataForgeAssetRuleTemplate& Rule = Group.Rules.AddDefaulted_GetRef();
		Rule.TemplateId = FGuid(11, 21, 31, 41);
		Rule.RuleId = TEXT("Icon");
		Rule.Ownership = EDataForgeAssetOwnership::External;
		Rule.RelativeFolderPattern = TEXT("Textures");
		Rule.AssetNamePattern = TEXT("T_{Id}");
		return Profile;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAssetLayoutMaterializationTest,
	"DataForge.Core.AssetLayoutProfile.MaterializeAndHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssetLayoutMaterializationTest::RunTest(const FString& Parameters)
{
	UDataForgeAssetLayoutProfile* Profile = DataForgeAssetLayoutProfileTests::MakeValidProfile();
	const TMap<FName, FString> Values = { { TEXT("Feature"), TEXT("Combat") } };
	const TArray<FName> Columns = { TEXT("Id"), TEXT("Category") };
	const FDataForgeAssetLayoutMaterialization First = FDataForgeAssetLayoutMaterializer::Materialize(*Profile, Values, &Columns);
	const FDataForgeAssetLayoutMaterialization Second = FDataForgeAssetLayoutMaterializer::Materialize(*Profile, Values, &Columns);

	TestTrue(TEXT("Valid Profile materializes"), First.bSuccess);
	TestEqual(TEXT("One concrete rule is produced"), First.AssetRules.Num(), 1);
	if (First.AssetRules.Num() == 1)
	{
		TestEqual(TEXT("Profile parameter resolves in root"), First.AssetRules[0].BaseFolder, FString(TEXT("/Game/Combat/Characters")));
		TestEqual(TEXT("Group and rule folders combine"), First.AssetRules[0].SubfolderPattern, FString(TEXT("{Category}/Textures")));
		TestEqual(TEXT("Source token remains for row compilation"), First.AssetRules[0].AssetNamePattern, FString(TEXT("T_{Id}")));
	}
	TestEqual(TEXT("Same Profile has a deterministic hash"), First.Origin.MaterializedHash, Second.Origin.MaterializedHash);
	TestEqual(TEXT("Typed baseline is retained"), First.Origin.Rules.Num(), 1);
	if (First.Origin.Rules.Num() == 1 && First.AssetRules.Num() == 1)
	{
		TestEqual(TEXT("Baseline keeps the concrete Rule Id"), First.Origin.Rules[0].BaselineRule.RuleId, First.AssetRules[0].RuleId);
	}

	const FString OriginalHash = FDataForgeAssetLayoutMaterializer::ComputeLayoutRevisionHash(*Profile);
	Profile->Purpose = TEXT("DifferentDiscoveryLabel");
	Profile->ProfileVersion++;
	TestEqual(TEXT("Discovery metadata and human version do not invalidate layout"), FDataForgeAssetLayoutMaterializer::ComputeLayoutRevisionHash(*Profile), OriginalHash);
	Profile->Groups[0].Rules[0].AssetNamePattern = TEXT("Icon_{Id}");
	TestNotEqual(TEXT("Materialized path changes invalidate layout"), FDataForgeAssetLayoutMaterializer::ComputeLayoutRevisionHash(*Profile), OriginalHash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeAssetLayoutValidationTest,
	"DataForge.Core.AssetLayoutProfile.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeAssetLayoutValidationTest::RunTest(const FString& Parameters)
{
	UDataForgeAssetLayoutProfile* Profile = DataForgeAssetLayoutProfileTests::MakeValidProfile();
	Profile->Groups[0].RootId = TEXT("MissingRoot");
	const FDataForgeAssetRuleTemplate DuplicateRule = Profile->Groups[0].Rules[0];
	Profile->Groups[0].Rules.Add(DuplicateRule);
	const TArray<FName> MissingColumns = { TEXT("ItemId"), TEXT("Category") };
	const FDataForgeAssetLayoutMaterialization Result = FDataForgeAssetLayoutMaterializer::Materialize(*Profile, {}, &MissingColumns);

	TestFalse(TEXT("Invalid Profile is rejected"), Result.bSuccess);
	TestEqual(TEXT("Invalid materialization returns no partial rules"), Result.AssetRules.Num(), 0);
	TestTrue(TEXT("Missing required parameter is diagnosed"), Result.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1604");
	}));
	TestTrue(TEXT("Missing root is diagnosed"), Result.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1616");
	}));
	TestTrue(TEXT("Duplicate rule identity is diagnosed"), Result.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1617") || Diagnostic.Code == TEXT("DF1618");
	}));
	TestTrue(TEXT("Unknown source column is diagnosed"), Result.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1609");
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeMaterializedRuleSetStandaloneTest,
	"DataForge.Core.AssetLayoutProfile.MaterializedRuleSetIsStandalone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeMaterializedRuleSetStandaloneTest::RunTest(const FString& Parameters)
{
	UDataForgeAssetLayoutProfile* Profile = DataForgeAssetLayoutProfileTests::MakeValidProfile();
	const TMap<FName, FString> Values = { { TEXT("Feature"), TEXT("Combat") } };
	const TArray<FName> Columns = { TEXT("Id"), TEXT("Category") };
	const FDataForgeAssetLayoutMaterialization Materialized = FDataForgeAssetLayoutMaterializer::Materialize(*Profile, Values, &Columns);
	if (!TestTrue(TEXT("Test Profile materializes"), Materialized.bSuccess)) return false;

	UDataForgeRuleSet* RuleSet = NewObject<UDataForgeRuleSet>(GetTransientPackage());
	RuleSet->RuleSetId = FGuid::NewGuid();
	RuleSet->Source.AdapterId = TEXT("AutomationLayoutProfile");
	RuleSet->Schema.PrimaryKey = TEXT("Id");
	RuleSet->Output.RowStruct = FDataForgeTestRow::StaticStruct();
	RuleSet->Output.AssetPath = TEXT("/Game/DataForgeTests/DT_LayoutProfile");
	RuleSet->AssetRules = Materialized.AssetRules;
	RuleSet->ProfileOrigin = Materialized.Origin;
	RuleSet->ProfileOrigin.Profile.Reset();

	FDataForgeSourceAdapterRegistry& Registry = FDataForgeSourceAdapterRegistry::Get();
	TestTrue(TEXT("Standalone test adapter registers"), Registry.Register(MakeShared<DataForgeAssetLayoutProfileTests::FProfileSourceAdapter>()));
	FCompiledDataForgeRuleSet Compiled;
	TArray<FDataForgeDiagnostic> Diagnostics;
	const bool bCompiled = FDataForgeCompiler::Compile(*RuleSet, Compiled, Diagnostics);
	Registry.Unregister(TEXT("AutomationLayoutProfile"));

	TestTrue(TEXT("Concrete RuleSet compiles without loading its Profile"), bCompiled);
	TestEqual(TEXT("Concrete asset rule remains available"), Compiled.AssetRules.Num(), 1);
	return true;
}

#endif
