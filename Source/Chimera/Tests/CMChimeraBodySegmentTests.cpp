#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Player/CMChimeraBodySegmentActor.h"
#include "Player/CMChimeraIdleTentacleComponent.h"
#include "Player/CMChimeraVisualDefinition.h"
#include "Player/CMChimeraWrapTentacleComponent.h"

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

    FDataValidationContext ValidationContext;
    TestEqual(TEXT("An unconfigured definition is invalid"),
        Definition->IsDataValid(ValidationContext),
        EDataValidationResult::Invalid);
    TestEqual(TEXT("Each missing role mesh has a precise error"),
        ValidationContext.GetNumErrors(),
        static_cast<uint32>(3));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMChimeraWrapTentacleMathTest,
    "Chimera.BodySegment.WrapTentacleMath",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMChimeraWrapTentacleMathTest::RunTest(
    const FString& Parameters)
{
    TestEqual(TEXT("Extension advances by its configured duration"),
        CMChimeraWrapTentacle::AdvanceAlpha(
            0.0f, true, 0.5f, 1.0f, 0.5f),
        0.5f);
    TestEqual(TEXT("Retraction uses its independent duration"),
        CMChimeraWrapTentacle::AdvanceAlpha(
            1.0f, false, 0.25f, 1.0f, 0.5f),
        0.5f);

    TestEqual(TEXT("First target point begins collapsed"),
        CMChimeraWrapTentacle::ResolvePointRevealAlpha(0.0f, 1, 6),
        0.0f);
    TestEqual(TEXT("First target point finishes before the second"),
        CMChimeraWrapTentacle::ResolvePointRevealAlpha(0.2f, 1, 6),
        1.0f);
    TestEqual(TEXT("Second target point has not started at that time"),
        CMChimeraWrapTentacle::ResolvePointRevealAlpha(0.2f, 2, 6),
        0.0f);
    TestEqual(TEXT("Last target point is fully revealed at completion"),
        CMChimeraWrapTentacle::ResolvePointRevealAlpha(1.0f, 5, 6),
        1.0f);

    const FBoxSphereBounds TargetBounds(
        FBox(FVector(100.0f, -50.0f, -50.0f),
            FVector(200.0f, 50.0f, 50.0f)));
    TestTrue(TEXT("Activation measures distance to the mesh bounds"),
        CMChimeraWrapTentacle::IsWithinActivationDistance(
            FVector(75.0f, 0.0f, 0.0f), TargetBounds, 25.0f));
    TestFalse(TEXT("Activation rejects points outside the threshold"),
        CMChimeraWrapTentacle::IsWithinActivationDistance(
            FVector::ZeroVector, TargetBounds, 99.0f));

    const ACMChimeraBodySegmentActor* DefaultSegment =
        GetDefault<ACMChimeraBodySegmentActor>();
    const UCMChimeraWrapTentacleComponent* DefaultWrap =
        DefaultSegment->FindComponentByClass<
            UCMChimeraWrapTentacleComponent>();
    TestNotNull(TEXT("Every segment owns a WrapTentacles component"),
        DefaultWrap);
    if (DefaultWrap)
    {
        TestEqual(TEXT("Default wrap path uses thirty-two spline points"),
            DefaultWrap->SplinePointCount, 32);
        TestEqual(TEXT("Surface-snapped anchors use two-centimeter spacing"),
            DefaultWrap->MinimumTargetAnchorDistance, 2.0f);
        TestEqual(TEXT("Default wrap tentacles use quarter width"),
            DefaultWrap->TentacleWidth, 0.25f);
        TestEqual(TEXT("Surface-snapped paths disable lateral offset"),
            DefaultWrap->Entanglement, 0.0f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMChimeraWrapTentacleRuntimeTest,
    "Chimera.BodySegment.WrapTentacleRuntime",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMChimeraWrapTentacleRuntimeTest::RunTest(
    const FString& Parameters)
{
    UWorld::InitializationValues InitValues;
    InitValues.AllowAudioPlayback(false)
        .CreatePhysicsScene(true)
        .CreateNavigation(false)
        .CreateAISystem(false)
        .ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Game,
        false,
        NAME_None,
        nullptr,
        true,
        ERHIFeatureLevel::Num,
        &InitValues);
    TestNotNull(TEXT("Wrap-tentacle runtime world is created"), World);
    if (!World)
    {
        return false;
    }

    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(
        EWorldType::Game);
    WorldContext.SetCurrentWorld(World);

    AActor* Owner = World->SpawnActor<AActor>();
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(
        nullptr,
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    USceneComponent* Root = Owner
        ? NewObject<USceneComponent>(Owner, TEXT("WrapTestRoot"))
        : nullptr;
    UStaticMeshComponent* Source = Owner
        ? NewObject<UStaticMeshComponent>(Owner, TEXT("WrapTestSource"))
        : nullptr;
    UStaticMeshComponent* Target = Owner
        ? NewObject<UStaticMeshComponent>(Owner, TEXT("WrapTestTarget"))
        : nullptr;
    UCMChimeraWrapTentacleComponent* Wrap = Owner
        ? NewObject<UCMChimeraWrapTentacleComponent>(
            Owner,
            TEXT("WrapTestComponent"))
        : nullptr;
    TestNotNull(TEXT("Engine sphere mesh is available"), Sphere);
    TestNotNull(TEXT("Wrap test owner is created"), Owner);
    TestNotNull(TEXT("Wrap runtime component is created"), Wrap);

    if (Owner && Root && Source && Target && Wrap && Sphere)
    {
        Owner->SetRootComponent(Root);
        Owner->AddInstanceComponent(Root);
        Root->RegisterComponent();

        Owner->AddInstanceComponent(Source);
        Source->SetupAttachment(Root);
        Source->SetStaticMesh(Sphere);
        Source->RegisterComponent();

        Owner->AddInstanceComponent(Target);
        Target->SetupAttachment(Root);
        Target->SetStaticMesh(Sphere);
        Target->SetRelativeLocation(FVector(150.0f, 0.0f, 0.0f));
        Target->RegisterComponent();

        Owner->AddInstanceComponent(Wrap);
        Wrap->SetupAttachment(Root);
        Wrap->TentacleCount = 2;
        Wrap->SplinePointCount = 4;
        Wrap->MinimumTargetAnchorDistance = 0.0f;
        Wrap->ActivationDistance = 200.0f;
        Wrap->RegisterComponent();
        Wrap->ConfigureSource(Source, 0);
        Wrap->SetTargetMesh(Target);
        Wrap->SetEffectActive(true);
    }

    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    if (Owner && !Owner->HasActorBegunPlay())
    {
        Owner->DispatchBeginPlay();
    }
    if (Wrap)
    {
        Wrap->TickComponent(1.2f, LEVELTICK_All, nullptr);
    }

    if (Wrap)
    {
        TestTrue(TEXT("Static target surface creates ordered paths"),
            Wrap->bPathsReady);
        TestEqual(TEXT("Configured number of tentacles uses the pool"),
            Wrap->RuntimeTentacles.Num(), 2);
        TestEqual(TEXT("Both tentacles become active after extension"),
            Wrap->GetActiveTentacleCount(), 2);
        for (const FCMChimeraWrapTentacleRuntime& Runtime
            : Wrap->RuntimeTentacles)
        {
            TestTrue(TEXT("Each tentacle uses one connected tube mesh"),
                Runtime.TubeMesh
                && Runtime.TubeMesh->IsRegistered()
                && Runtime.TubeMesh->IsVisible()
                && !Runtime.TubeMesh->bHiddenInGame);
            const FProcMeshSection* TubeSection = Runtime.TubeMesh
                ? Runtime.TubeMesh->GetProcMeshSection(0)
                : nullptr;
            TestEqual(TEXT("Four path rings share one tube section"),
                TubeSection ? TubeSection->ProcVertexBuffer.Num() : 0,
                36);
            TestEqual(TEXT("Adjacent rings are joined by shared tube triangles"),
                TubeSection ? TubeSection->ProcIndexBuffer.Num() : 0,
                144);
            for (int32 AnchorIndex = 1;
                AnchorIndex < Runtime.TargetAnchors.Num();
                ++AnchorIndex)
            {
                TestTrue(TEXT("Adjacent surface anchors keep compatible normals"),
                    FVector::DotProduct(
                        Wrap->ResolveAnchorWorldNormal(
                            Runtime.TargetAnchors[AnchorIndex - 1]),
                        Wrap->ResolveAnchorWorldNormal(
                            Runtime.TargetAnchors[AnchorIndex])) >= 0.0f);
            }
        }

        UProceduralMeshComponent* FirstPooledMesh =
            Wrap->RuntimeTentacles.IsValidIndex(0)
                ? Wrap->RuntimeTentacles[0].TubeMesh.Get()
                : nullptr;
        if (Target)
        {
            Target->SetRelativeLocation(FVector(1000.0f, 0.0f, 0.0f));
            Target->UpdateBounds();
            Wrap->TickComponent(0.4f, LEVELTICK_All, nullptr);
            TestTrue(TEXT("Leaving range retracts over time"),
                FMath::IsNearlyEqual(Wrap->GetExtensionAlpha(), 0.5f));
            Wrap->TickComponent(0.4f, LEVELTICK_All, nullptr);
            TestEqual(TEXT("Retraction hides all tentacles"),
                Wrap->GetActiveTentacleCount(), 0);

            Target->SetRelativeLocation(FVector(150.0f, 0.0f, 0.0f));
            Target->UpdateBounds();
            Wrap->TickComponent(1.2f, LEVELTICK_All, nullptr);
            TestEqual(TEXT("Re-entering range extends the same pool"),
                Wrap->GetActiveTentacleCount(), 2);
            TestTrue(TEXT("Re-entry does not replace the connected tube mesh"),
                Wrap->RuntimeTentacles.IsValidIndex(0)
                && Wrap->RuntimeTentacles[0].TubeMesh == FirstPooledMesh);
        }
    }

    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
