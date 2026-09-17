#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Aggressive/Common/Behavior/CMAggressiveBehaviorComponent.h"
#include "Engine/CollisionProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMAggressiveWanderDebugCommandTest, "Chimera.AI.Aggressive.Behavior.WanderDebugCommand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMAggressiveWanderDebugCommandTest::RunTest(const FString& Parameters)
{
#if !UE_BUILD_SHIPPING
    TestNotNull(TEXT("Hostile wander debug console command is registered"), IConsoleManager::Get().FindConsoleObject(TEXT("CM.AI.WanderDebug")));
#endif
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMAggressiveStuckProgressRulesTest, "Chimera.AI.Aggressive.Behavior.StuckProgressRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMAggressiveStuckProgressRulesTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Small progress toward the goal remains stuck"), CMAggressiveBehaviorRules::HasMadeGoalProgress(1000.0f, 950.0f, 75.0f));
    TestTrue(TEXT("Required progress toward the goal resets stuck tracking"), CMAggressiveBehaviorRules::HasMadeGoalProgress(1000.0f, 925.0f, 75.0f));
    TestFalse(TEXT("Movement away from the goal never counts as progress"), CMAggressiveBehaviorRules::HasMadeGoalProgress(1000.0f, 1100.0f, 75.0f));
    TestFalse(TEXT("Tetra jitter below the movement threshold remains stuck"), CMAggressiveBehaviorRules::HasMovedMinimumDistance(FVector::ZeroVector, FVector(14.9f, 0.0f, 20.0f), 15.0f));
    TestTrue(TEXT("Tetra planar movement at the threshold resets stuck tracking"), CMAggressiveBehaviorRules::HasMovedMinimumDistance(FVector::ZeroVector, FVector(15.0f, 0.0f, 20.0f), 15.0f));
    TestFalse(TEXT("Ripper attack stays on cooldown before its ready time"), CMAggressiveBehaviorRules::IsRipperAttackReady(9.9, 10.0));
    TestTrue(TEXT("Ripper attack becomes ready at its ready time"), CMAggressiveBehaviorRules::IsRipperAttackReady(10.0, 10.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMRipperCollisionProfileTest, "Chimera.AI.Aggressive.Ripper.CollisionProfile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMRipperCollisionProfileTest::RunTest(const FString& Parameters)
{
    FCollisionResponseTemplate Profile;
    TestTrue(TEXT("Ripper collision profile exists"), UCollisionProfile::Get()->GetProfileTemplate(TEXT("CMRipperBody"), Profile));
    TestEqual(TEXT("Ripper body uses the dedicated object channel"), Profile.ObjectType.GetValue(), ECC_GameTraceChannel3);
    TestEqual(TEXT("Ripper bodies ignore peer Ripper bodies"), Profile.ResponseToChannels.GetResponse(ECC_GameTraceChannel3), ECR_Ignore);
    TestEqual(TEXT("Ripper bodies still block the world"), Profile.ResponseToChannels.GetResponse(ECC_WorldStatic), ECR_Block);
    TestEqual(TEXT("Ripper bodies still block other physics actors"), Profile.ResponseToChannels.GetResponse(ECC_PhysicsBody), ECR_Block);
    TestEqual(TEXT("Ripper bodies still block pawns"), Profile.ResponseToChannels.GetResponse(ECC_Pawn), ECR_Block);
    return true;
}

#endif
