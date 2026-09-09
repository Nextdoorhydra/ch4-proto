#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Player/CMChimera.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMLegInputStrengthTest,
    "Chimera.Input.Leg.HoldStrength",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMLegInputStrengthTest::RunTest(const FString& Parameters)
{
    const ACMChimera* Chimera = GetDefault<ACMChimera>();
    TestNotNull(TEXT("Chimera defaults are available"), Chimera);
    if (!Chimera)
    {
        return false;
    }

    const float ImmediateTap =
        Chimera->GetLegInputStrengthMultiplier(0.0f);
    const float TapBoundary =
        Chimera->GetLegInputStrengthMultiplier(0.04f);
    const float ShortHold =
        Chimera->GetLegInputStrengthMultiplier(0.10f);
    const float MidHold =
        Chimera->GetLegInputStrengthMultiplier(0.16f);
    const float LongHold =
        Chimera->GetLegInputStrengthMultiplier(0.22f);
    const float FullHold =
        Chimera->GetLegInputStrengthMultiplier(0.28f);

    TestTrue(
        TEXT("Immediate taps retain 35 percent strength"),
        FMath::IsNearlyEqual(ImmediateTap, 0.35f));
    TestTrue(
        TEXT("The first 40 ms remains in the tap-strength band"),
        FMath::IsNearlyEqual(TapBoundary, ImmediateTap));
    TestTrue(
        TEXT("Strength rises monotonically through the hold window"),
        ImmediateTap < ShortHold
            && ShortHold < MidHold
            && MidHold < LongHold
            && LongHold < FullHold);
    TestTrue(
        TEXT("The curve reserves its first half for fine adjustment"),
        MidHold < 0.675f);
    TestTrue(
        TEXT("A 280 ms hold reaches full existing strength"),
        FMath::IsNearlyEqual(FullHold, 1.0f));
    TestTrue(
        TEXT("Long holds never exceed full existing strength"),
        FMath::IsNearlyEqual(
            Chimera->GetLegInputStrengthMultiplier(1.0f),
            1.0f));
    TestTrue(
        TEXT("Invalid negative durations clamp to tap strength"),
        FMath::IsNearlyEqual(
            Chimera->GetLegInputStrengthMultiplier(-1.0f),
            ImmediateTap));

    return true;
}

#endif
