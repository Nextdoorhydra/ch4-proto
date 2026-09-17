#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/CMPartAnimInstance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMPartAnimationMathTest,
    "Chimera.Animation.Part.ProceduralMath",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMPartAnimationMathTest::RunTest(const FString& Parameters)
{
    TestEqual(
        TEXT("Loose motion starts at rest"),
        CMPartAnimation::CalculateLooseMotionAlpha(0.0f),
        0.0f);
    TestTrue(
        TEXT("Loose motion reaches its apex halfway through"),
        FMath::IsNearlyEqual(
            CMPartAnimation::CalculateLooseMotionAlpha(0.5f),
            1.0f));
    TestTrue(
        TEXT("Loose motion returns to rest"),
        FMath::IsNearlyZero(
            CMPartAnimation::CalculateLooseMotionAlpha(1.0f)));
    TestEqual(
        TEXT("Inactive physics contributes no blend"),
        CMPartAnimation::CalculatePhysicsBlendWeight(false, 0.8f),
        0.0f);
    TestEqual(
        TEXT("Physics blend clamps invalid tuning"),
        CMPartAnimation::CalculatePhysicsBlendWeight(true, 2.0f),
        1.0f);
    TestEqual(
        TEXT("A delayed joint stays at rest before its delay"),
        CMPartAnimation::CalculateDelayedMotionPhase(0.1f, 0.2f),
        0.0f);
    TestTrue(
        TEXT("A delayed joint catches up to the active step"),
        CMPartAnimation::CalculateDelayedMotionPhase(0.6f, 0.2f) > 0.0f);
    TestEqual(
        TEXT("An inactive joint has no procedural motion"),
        CMPartAnimation::CalculateJointMotionAlpha(0.0f),
        0.0f);
    TestTrue(
        TEXT("A joint produces motion only during its own step"),
        !FMath::IsNearlyZero(
            CMPartAnimation::CalculateJointMotionAlpha(0.25f)));

    const FVector SwingStart(0.0f, 0.0f, 0.0f);
    const FVector SwingStrike(100.0f, 0.0f, 0.0f);
    const FVector SwingRelaxed(0.0f, 0.0f, -80.0f);
    TestTrue(
        TEXT("Arm swing begins at the captured hand location"),
        CMPartAnimation::CalculateArmSwingTarget(
            SwingStart,
            SwingStrike,
            SwingRelaxed,
            FVector::UpVector,
            0.0f,
            30.0f).Equals(SwingStart));
    TestTrue(
        TEXT("Arm swing reaches its strike point"),
        CMPartAnimation::CalculateArmSwingTarget(
            SwingStart,
            SwingStrike,
            SwingRelaxed,
            FVector::UpVector,
            0.62f,
            30.0f).Equals(SwingStrike, 0.01f));
    TestTrue(
        TEXT("Arm swing returns to its relaxed target"),
        CMPartAnimation::CalculateArmSwingTarget(
            SwingStart,
            SwingStrike,
            SwingRelaxed,
            FVector::UpVector,
            1.0f,
            30.0f).Equals(SwingRelaxed, 0.01f));

    const FVector SwingBack(-80.0f, 0.0f, -20.0f);
    TestTrue(
        TEXT("Directional Arm swing reaches its backward windup"),
        CMPartAnimation::CalculateArmForwardBackSwingTarget(
            SwingStart,
            SwingBack,
            SwingStrike,
            SwingRelaxed,
            FVector::UpVector,
            0.22f,
            30.0f).Equals(SwingBack, 0.01f));
    TestTrue(
        TEXT("Directional Arm swing crosses forward"),
        CMPartAnimation::CalculateArmForwardBackSwingTarget(
            SwingStart,
            SwingBack,
            SwingStrike,
            SwingRelaxed,
            FVector::UpVector,
            0.68f,
            30.0f).Equals(SwingStrike, 0.01f));
    TestTrue(
        TEXT("Directional Arm swing settles back to loose rest"),
        CMPartAnimation::CalculateArmForwardBackSwingTarget(
            SwingStart,
            SwingBack,
            SwingStrike,
            SwingRelaxed,
            FVector::UpVector,
            1.0f,
            30.0f).Equals(SwingRelaxed, 0.01f));

    const FVector FanShoulder(10.0f, 20.0f, 30.0f);
    const FVector FanBack = CMPartAnimation::CalculateArmPlanarFanTarget(
        FanShoulder,
        FVector::RightVector,
        FVector::ForwardVector,
        0.0f,
        100.0f,
        25.0f,
        70.0f);
    const FVector FanMiddle = CMPartAnimation::CalculateArmPlanarFanTarget(
        FanShoulder,
        FVector::RightVector,
        FVector::ForwardVector,
        0.5f,
        100.0f,
        25.0f,
        70.0f);
    const FVector FanFront = CMPartAnimation::CalculateArmPlanarFanTarget(
        FanShoulder,
        FVector::RightVector,
        FVector::ForwardVector,
        1.0f,
        100.0f,
        25.0f,
        70.0f);
    TestTrue(TEXT("Top-view Arm fan begins behind the shoulder"),
        FVector::DotProduct(
            FanBack - FanShoulder,
            FVector::ForwardVector) < 0.0f);
    TestTrue(TEXT("Top-view Arm fan crosses the outward radial"),
        (FanMiddle - FVector(10.0f, 120.0f, 55.0f)).IsNearlyZero(0.01f));
    TestTrue(TEXT("Top-view Arm fan ends ahead of the shoulder"),
        FVector::DotProduct(
            FanFront - FanShoulder,
            FVector::ForwardVector) > 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMPartAnimationSlopeFootTest,
    "Chimera.Animation.Part.SlopeFootMath",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMPartAnimationSlopeFootTest::RunTest(const FString& Parameters)
{
    const FVector GroundNormal = FVector(0.0f, 0.5f, 0.8660254f)
        .GetSafeNormal();
    FVector Forward;
    FVector Right;
    FVector Up;
    TestTrue(
        TEXT("Walkable slope produces a valid tangent frame"),
        CMPartAnimation::BuildSlopeFrame(
            GroundNormal,
            FVector::ForwardVector,
            Forward,
            Right,
            Up));
    TestTrue(TEXT("Slope frame forward is normalized"),
        FMath::IsNearlyEqual(Forward.SizeSquared(), 1.0f, 0.001f));
    TestTrue(TEXT("Slope frame right is normalized"),
        FMath::IsNearlyEqual(Right.SizeSquared(), 1.0f, 0.001f));
    TestTrue(TEXT("Slope frame up is normalized"),
        FMath::IsNearlyEqual(Up.SizeSquared(), 1.0f, 0.001f));
    TestTrue(TEXT("Slope frame axes are orthogonal"),
        FMath::Abs(FVector::DotProduct(Forward, Right)) < 0.001f
            && FMath::Abs(FVector::DotProduct(Forward, Up)) < 0.001f
            && FMath::Abs(FVector::DotProduct(Right, Up)) < 0.001f);
    TestTrue(TEXT("Slope frame up follows the measured ground normal"),
        FVector::DotProduct(Up, GroundNormal) > 0.999f);

    FVector InvalidForward;
    FVector InvalidRight;
    FVector InvalidUp;
    TestFalse(
        TEXT("Downward ground normal falls back instead of solving"),
        CMPartAnimation::BuildSlopeFrame(
            FVector(0.0f, 0.0f, -1.0f),
            FVector::ForwardVector,
            InvalidForward,
            InvalidRight,
            InvalidUp));
    TestEqual(TEXT("Invalid slope falls back to world up"),
        InvalidUp,
        FVector::UpVector);

    const FVector StartLocation = FVector::ZeroVector;
    const FVector TargetLocation = FVector(100.0f, 0.0f, 0.0f);
    const FVector StartTarget = CMPartAnimation::CalculateSlopeFootTarget(
        StartLocation,
        TargetLocation,
        FVector::UpVector,
        GroundNormal,
        0.0f,
        25.0f,
        2.0f);
    const FVector EndTarget = CMPartAnimation::CalculateSlopeFootTarget(
        StartLocation,
        TargetLocation,
        FVector::UpVector,
        GroundNormal,
        1.0f,
        25.0f,
        2.0f);
    const FVector MidTarget = CMPartAnimation::CalculateSlopeFootTarget(
        StartLocation,
        TargetLocation,
        FVector::UpVector,
        GroundNormal,
        0.5f,
        25.0f,
        2.0f);
    TestTrue(TEXT("Step target starts at the start contact plus sole offset"),
        StartTarget.Equals(FVector(0.0f, 0.0f, 2.0f), 0.01f));
    TestTrue(TEXT("Step target ends at the ground contact plus sole offset"),
        EndTarget.Equals(TargetLocation + GroundNormal * 2.0f, 0.01f));
    TestTrue(TEXT("Swing target gains clearance at mid phase"),
        MidTarget.Z > FMath::Max(StartTarget.Z, EndTarget.Z));

    const FVector MidNormal = CMPartAnimation::CalculateSlopeFootNormal(
        FVector::UpVector,
        GroundNormal,
        0.5f);
    TestTrue(TEXT("Swing normal interpolates toward the target slope"),
        FVector::DotProduct(MidNormal, GroundNormal) > 0.95f
            && FVector::DotProduct(MidNormal, FVector::UpVector) > 0.95f);

    const FVector ClampedTarget = CMPartAnimation::ClampFootTargetToReach(
        FVector::ZeroVector,
        FVector(100.0f, 0.0f, 0.0f),
        100.0f,
        0.95f);
    TestTrue(TEXT("Reach clamp keeps the effector inside the measured chain"),
        FMath::IsNearlyEqual(ClampedTarget.Size(), 95.0f, 0.01f));
    const FVector UnclampedTarget = CMPartAnimation::ClampFootTargetToReach(
        FVector::ZeroVector,
        FVector(40.0f, 0.0f, 0.0f),
        100.0f,
        0.95f);
    TestTrue(TEXT("Reach clamp preserves an in-range effector"),
        UnclampedTarget.Equals(FVector(40.0f, 0.0f, 0.0f), 0.01f));

    const FQuat SlopeRotation = CMPartAnimation::MakeSlopeFootRotation(
        GroundNormal,
        FVector::ForwardVector);
    TestTrue(TEXT("Slope foot rotation aligns its up axis to the normal"),
        FVector::DotProduct(
            SlopeRotation.RotateVector(FVector::UpVector),
            GroundNormal) > 0.999f);

    const FQuat ReferenceRotation = FQuat::Identity;
    const FQuat ExcessiveRotation = FQuat(
        FVector::RightVector,
        FMath::DegreesToRadians(120.0f));
    const FQuat LimitedRotation =
        CMPartAnimation::ClampRotationToReference(
            ExcessiveRotation,
            ReferenceRotation,
            40.0f);
    TestTrue(TEXT("Foot rotation remains inside its configured range"),
        FMath::RadiansToDegrees(ReferenceRotation.AngularDistance(
            LimitedRotation)) <= 40.01f);
    return true;
}

#endif
