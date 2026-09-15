#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Player/CMChimeraBodySegmentActor.h"
#include "Player/CMChimeraIdleTentacleComponent.h"
#include "Player/CMChimeraVisualDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMChimeraSegmentVisualRoleTest,
    "Chimera.BodySegment.VisualRoles",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMChimeraSegmentVisualRoleTest::RunTest(const FString& Parameters)
{
    const auto TestLayout = [this](
        const int32 ActiveSegmentCount,
        const TArray<ECMChimeraSegmentVisualRole>& ExpectedRoles)
    {
        TestEqual(
            *FString::Printf(
                TEXT("%d-segment layout has the expected test data"),
                ActiveSegmentCount),
            ExpectedRoles.Num(),
            ActiveSegmentCount);

        for (int32 SegmentIndex = 0;
            SegmentIndex < ActiveSegmentCount;
            ++SegmentIndex)
        {
            TestEqual(
                *FString::Printf(
                    TEXT("%d-segment layout role at index %d"),
                    ActiveSegmentCount,
                    SegmentIndex),
                CMChimeraVisual::ResolveSegmentVisualRole(
                    SegmentIndex,
                    ActiveSegmentCount),
                ExpectedRoles[SegmentIndex]);
        }
    };

    TestLayout(2, {
        ECMChimeraSegmentVisualRole::Head,
        ECMChimeraSegmentVisualRole::Tail
    });
    TestLayout(4, {
        ECMChimeraSegmentVisualRole::Head,
        ECMChimeraSegmentVisualRole::Body,
        ECMChimeraSegmentVisualRole::Body,
        ECMChimeraSegmentVisualRole::Tail
    });

    TArray<ECMChimeraSegmentVisualRole> MaximumLayout;
    MaximumLayout.Init(ECMChimeraSegmentVisualRole::Body, 16);
    MaximumLayout[0] = ECMChimeraSegmentVisualRole::Head;
    MaximumLayout.Last() = ECMChimeraSegmentVisualRole::Tail;
    TestLayout(16, MaximumLayout);

    TestEqual(
        TEXT("Degenerate one-segment layout keeps a visible Head"),
        CMChimeraVisual::ResolveSegmentVisualRole(0, 1),
        ECMChimeraSegmentVisualRole::Head);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMChimeraVisualDefinitionContractTest,
    "Chimera.BodySegment.VisualDefinitionContract",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMChimeraVisualDefinitionContractTest::RunTest(
    const FString& Parameters)
{
    const UCMChimeraVisualDefinition* Definition =
        NewObject<UCMChimeraVisualDefinition>();

    TestEqual(TEXT("Five death-rig bones are required"),
        Definition->RequiredBones.Num(), 5);
    TestTrue(TEXT("root bone is required"),
        Definition->RequiredBones.Contains(FName(TEXT("root"))));
    TestTrue(TEXT("core_root bone is required"),
        Definition->RequiredBones.Contains(FName(TEXT("core_root"))));
    TestEqual(TEXT("Two sampling regions are required"),
        Definition->RequiredSamplingRegions.Num(), 2);
    TestTrue(TEXT("Idle top region is required"),
        Definition->RequiredSamplingRegions.Contains(
            FName(TEXT("IdleTentacleTop"))));
    TestTrue(TEXT("Underbody region is required"),
        Definition->RequiredSamplingRegions.Contains(
            FName(TEXT("UnderbodyArms"))));
    TestEqual(TEXT("Four physics bodies are required"),
        Definition->RequiredPhysicsBodies.Num(), 4);

    TestTrue(TEXT("Head role selects Head preset"),
        &Definition->GetPreset(ECMChimeraSegmentVisualRole::Head)
            == &Definition->Head);
    TestTrue(TEXT("Body role selects Body preset"),
        &Definition->GetPreset(ECMChimeraSegmentVisualRole::Body)
            == &Definition->Body);
    TestTrue(TEXT("Tail role selects Tail preset"),
        &Definition->GetPreset(ECMChimeraSegmentVisualRole::Tail)
            == &Definition->Tail);

#if WITH_EDITOR
    FDataValidationContext ValidationContext;
    TestEqual(TEXT("An unconfigured definition is invalid"),
        Definition->IsDataValid(ValidationContext),
        EDataValidationResult::Invalid);
    TestEqual(TEXT("Each missing role mesh has a precise error"),
        ValidationContext.GetNumErrors(),
        static_cast<uint32>(3));
#endif
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMChimeraIdleTentacleMathTest,
    "Chimera.BodySegment.IdleTentacleMath",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMChimeraIdleTentacleMathTest::RunTest(
    const FString& Parameters)
{
    TestEqual(TEXT("Tentacle starts collapsed"),
        CMChimeraIdleTentacle::ResolveLifecycleLengthAlpha(
            0.0f, 1.0f, 2.0f, 1.0f),
        0.0f);
    TestEqual(TEXT("Tentacle is half grown midway"),
        CMChimeraIdleTentacle::ResolveLifecycleLengthAlpha(
            0.5f, 1.0f, 2.0f, 1.0f),
        0.5f);
    TestEqual(TEXT("Tentacle reaches its target before idling"),
        CMChimeraIdleTentacle::ResolveLifecycleLengthAlpha(
            1.0f, 1.0f, 2.0f, 1.0f),
        1.0f);
    TestEqual(TEXT("Tentacle remains at target length while idling"),
        CMChimeraIdleTentacle::ResolveLifecycleLengthAlpha(
            2.0f, 1.0f, 2.0f, 1.0f),
        1.0f);
    TestEqual(TEXT("Tentacle is half retracted midway"),
        CMChimeraIdleTentacle::ResolveLifecycleLengthAlpha(
            3.5f, 1.0f, 2.0f, 1.0f),
        0.5f);
    TestEqual(TEXT("Tentacle finishes collapsed"),
        CMChimeraIdleTentacle::ResolveLifecycleLengthAlpha(
            4.0f, 1.0f, 2.0f, 1.0f),
        0.0f);

    TestEqual(TEXT("Tentacle remains extended during idle"),
        CMChimeraIdleTentacle::ResolveRetractionAlpha(
            1.0f, 2.0f, 1.0f),
        1.0f);
    TestEqual(TEXT("Tentacle is half retracted midway"),
        CMChimeraIdleTentacle::ResolveRetractionAlpha(
            2.5f, 2.0f, 1.0f),
        0.5f);
    TestEqual(TEXT("Tentacle fully retracts at lifecycle end"),
        CMChimeraIdleTentacle::ResolveRetractionAlpha(
            3.0f, 2.0f, 1.0f),
        0.0f);

    const FVector Anchor(10.0f, 20.0f, 30.0f);
    const FVector RetractedPoint =
        CMChimeraIdleTentacle::EvaluateSplinePoint(
            Anchor,
            FVector::UpVector,
            FVector::ForwardVector,
            FVector::RightVector,
            0.5f,
            100.0f,
            20.0f,
            0.0f,
            1.0f,
            0.0f);
    TestTrue(TEXT("Every point collapses to its anchor after retraction"),
        RetractedPoint.Equals(Anchor));

    const FVector WavePointA =
        CMChimeraIdleTentacle::EvaluateSplinePoint(
            Anchor,
            FVector::UpVector,
            FVector::ForwardVector,
            FVector::RightVector,
            0.5f,
            100.0f,
            20.0f,
            0.0f,
            1.0f,
            1.0f);
    const FVector WavePointB =
        CMChimeraIdleTentacle::EvaluateSplinePoint(
            Anchor,
            FVector::UpVector,
            FVector::ForwardVector,
            FVector::RightVector,
            0.5f,
            100.0f,
            20.0f,
            PI * 0.5f,
            1.0f,
            1.0f);
    TestFalse(TEXT("Wave phase animates spline points over time"),
        WavePointA.Equals(WavePointB));

    const TArray<FVector> ExistingAnchors = {
        FVector::ZeroVector,
        FVector(100.0f, 0.0f, 0.0f)
    };
    TestFalse(TEXT("Candidate inside minimum spacing is rejected"),
        CMChimeraIdleTentacle::IsSpacedFromActiveAnchors(
            FVector(20.0f, 0.0f, 0.0f),
            ExistingAnchors,
            50.0f));
    TestTrue(TEXT("Candidate outside minimum spacing is accepted"),
        CMChimeraIdleTentacle::IsSpacedFromActiveAnchors(
            FVector(50.0f, 60.0f, 0.0f),
            ExistingAnchors,
            50.0f));

    const ACMChimeraBodySegmentActor* DefaultSegment =
        GetDefault<ACMChimeraBodySegmentActor>();
    TestNotNull(TEXT("Every segment owns an IdleTentacles component"),
        DefaultSegment->FindComponentByClass<
            UCMChimeraIdleTentacleComponent>());
    return true;
}

#endif
