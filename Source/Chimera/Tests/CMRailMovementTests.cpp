#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Stage/Device/Component/CMInteractionHighlightComponent.h"
#include "Stage/Device/Component/CMRailMovementComponent.h"
#include "Parts/Arm/CMArmPart.h"
#include "Player/CMPartSlotComponent.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMRailMovementTest,
    "Chimera.Rail.MovementRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMRailMovementTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
    const UWorld::InitializationValues Init = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
        nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("World"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    USceneComponent* Root = NewObject<USceneComponent>(Owner);
    Owner->SetRootComponent(Root);
    Root->RegisterComponent();
    USplineComponent* Spline = NewObject<USplineComponent>(Owner);
    Spline->SetupAttachment(Root);
    Spline->RegisterComponent();
    Spline->SetSplinePoints({FVector(0, 0, 100), FVector(100, 0, 100)}, ESplineCoordinateSpace::Local);
    UBoxComponent* Body = NewObject<UBoxComponent>(Owner);
    Body->SetupAttachment(Root);
    Body->SetBoxExtent(FVector(5));
    Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Body->RegisterComponent();
    UBoxComponent* Grip = NewObject<UBoxComponent>(Owner);
    Grip->SetupAttachment(Body);
    Grip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Grip->RegisterComponent();
    UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(Owner);
    Visual->SetupAttachment(Body);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->RegisterComponent();
    UCMRailMovementComponent* Movement = NewObject<UCMRailMovementComponent>(Owner);
    Movement->RegisterComponent();
    Movement->ConfigureRail(Spline, Body, Grip, Visual);

    UStaticMeshComponent* HighlightMesh = NewObject<UStaticMeshComponent>(Owner);
    HighlightMesh->SetupAttachment(Root);
    HighlightMesh->RegisterComponent();
    UMaterial* OriginalOverlay = NewObject<UMaterial>();
    UMaterial* HighlightOverlay = NewObject<UMaterial>();
    HighlightMesh->SetOverlayMaterial(OriginalOverlay);
    UCMInteractionHighlightComponent* Highlight = NewObject<UCMInteractionHighlightComponent>(Owner);
    Highlight->HighlightMaterial = HighlightOverlay;
    Highlight->RegisterComponent();
    Highlight->AddHighlightTarget(HighlightMesh);
    Highlight->SetHighlighted(true);
    TestTrue(TEXT("Available interaction applies overlay"), HighlightMesh->GetOverlayMaterial() == HighlightOverlay);
    Highlight->SetHighlighted(false);
    TestTrue(TEXT("Unavailable interaction restores overlay"), HighlightMesh->GetOverlayMaterial() == OriginalOverlay);

    TestTrue(TEXT("Configured open rail"), Movement->IsConfigured());
    TestTrue(TEXT("Forward travel"), Movement->AdvanceDistance(40));
    TestTrue(TEXT("Continuous progress"), FMath::IsNearlyEqual(Movement->GetProgress(), 0.4f));
    TestTrue(TEXT("Body follows rail"), Body->GetComponentLocation().Equals(FVector(40, 0, 100), 0.01f));
    TestTrue(TEXT("Visual starts smoothing from previous pose"),
        Visual->GetComponentLocation().Equals(FVector(0, 0, 100), 0.01f));
    Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Visual interpolates toward collision body"),
        Visual->GetComponentLocation().X > 0.0f
        && Visual->GetComponentLocation().X < Body->GetComponentLocation().X);
    for (int32 Index = 0; Index < 20; ++Index)
    {
        Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
    }
    TestTrue(TEXT("Visual smoothing reaches collision body"),
        Visual->GetComponentLocation().Equals(Body->GetComponentLocation(), 0.1f));
    Movement->bTrackingHold = true;
    Movement->StopMovement();
    TestTrue(TEXT("Release keeps intermediate progress"), FMath::IsNearlyEqual(Movement->GetProgress(), 0.4f));
    TestFalse(TEXT("Release stops tick"), Movement->IsComponentTickEnabled());
    Movement->AdvanceDistance(1000);
    TestEqual(TEXT("End clamp"), Movement->GetProgress(), 1.0f);
    Movement->AdvanceDistance(-1000);
    TestEqual(TEXT("Start clamp"), Movement->GetProgress(), 0.0f);
    TestFalse(TEXT("Invalid delta rejected"), Movement->AdvanceDistance(std::numeric_limits<float>::infinity()));

    AActor* Obstacle = World->SpawnActor<AActor>();
    UBoxComponent* Wall = NewObject<UBoxComponent>(Obstacle);
    Obstacle->SetRootComponent(Wall);
    Wall->SetBoxExtent(FVector(5, 20, 20));
    Wall->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Wall->RegisterComponent();
    Wall->SetWorldLocation(FVector(60, 0, 100));
    Movement->AdvanceDistance(100);
    TestTrue(TEXT("Blocking obstacle prevents crossing"), Movement->GetProgress() > 0.0f && Movement->GetProgress() < 0.51f);
    const float BlockedProgress = Movement->GetProgress();
    Movement->AdvanceDistance(100);
    TestEqual(TEXT("Repeated blocked pull never accumulates"), Movement->GetProgress(), BlockedProgress);
    Movement->AdvanceDistance(-10);
    TestTrue(TEXT("Can reverse away from obstacle"), Movement->GetProgress() < BlockedProgress);
    Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    UBoxComponent* Floor = NewObject<UBoxComponent>(Obstacle);
    Floor->SetupAttachment(Wall);
    Floor->SetBoxExtent(FVector(200, 200, 5));
    Floor->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Floor->RegisterComponent();
    Floor->SetWorldLocation(FVector(50, 0, 90.5f));
    Movement->InitialProgress = 0.0f;
    Movement->ResetRail();
    TestTrue(TEXT("Floor contact parallel to travel does not block rail"),
        Movement->AdvanceDistance(10.0f));
    Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Movement->InitialProgress = 0.0f;
    Movement->ResetRail();
    TestFalse(TEXT("Perpendicular collision motion does not push rail"),
        Movement->TryCollisionPush(Obstacle, FVector(0, 100, 0), 0.1f));
    TestTrue(TEXT("Collision direction along tangent pushes rail"),
        Movement->TryCollisionPush(
            Obstacle,
            FVector(100, 0, 1000),
            0.1f));
    TestTrue(TEXT("Collision push advances expected distance"),
        FMath::IsNearlyEqual(Movement->GetProgress(), 0.1f));
    Movement->InitialProgress = 0.0f;
    Movement->ResetRail();
    TestTrue(TEXT("Collision push ignores direction magnitude"),
        Movement->TryCollisionPush(
            Obstacle,
            FVector(0.001f, 0, 1000),
            0.1f));
    TestTrue(TEXT("Fixed collision push advances expected distance"),
        FMath::IsNearlyEqual(Movement->GetProgress(), 0.1f));

    Movement->MoveToProgress(1.0f);
    for (int32 Index = 0; Index < 20; ++Index) Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Mechanism command reaches end"), Movement->GetProgress(), 1.0f);
    TestFalse(TEXT("Completed command idles"), Movement->IsComponentTickEnabled());
    Movement->InitialProgress = 0.25f;
    Movement->ResetRail();
    TestEqual(TEXT("Reset restores authored progress"), Movement->GetProgress(), 0.25f);

    Spline->SetSplinePoints({FVector(0, 0, 100), FVector(50, 50, 100), FVector(100, 0, 100)}, ESplineCoordinateSpace::Local);
    Movement->InitialProgress = 0;
    Movement->ResetRail();
    Movement->bFollowRailRotation = true;
    Movement->AdvanceDistance(Spline->GetSplineLength() * 0.5f);
    const float Distance = Spline->GetSplineLength() * Movement->GetProgress();
    TestTrue(TEXT("Curve position stays on spline"), Body->GetComponentLocation().Equals(
        Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World), 0.01f));
    TestTrue(TEXT("Curve departs from straight chord"), Body->GetComponentLocation().Y > 20);
    TestTrue(TEXT("Follow rotation uses spline frame"), Body->GetComponentQuat().Equals(
        Spline->GetQuaternionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World), 0.001f));
    Movement->bFollowRailRotation = false;
    Movement->RotationOffset = FRotator(0, 25, 0);
    Movement->ApplyPose();
    TestTrue(TEXT("Fixed orientation ignores tangent"), Body->GetComponentQuat().Equals(Movement->RotationOffset.Quaternion(), 0.001f));
    Movement->Progress = 0.7f;
    Movement->OnRep_Progress();
    TestTrue(TEXT("Received progress applies pose"), Body->GetComponentLocation().Equals(
        Spline->GetLocationAtDistanceAlongSpline(Spline->GetSplineLength() * 0.7f, ESplineCoordinateSpace::World), 0.01f));
    Spline->SetClosedLoop(true);
    TestFalse(TEXT("Closed loop is rejected explicitly"), Movement->IsConfigured());
    Spline->SetClosedLoop(false);
    Spline->SetSplinePoints({FVector::ZeroVector, FVector::ZeroVector}, ESplineCoordinateSpace::Local);
    TestFalse(TEXT("Zero-length rail rejected"), Movement->IsConfigured());

    // Real arm contract: motion along the rail, sideways input, release, and hold cancellation.
    Spline->SetSplinePoints({FVector(0, 0, 100), FVector(100, 0, 100)}, ESplineCoordinateSpace::Local);
    Movement->InitialProgress = 0;
    Movement->RotationOffset = FRotator::ZeroRotator;
    Movement->ResetRail();
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AActor* ChimeraProxy = World->SpawnActor<AActor>();
    UBoxComponent* SegmentBody = NewObject<UBoxComponent>(ChimeraProxy);
    ChimeraProxy->SetRootComponent(SegmentBody);
    SegmentBody->SetBoxExtent(FVector(10));
    SegmentBody->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    SegmentBody->RegisterComponent();
    SegmentBody->SetWorldLocation(FVector(0, 20, 100));
    UCMPartSlotComponent* PartSlot = NewObject<UCMPartSlotComponent>(ChimeraProxy);
    PartSlot->SetupAttachment(SegmentBody);
    PartSlot->RegisterComponent();
    ACMArmPart* Arm = World->SpawnActor<ACMArmPart>();
    Arm->DispatchBeginPlay(); // Initializes health; a pre-BeginPlay native part is not operational.
    if (TestNotNull(TEXT("Arm slot"), PartSlot))
    {
        TestTrue(TEXT("Attach arm"), Arm->AttachToComponent(
            PartSlot, FAttachmentTransformRules::KeepWorldTransform));
        Arm->SynchronizeAttachedPartSlot(PartSlot);
        TestTrue(TEXT("Attached arm is operational"), Arm->IsOperational());
        Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Arm->SetActorLocation(FVector(0, 20, 100));
        FCMArmHoldSpec Spec;
        TestTrue(TEXT("Handle can be queried"), Movement->QueryArmHold(Arm, Spec));
        TestTrue(TEXT("Begin real hold"), Movement->BeginArmHold(Arm));
        TestFalse(TEXT("Second hold is rejected"), Movement->QueryArmHold(Arm, Spec));
        Arm->BeginInteractableHold(Grip, Grip->GetComponentLocation(), FVector::UpVector);
        Arm->SetActorLocation(FVector(10, 20, 100));
        Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
        TestTrue(TEXT("Arm motion drives rail"), FMath::IsNearlyEqual(Movement->GetProgress(), 0.1f));
        Arm->SetActorLocation(FVector(10, 30, 100));
        Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
        TestTrue(TEXT("Sideways input is ignored"), FMath::IsNearlyEqual(Movement->GetProgress(), 0.1f));
        Movement->EndArmHold(Arm);
        TestTrue(TEXT("Actual release preserves pose"), FMath::IsNearlyEqual(Movement->GetProgress(), 0.1f));
        TestFalse(TEXT("Release clears arm anchor"), Arm->IsHolding());
        Movement->SetInteractionEnabled(false);
        TestFalse(TEXT("Disabled interaction rejects hold"), Movement->BeginArmHold(Arm));
        Movement->SetInteractionEnabled(true);
        TestTrue(TEXT("Regrab"), Movement->BeginArmHold(Arm));
        Arm->EndGroundAnchor();
        Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
        TestFalse(TEXT("Cancelled arm releases rail"), Movement->bTrackingHold);
    }

    // Endpoint connections form a graph, including a second junction downstream.
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Spline->SetSplinePoints(
        {FVector(0, 0, 100), FVector(100, 0, 100)},
        ESplineCoordinateSpace::Local);
    USplineComponent* NorthBranch = NewObject<USplineComponent>(Owner);
    NorthBranch->SetupAttachment(Root);
    NorthBranch->RegisterComponent();
    NorthBranch->SetSplinePoints(
        {FVector(100, 0, 100), FVector(100, 100, 100)},
        ESplineCoordinateSpace::Local);
    USplineComponent* SouthBranch = NewObject<USplineComponent>(Owner);
    SouthBranch->SetupAttachment(Root);
    SouthBranch->RegisterComponent();
    SouthBranch->SetSplinePoints(
        {FVector(100, 0, 100), FVector(100, -100, 100)},
        ESplineCoordinateSpace::Local);
    USplineComponent* WestBranch = NewObject<USplineComponent>(Owner);
    WestBranch->SetupAttachment(Root);
    WestBranch->RegisterComponent();
    WestBranch->SetSplinePoints(
        {FVector(100, 100, 100), FVector(0, 100, 100)},
        ESplineCoordinateSpace::Local);
    USplineComponent* EastBranch = NewObject<USplineComponent>(Owner);
    EastBranch->SetupAttachment(Root);
    EastBranch->RegisterComponent();
    EastBranch->SetSplinePoints(
        {FVector(100, 100, 100), FVector(200, 100, 100)},
        ESplineCoordinateSpace::Local);
    Movement->InitialSegmentIndex = 0;
    Movement->InitialProgress = 0.0f;
    Movement->ConfigureRailGraph(
        {Spline, NorthBranch, SouthBranch, WestBranch, EastBranch},
        Body,
        Grip);
    TestTrue(TEXT("Entry reaches branch junction"), Movement->AdvanceDistance(100.0f));
    TestTrue(TEXT("Entry end is a junction"), Movement->IsAtConnectedJunction());
    TestTrue(TEXT("North input selects north branch"),
        Movement->TryCollisionPush(Obstacle, FVector(0, 100, 0), 0.1f));
    TestEqual(TEXT("North segment index"), Movement->GetActiveSegmentIndex(), 1);
    TestTrue(TEXT("North branch advances"), FMath::IsNearlyEqual(Movement->GetProgress(), 0.1f));
    TestTrue(TEXT("North segment reaches second junction"),
        Movement->TryCollisionPush(Obstacle, FVector(0, 100, 0), 1.0f));
    TestTrue(TEXT("Second junction is connected"), Movement->IsAtConnectedJunction());
    TestTrue(TEXT("East input selects downstream east segment"),
        Movement->TryCollisionPush(Obstacle, FVector(100, 0, 0), 0.1f));
    TestEqual(TEXT("East segment index"), Movement->GetActiveSegmentIndex(), 4);
    TestTrue(TEXT("East segment advances"), FMath::IsNearlyEqual(Movement->GetProgress(), 0.1f));
    TestTrue(TEXT("Reverse input returns to second junction"),
        Movement->TryCollisionPush(Obstacle, FVector(-100, 0, 0), 1.0f));
    TestTrue(TEXT("Returned endpoint remains connected"), Movement->IsAtConnectedJunction());
    TestTrue(TEXT("West input selects alternate downstream segment"),
        Movement->TryCollisionPush(Obstacle, FVector(-100, 0, 0), 0.1f));
    TestEqual(TEXT("West segment index"), Movement->GetActiveSegmentIndex(), 3);

    World->DestroyWorld(false);
    return true;
}
#endif
