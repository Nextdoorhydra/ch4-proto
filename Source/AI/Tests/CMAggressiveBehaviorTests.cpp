#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMAggressiveWanderDebugCommandTest, "Chimera.AI.Aggressive.Behavior.WanderDebugCommand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMAggressiveWanderDebugCommandTest::RunTest(const FString& Parameters)
{
#if !UE_BUILD_SHIPPING
    TestNotNull(TEXT("Hostile wander debug console command is registered"), IConsoleManager::Get().FindConsoleObject(TEXT("CM.AI.WanderDebug")));
#endif
    return true;
}

#endif
