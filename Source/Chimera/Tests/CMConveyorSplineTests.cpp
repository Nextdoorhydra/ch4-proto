#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "Misc/AutomationTest.h"
#include "Stage/Obstacle/CMConveyorSegmentActor.h"
#include "Stage/Obstacle/CMConveyorSplineActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMConveyorSplineDistanceTest,
    "Chimera.Obstacle.ConveyorSpline.Distance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMConveyorSplineDistanceTest::RunTest(const FString& Parameters)
{
    bool bReachedEnd = false;
    float Distance = ACMConveyorSplineActor::CalculateNextDistance(25.0f, 100.0f, 10.0f, false, bReachedEnd);
    TestEqual(TEXT("Open conveyor advances inside its bounds"), Distance, 35.0f);
    TestFalse(TEXT("Interior advance does not reach an end"), bReachedEnd);

    Distance = ACMConveyorSplineActor::CalculateNextDistance(95.0f, 100.0f, 10.0f, false, bReachedEnd);
    TestEqual(TEXT("Forward open conveyor clamps at its end"), Distance, 100.0f);
    TestTrue(TEXT("Forward open conveyor reports its end"), bReachedEnd);

    Distance = ACMConveyorSplineActor::CalculateNextDistance(5.0f, 100.0f, -10.0f, false, bReachedEnd);
    TestEqual(TEXT("Reverse open conveyor clamps at its start"), Distance, 0.0f);
    TestTrue(TEXT("Reverse open conveyor reports its start"), bReachedEnd);

    Distance = ACMConveyorSplineActor::CalculateNextDistance(95.0f, 100.0f, 10.0f, true, bReachedEnd);
    TestEqual(TEXT("Forward loop wraps through its seam"), Distance, 5.0f);
    TestFalse(TEXT("Forward loop never reports an end"), bReachedEnd);

    Distance = ACMConveyorSplineActor::CalculateNextDistance(5.0f, 100.0f, -10.0f, true, bReachedEnd);
    TestEqual(TEXT("Reverse loop wraps through its seam"), Distance, 95.0f);
    TestFalse(TEXT("Reverse loop never reports an end"), bReachedEnd);

    const ACMConveyorSplineActor* DefaultConveyor = GetDefault<ACMConveyorSplineActor>();
    TestFalse(TEXT("Conveyor actor tick remains disabled"), DefaultConveyor->PrimaryActorTick.bCanEverTick);
    TestTrue(TEXT("Conveyor preserves authored part placement by default"), DefaultConveyor->bPreserveInitialPartPlacement);
    const USplineComponent* DefaultSpline = DefaultConveyor->FindComponentByClass<USplineComponent>();
    TestNotNull(TEXT("Conveyor owns a spline component"), DefaultSpline);
    if (DefaultSpline)
    {
        TestEqual(TEXT("Default spline has two editable points"), DefaultSpline->GetNumberOfSplinePoints(), 2);
        TestEqual(TEXT("Default spline length is one meter"), DefaultSpline->GetSplineLength(), 100.0f);
        TestEqual(TEXT("Spline is attached to the actor root"), DefaultSpline->GetAttachParent(), DefaultConveyor->GetRootComponent());
        TestFalse(TEXT("Spline location follows its actor"), DefaultSpline->IsUsingAbsoluteLocation());
        TestEqual(TEXT("Spline mobility matches its movable actor root"), DefaultSpline->Mobility, EComponentMobility::Movable);
        TestTrue(TEXT("Default spline debug drawing is enabled"), DefaultSpline->bDrawDebug);
    }

    const ACMConveyorSegmentActor* DefaultSegment = GetDefault<ACMConveyorSegmentActor>();
    TestFalse(TEXT("Conveyor segment tick remains disabled"), DefaultSegment->PrimaryActorTick.bCanEverTick);
    const USplineComponent* DefaultSegmentPath = DefaultSegment->GetConveyorPath();
    TestNotNull(TEXT("Conveyor segment owns a local path"), DefaultSegmentPath);
    if (DefaultSegmentPath)
    {
        TestEqual(TEXT("Default straight segment path matches its mesh length"), DefaultSegmentPath->GetSplineLength(), 240.0f);
        TestFalse(TEXT("Segment path is open"), DefaultSegmentPath->IsClosedLoop());
    }
    return true;
}

#endif
