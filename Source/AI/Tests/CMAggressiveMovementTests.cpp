#include "Aggressive/Common/Core/CMAggressiveAITypes.h"
#include "Aggressive/Common/Movement/CMAIFixedLegActuatorComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMAggressiveMovementRulesTest, "Chimera.AI.Aggressive.Movement.Rules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMAggressiveMovementRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Forward vectors resolve to forward"), CMAggressiveDirection::QuantizeLocalDirection8(FVector::ForwardVector), ECMAggressiveMoveDirection::Forward);
    TestEqual(TEXT("Right vectors resolve to right"), CMAggressiveDirection::QuantizeLocalDirection8(FVector::RightVector), ECMAggressiveMoveDirection::Right);
    TestEqual(TEXT("Negligible vectors resolve to no direction"), CMAggressiveDirection::QuantizeLocalDirection8(FVector(0.001f, 0.0f, 0.0f), 0.01f), ECMAggressiveMoveDirection::None);

    FCMAggressiveMovementGoal Goal;
    Goal.WorldLocation = FVector(100.0f, 0.0f, 0.0f);
    Goal.AcceptanceRadius = 10.0f;
    TestEqual(TEXT("A distant goal resolves in body space"), CMAggressiveMovementCommand::ResolveGoalDirection(FVector::ZeroVector, FQuat::Identity, Goal), ECMAggressiveMoveDirection::Forward);
    TestEqual(TEXT("A reached goal clears the requested direction"), CMAggressiveMovementCommand::ResolveGoalDirection(FVector(95.0f, 0.0f, 0.0f), FQuat::Identity, Goal), ECMAggressiveMoveDirection::None);

    TArray<int32> ActiveLegIndices;
    TestTrue(TEXT("Valid activation signals are accepted"), CMAggressiveMovementCommand::SelectActiveLegIndices({0.49f, 0.5f, 1.0f}, 3, 0.5f, ActiveLegIndices));
    TestEqual(TEXT("Signals at or above the threshold activate two legs"), ActiveLegIndices, TArray<int32>({1, 2}));
    TestFalse(TEXT("A signal count mismatch is rejected"), CMAggressiveMovementCommand::SelectActiveLegIndices({1.0f}, 2, 0.5f, ActiveLegIndices));
    TestTrue(TEXT("Rejected signals leave no stale leg indices"), ActiveLegIndices.IsEmpty());

    TestTrue(TEXT("A body inside the acceptance radius advances"), CMAggressiveOmnidirectionalPath::ShouldAdvancePathPoint(FVector(95.0f, 0.0f, 0.0f), FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), 10.0f, 20.0f, false));
    TestTrue(TEXT("A nearby body past an intermediate point advances"), CMAggressiveOmnidirectionalPath::ShouldAdvancePathPoint(FVector(105.0f, 0.0f, 0.0f), FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), 1.0f, 20.0f, false));
    TestFalse(TEXT("Passing a final point outside acceptance does not finish the path"), CMAggressiveOmnidirectionalPath::ShouldAdvancePathPoint(FVector(105.0f, 0.0f, 0.0f), FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), 1.0f, 20.0f, true));

    const FVector ContactLocation(100.0f, -50.0f, 128.0f);
    FVector GroundSweepStart;
    FVector GroundSweepEnd;
    CMAIFixedLegActuation::CalculateGroundSweepSegment(ContactLocation, 12.0f, 8.0f, GroundSweepStart, GroundSweepEnd);
    TestTrue(TEXT("Ground sweep sphere starts clear of the contact plane"), GroundSweepStart.Z - 12.0f > ContactLocation.Z);
    TestEqual(TEXT("Ground sweep keeps the contact point X coordinate"), GroundSweepStart.X, ContactLocation.X);
    TestEqual(TEXT("Ground sweep keeps the contact point Y coordinate"), GroundSweepStart.Y, ContactLocation.Y);
    TestEqual(TEXT("Ground sweep travels the configured distance"), GroundSweepStart.Z - GroundSweepEnd.Z, 8.0);

    return true;
}

#endif
