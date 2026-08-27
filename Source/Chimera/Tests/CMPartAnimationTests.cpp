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
    return true;
}

#endif
