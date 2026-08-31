#include "Aggressive/Common/Perception/CMAggressiveSightComponent.h"

#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMAggressiveSightGeometryTest, "Chimera.AI.Aggressive.Sight.Geometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMAggressiveSightGeometryTest::RunTest(const FString& Parameters)
{
#if !UE_BUILD_SHIPPING
    TestNotNull(TEXT("Hostile sight debug console command is registered"), IConsoleManager::Get().FindConsoleObject(TEXT("CM.AI.SightDebug")));
#endif

    const FVector Origin = FVector::ZeroVector;
    const FVector Forward = FVector::ForwardVector;

    TestTrue(TEXT("Point at the configured distance is visible"), UCMAggressiveSightComponent::IsPointInsideSight(Origin, Forward, 60.0f, 180.0f, 300.0f, FVector(300.0f, 0.0f, 0.0f)));
    TestFalse(TEXT("Point beyond the configured distance is hidden"), UCMAggressiveSightComponent::IsPointInsideSight(Origin, Forward, 60.0f, 180.0f, 300.0f, FVector(300.1f, 0.0f, 0.0f)));

    const FVector HorizontalBoundary = FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(30.0f, FVector::UpVector) * 200.0f;
    const FVector OutsideHorizontal = FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(30.1f, FVector::UpVector) * 200.0f;
    TestTrue(TEXT("Horizontal half-angle boundary is visible"), UCMAggressiveSightComponent::IsPointInsideSight(Origin, Forward, 60.0f, 180.0f, 300.0f, HorizontalBoundary));
    TestFalse(TEXT("Point beyond horizontal half-angle is hidden"), UCMAggressiveSightComponent::IsPointInsideSight(Origin, Forward, 60.0f, 180.0f, 300.0f, OutsideHorizontal));

    TestTrue(TEXT("Full vertical sight accepts a point directly above"), UCMAggressiveSightComponent::IsPointInsideSight(Origin, Forward, 60.0f, 180.0f, 300.0f, FVector(0.0f, 0.0f, 200.0f)));
    TestFalse(TEXT("Configured vertical half-angle rejects a high point"), UCMAggressiveSightComponent::IsPointInsideSight(Origin, Forward, 60.0f, 60.0f, 300.0f, FVector(100.0f, 0.0f, 200.0f)));
    TestTrue(TEXT("Reversed forward supports Centipede tail sight"), UCMAggressiveSightComponent::IsPointInsideSight(Origin, -Forward, 40.0f, 180.0f, 200.0f, FVector(-150.0f, 0.0f, 0.0f)));

    return true;
}

#endif
