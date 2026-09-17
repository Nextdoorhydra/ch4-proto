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

    float TapEndSeconds = 0.0f;
    float NormalEndSeconds = 0.0f;
    float ChargeEndSeconds = 0.0f;
    float OverchargeEndSeconds = 0.0f;
    Chimera->GetLegInputHoldThresholds(
        TapEndSeconds,
        NormalEndSeconds,
        ChargeEndSeconds,
        OverchargeEndSeconds
    );
    TestTrue(
        TEXT("The shared HUD and gameplay thresholds retain their defaults"),
        FMath::IsNearlyEqual(TapEndSeconds, 0.25f)
            && FMath::IsNearlyEqual(NormalEndSeconds, 0.7f)
            && FMath::IsNearlyEqual(ChargeEndSeconds, 1.0f)
            && FMath::IsNearlyEqual(OverchargeEndSeconds, 3.0f));

    const float ImmediateTap =
        Chimera->GetLegInputStrengthMultiplier(0.0f);
    const float TapBoundary =
        Chimera->GetLegInputStrengthMultiplier(0.25f);
    const float NormalMidpoint =
        Chimera->GetLegInputStrengthMultiplier(0.475f);
    const float NormalBoundary =
        Chimera->GetLegInputStrengthMultiplier(0.7f);
    const float ChargeMidpoint =
        Chimera->GetLegInputStrengthMultiplier(0.85f);
    const float FullCharge =
        Chimera->GetLegInputStrengthMultiplier(1.0f);
    const float OverchargeMidpoint =
        Chimera->GetLegInputStrengthMultiplier(2.0f);
    const float FullOvercharge =
        Chimera->GetLegInputStrengthMultiplier(3.0f);

    TestTrue(
        TEXT("Immediate taps retain 35 percent strength"),
        FMath::IsNearlyEqual(ImmediateTap, 0.35f));
    TestTrue(
        TEXT("The first 250 ms remains in the tap-strength band"),
        FMath::IsNearlyEqual(TapBoundary, ImmediateTap));
    TestTrue(
        TEXT("Normal strength rises from 35 to 100 percent"),
        TapBoundary < NormalMidpoint
            && NormalMidpoint < NormalBoundary
            && FMath::IsNearlyEqual(NormalBoundary, 1.0f));
    TestTrue(
        TEXT("The Normal window uses the configured ease-out curve"),
        NormalMidpoint > 0.675f);
    TestTrue(
        TEXT("Charging strength rises from 100 to 200 percent"),
        NormalBoundary < ChargeMidpoint
            && ChargeMidpoint < FullCharge
            && FMath::IsNearlyEqual(FullCharge, 2.0f));
    TestTrue(
        TEXT("Overcharging uses a late-rising cubic curve"),
        FullCharge < OverchargeMidpoint
            && OverchargeMidpoint < FullOvercharge
            && FMath::IsNearlyEqual(OverchargeMidpoint, 3.0f)
            && FullOvercharge - OverchargeMidpoint
                > OverchargeMidpoint - FullCharge
            && FMath::IsNearlyEqual(FullOvercharge, 10.0f));
    TestTrue(
        TEXT("Long holds remain at 1000 percent after full overcharge"),
        FMath::IsNearlyEqual(
            Chimera->GetLegInputStrengthMultiplier(4.0f),
            10.0f));
    TestTrue(
        TEXT("Invalid negative durations clamp to tap strength"),
        FMath::IsNearlyEqual(
            Chimera->GetLegInputStrengthMultiplier(-1.0f),
            ImmediateTap));

    return true;
}

#endif
