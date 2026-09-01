#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Stage/Obstacle/Data/CMObstacleBalanceTableRow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMObstacleBalanceResolveTest,
    "Chimera.Obstacle.Balance.ResolveModes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMObstacleBalanceResolveTest::RunTest(const FString& Parameters)
{
    FCMObstacleDamageBalanceTableRow DamageRow;
    DamageRow.DefaultDamage = 10.0f;
    DamageRow.HighDamage = 25.0f;
    FCMObstacleStatusBalanceTableRow StatusRow;
    StatusRow.DefaultDuration = 4.0f;
    StatusRow.StatusEffect = ECMObstacleStatusEffect::BodyVisionReduced;
    StatusRow.PrimaryStatusValue = 0.6f;
    StatusRow.SecondaryStatusValue = 0.7f;

    FCMObstacleBalanceSelection Selection;
    FCMResolvedObstacleBalance Result = CMObstacleBalance::Resolve(
        &DamageRow, &StatusRow, Selection, 1.5f);
    TestEqual(TEXT("Default damage uses stage multiplier"), Result.Damage, 15.0f);
    TestEqual(TEXT("Default duration comes from sheet"), Result.Duration, 4.0f);

    Selection.DamageMode = ECMObstacleDamageMode::High;
    Result = CMObstacleBalance::Resolve(&DamageRow, &StatusRow, Selection, 1.5f);
    TestEqual(TEXT("High damage uses stage multiplier"), Result.Damage, 37.5f);

    Selection.DamageMode = ECMObstacleDamageMode::Custom;
    Selection.CustomDamage = 7.0f;
    Selection.DurationMode = ECMObstacleDurationMode::Custom;
    Selection.CustomDuration = 2.0f;
    Result = CMObstacleBalance::Resolve(&DamageRow, &StatusRow, Selection, 1.5f);
    TestEqual(TEXT("Custom damage is final and ignores multiplier"), Result.Damage, 7.0f);
    TestEqual(TEXT("Custom duration is final"), Result.Duration, 2.0f);
    TestEqual(TEXT("Primary status value is preserved"), Result.PrimaryStatusValue, 0.6f);
    TestEqual(TEXT("Secondary status value is preserved"), Result.SecondaryStatusValue, 0.7f);

    Selection.DamageMode = ECMObstacleDamageMode::Default;
    Selection.DurationMode = ECMObstacleDurationMode::Default;
    Result = CMObstacleBalance::Resolve(&DamageRow, nullptr, Selection, 1.5f);
    TestTrue(TEXT("Damage-only selection is valid"), Result.bValid);
    TestEqual(TEXT("Damage-only selection has no status"), Result.StatusEffect,
        ECMObstacleStatusEffect::None);
    Result = CMObstacleBalance::Resolve(nullptr, &StatusRow, Selection, 1.5f);
    TestTrue(TEXT("Status-only selection is valid"), Result.bValid);
    TestEqual(TEXT("Status-only selection has zero damage"), Result.Damage, 0.0f);

    Selection.DamageMode = ECMObstacleDamageMode::Custom;
    Selection.CustomDamage = 12.0f;
    Result = CMObstacleBalance::Resolve(nullptr, nullptr, Selection, 3.0f);
    TestTrue(TEXT("Custom-only damage selection is valid"), Result.bValid);
    TestEqual(TEXT("Custom-only damage ignores stage multiplier"),
        Result.Damage, 12.0f);
    return true;
}

#endif
