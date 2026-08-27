#include "Misc/AutomationTest.h"

#include "Character/Sacrifice/CMSacrificeRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMSacrificeRulesTest,
    "Chimera.AI.Sacrifice.Rules",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMSacrificeRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Sacrifice has five severable parts"),
        FCMSacrificeRules::GetSeverableBodyParts().Num(), 5);

    const float ExpectedDurations[] = { 60.0f, 45.0f, 30.0f, 15.0f, 0.0f };
    for (int32 MissingCount = 1; MissingCount <= 5; ++MissingCount)
    {
        TestEqual(
            *FString::Printf(TEXT("Initial bleed duration for %d parts"),
                MissingCount),
            FCMSacrificeRules::GetInitialBleedDuration(MissingCount),
            ExpectedDurations[MissingCount - 1]);
    }
    TestEqual(TEXT("Two additional parts reduce thirty seconds"),
        FCMSacrificeRules::GetAdditionalBleedReduction(2), 30.0f);

    FRandomStream RandomStream(1337);
    const TArray<ECMBodyPart> AllParts =
        FCMSacrificeRules::GetSeverableBodyParts();
    const ECMBodyPart Selected = FCMSacrificeRules::SelectRandomPart(
        AllParts,
        RandomStream);
    TestTrue(TEXT("One hit selects one available part"),
        AllParts.Contains(Selected));

    FRandomStream EmptyRandomStream(42);
    TestEqual(TEXT("No attached parts produces no selection"),
        FCMSacrificeRules::SelectRandomPart({}, EmptyRandomStream),
        ECMBodyPart::None);
    return true;
}

#endif
