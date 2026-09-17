#include "DataForgeNamingPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"

namespace DataForgeNamingPolicyTests
{
	UDataForgeNamingPolicy* MakeChimeraPolicy()
	{
		UDataForgeNamingPolicy* Policy = NewObject<UDataForgeNamingPolicy>(GetTransientPackage());
		FDataForgeNamingPolicyResolver::ConfigureChimeraDefaults(*Policy);
		return Policy;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeChimeraNamingRoundTripTest,
	"DataForge.Core.Naming.ChimeraRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeChimeraNamingRoundTripTest::RunTest(const FString& Parameters)
{
	UDataForgeNamingPolicy* Policy = DataForgeNamingPolicyTests::MakeChimeraPolicy();
	TestTrue(TEXT("Chimera Naming Policy is valid"), FDataForgeNamingPolicyResolver::ValidatePolicy(*Policy).bSuccess);
	TestTrue(TEXT("Built-in Chimera Policy exposes the common Unreal prefixes"), Policy->AssetKinds.Num() >= 30);

	struct FCase
	{
		const TCHAR* AssetName;
		const TCHAR* AssetKind;
		const TCHAR* Subject;
		const TCHAR* Role;
		const TCHAR* Variant;
		int32 Numbering;
	};
	const TArray<FCase> Cases = {
		{ TEXT("BP_CMSomeItem"), TEXT("Blueprint"), TEXT("SomeItem"), TEXT(""), TEXT(""), INDEX_NONE },
		{ TEXT("BP_CMSomeItem_2"), TEXT("Blueprint"), TEXT("SomeItem"), TEXT(""), TEXT(""), 2 },
		{ TEXT("T_CMItem_1"), TEXT("Texture"), TEXT("Item"), TEXT(""), TEXT(""), 1 },
		{ TEXT("M_CMObstacleBarricade"), TEXT("Material"), TEXT("Obstacle"), TEXT("Barricade"), TEXT(""), INDEX_NONE },
		{ TEXT("NS_CMObstacleImpact"), TEXT("NiagaraSystem"), TEXT("Obstacle"), TEXT("Impact"), TEXT(""), INDEX_NONE },
		{ TEXT("GA_CMJump"), TEXT("GameplayAbility"), TEXT("Jump"), TEXT(""), TEXT(""), INDEX_NONE },
		{ TEXT("GE_CMInvulnerable"), TEXT("GameplayEffect"), TEXT("Invulnerable"), TEXT(""), TEXT(""), INDEX_NONE }
	};

	for (const FCase& Case : Cases)
	{
		FDataForgeNamingParseContext Context;
		Context.AssetKind = Case.AssetKind;
		Context.Subject = Case.Subject;
		Context.Role = Case.Role;
		const FDataForgeNamingResult Parsed = FDataForgeNamingPolicyResolver::Parse(*Policy, Case.AssetName, Context);
		TestTrue(FString::Printf(TEXT("%s parses"), Case.AssetName), Parsed.bSuccess);
		if (!Parsed.bSuccess) continue;
		TestEqual(TEXT("Parsed Subject is preserved"), Parsed.Identity.Subject, FString(Case.Subject));
		TestEqual(TEXT("Parsed Role is preserved"), Parsed.Identity.Role, FString(Case.Role));
		TestEqual(TEXT("Parsed Variant is preserved"), Parsed.Identity.Variant, FString(Case.Variant));
		TestEqual(TEXT("Parsed Numbering is preserved"), Parsed.Identity.Numbering, Case.Numbering);
		const FDataForgeNamingResult Rebuilt = FDataForgeNamingPolicyResolver::Build(*Policy, Parsed.Identity);
		TestTrue(FString::Printf(TEXT("%s rebuilds"), Case.AssetName), Rebuilt.bSuccess);
		TestEqual(TEXT("Parse and Build round trip is exact"), Rebuilt.AssetName, FString(Case.AssetName));
	}

	FDataForgeNamingParseContext VariantContext;
	VariantContext.AssetKind = TEXT("Material");
	VariantContext.Subject = TEXT("Aria");
	VariantContext.Role = TEXT("Body");
	const FDataForgeNamingResult Variant = FDataForgeNamingPolicyResolver::Parse(*Policy, TEXT("M_CMAriaBodyWinter_3"), VariantContext);
	TestTrue(TEXT("Known Subject and Role expose Variant"), Variant.bSuccess);
	TestEqual(TEXT("Variant is the remaining semantic component"), Variant.Identity.Variant, FString(TEXT("Winter")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataForgeChimeraNamingValidationTest,
	"DataForge.Core.Naming.ChimeraValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataForgeChimeraNamingValidationTest::RunTest(const FString& Parameters)
{
	UDataForgeNamingPolicy* Policy = DataForgeNamingPolicyTests::MakeChimeraPolicy();

	TestFalse(TEXT("Missing CM project prefix is rejected"),
		FDataForgeNamingPolicyResolver::Parse(*Policy, TEXT("T_Item_1")).bSuccess);
	TestFalse(TEXT("Unknown type prefix is rejected"),
		FDataForgeNamingPolicyResolver::Parse(*Policy, TEXT("X_CMItem")).bSuccess);
	TestFalse(TEXT("Leading-zero numbering is rejected"),
		FDataForgeNamingPolicyResolver::Parse(*Policy, TEXT("T_CMItem_01")).bSuccess);

	FDataForgeNamingParseContext WrongSubject;
	WrongSubject.AssetKind = TEXT("Material");
	WrongSubject.Subject = TEXT("Rina");
	TestFalse(TEXT("Known Subject mismatch is rejected"),
		FDataForgeNamingPolicyResolver::Parse(*Policy, TEXT("M_CMAriaBody"), WrongSubject).bSuccess);

	FDataForgeNamingParseContext WrongClass;
	WrongClass.AssetKind = TEXT("Texture");
	WrongClass.Subject = TEXT("Item");
	WrongClass.ActualAssetClass = UMaterialInterface::StaticClass();
	const FDataForgeNamingResult ClassResult = FDataForgeNamingPolicyResolver::Parse(*Policy, TEXT("T_CMItem"), WrongClass);
	TestFalse(TEXT("Asset Kind and loaded class mismatch is rejected"), ClassResult.bSuccess);
	TestTrue(TEXT("Class mismatch has a stable diagnostic"), ClassResult.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == TEXT("DF1708");
	}));

	FDataForgeAssetIdentity InvalidIdentity;
	InvalidIdentity.AssetKind = TEXT("Texture");
	InvalidIdentity.Subject = TEXT("item");
	TestFalse(TEXT("Non-PascalCase Subject cannot be built"),
		FDataForgeNamingPolicyResolver::Build(*Policy, InvalidIdentity).bSuccess);
	return true;
}

#endif
