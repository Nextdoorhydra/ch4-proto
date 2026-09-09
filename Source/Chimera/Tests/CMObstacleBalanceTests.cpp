#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Stage/Obstacle/CMStageObstacleBase.h"
#include "Stage/Obstacle/Data/CMObstacleBalanceTableRow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMObstacleBalanceResolveTest,
    "Chimera.Obstacle.Balance.ResolveModes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMObstacleBalanceResolveTest::RunTest(const FString& Parameters)
{
    FCMObstacleDamageBalanceTableRow DamageRow;
    DamageRow.DefaultDamage = 10.0f;
    DamageRow.HighDamage = 25.0f;
    FCMObstacleBalanceSelection Selection;
    FCMResolvedObstacleBalance Result = CMObstacleBalance::Resolve(
        &DamageRow, Selection, 1.5f);
    TestEqual(TEXT("Default damage uses stage multiplier"), Result.Damage, 15.0f);

    Selection.DamageMode = ECMObstacleDamageMode::High;
    Result = CMObstacleBalance::Resolve(&DamageRow, Selection, 1.5f);
    TestEqual(TEXT("High damage uses stage multiplier"), Result.Damage, 37.5f);

    Selection.DamageMode = ECMObstacleDamageMode::Custom;
    Selection.CustomDamage = 7.0f;
    Result = CMObstacleBalance::Resolve(&DamageRow, Selection, 1.5f);
    TestEqual(TEXT("Custom damage is final and ignores multiplier"), Result.Damage, 7.0f);

    Selection.DamageMode = ECMObstacleDamageMode::Default;
    Result = CMObstacleBalance::Resolve(&DamageRow, Selection, 1.5f);
    TestTrue(TEXT("Damage-only selection is valid"), Result.bValid);

    Selection.DamageMode = ECMObstacleDamageMode::Custom;
    Selection.CustomDamage = 12.0f;
    Result = CMObstacleBalance::Resolve(nullptr, Selection, 3.0f);
    TestTrue(TEXT("Custom-only damage selection is valid"), Result.bValid);
    TestEqual(TEXT("Custom-only damage ignores stage multiplier"),
        Result.Damage, 12.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMObstacleKillOnEnterTest,
    "Chimera.Obstacle.Balance.KillOnEnterWithoutDamageBalance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMObstacleKillOnEnterTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Init = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
        nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("World"), World))
    {
        return false;
    }

    ACMStageObstacleBase* Obstacle = World->SpawnActor<ACMStageObstacleBase>();
    if (!TestNotNull(TEXT("Obstacle"), Obstacle))
    {
        World->DestroyWorld(false);
        return false;
    }

    Obstacle->PartEffect.ApplicationPolicy =
        ECMObstacleEffectApplicationPolicy::KillOnEnter;
    Obstacle->PartEffect.StatusEffect = ECMPartObstacleStatusEffect::None;
    Obstacle->ResolveBalance();
    Obstacle->ApplyResolvedBalance();
    TestTrue(TEXT("Kill on enter enables hazard without damage balance"),
        Obstacle->PartEffect.bEnabled);

    Obstacle->PartEffect.ApplicationPolicy =
        ECMObstacleEffectApplicationPolicy::OnceOnEnter;
    Obstacle->ApplyResolvedBalance();
    TestFalse(TEXT("Ordinary empty effect remains disabled"),
        Obstacle->PartEffect.bEnabled);

    World->DestroyWorld(false);
    return true;
}

#endif
