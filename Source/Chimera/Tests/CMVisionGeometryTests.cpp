#include "Misc/AutomationTest.h"

#include "Parts/Head/CMVisionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMVisionGeometryTest,
    "Chimera.Vision.ConeGeometry",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMVisionGeometryTest::RunTest(const FString& Parameters)
{
    const FVector Origin = FVector::ZeroVector;
    const FVector Direction = FVector::ForwardVector;
    constexpr float AngleDegrees = 90.0f;
    constexpr float Distance = 1000.0f;

    TestTrue(
        TEXT("A point in front is visible"),
        UCMVisionComponent::IsPointInsideVisionCone(
            Origin,
            Direction,
            AngleDegrees,
            Distance,
            FVector(500.0f, 0.0f, 300.0f)
        )
    );
    TestTrue(
        TEXT("A point on the horizontal angle boundary is visible"),
        UCMVisionComponent::IsPointInsideVisionCone(
            Origin,
            Direction,
            AngleDegrees,
            Distance,
            FVector(500.0f, 500.0f, 0.0f)
        )
    );
    TestFalse(
        TEXT("A point outside the angle is hidden"),
        UCMVisionComponent::IsPointInsideVisionCone(
            Origin,
            Direction,
            AngleDegrees,
            Distance,
            FVector(0.0f, 500.0f, 0.0f)
        )
    );
    TestFalse(
        TEXT("A point outside the distance is hidden"),
        UCMVisionComponent::IsPointInsideVisionCone(
            Origin,
            Direction,
            AngleDegrees,
            Distance,
            FVector(1001.0f, 0.0f, 0.0f)
        )
    );

    return true;
}

#endif
