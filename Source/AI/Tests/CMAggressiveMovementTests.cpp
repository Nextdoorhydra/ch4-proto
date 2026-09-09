#include "Aggressive/Common/Core/CMAggressiveAITypes.h"
#include "Aggressive/Common/Movement/CMAIFixedLegActuatorComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "Aggressive/Centipede/CMCentipedePawn.h"
#include "Aggressive/Ripper/CMRipperPawn.h"
#include "Aggressive/Tetra/CMTetraPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
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

    TestEqual(TEXT("Scaled leg penetration is lifted to the ground plane"), CMAIFixedLegActuation::CalculateRequiredGroundLift(-20.0f, 0.0f, 150.0f, 0.5f), 20.0f);
    TestEqual(TEXT("A touching leg does not move the actor"), CMAIFixedLegActuation::CalculateRequiredGroundLift(0.0f, 0.0f, 150.0f, 0.5f), 0.0f);
    TestEqual(TEXT("A leg above the ground does not move the actor"), CMAIFixedLegActuation::CalculateRequiredGroundLift(10.0f, 0.0f, 150.0f, 0.5f), 0.0f);
    TestEqual(TEXT("An implausibly deep placement is not teleported"), CMAIFixedLegActuation::CalculateRequiredGroundLift(-200.0f, 0.0f, 150.0f, 0.5f), 0.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMAggressiveGroundPlacementTest, "Chimera.AI.Aggressive.Movement.GroundPlacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMAggressiveGroundPlacementTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
    const UWorld::InitializationValues InitValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &InitValues);
    if (!TestNotNull(TEXT("Ground placement test world"), World))
    {
        return false;
    }

    AActor* Floor = World->SpawnActor<AActor>(FVector(0.0f, 0.0f, -10.0f), FRotator::ZeroRotator);
    UBoxComponent* FloorCollision = NewObject<UBoxComponent>(Floor, TEXT("FloorCollision"));
    Floor->SetRootComponent(FloorCollision);
    Floor->AddInstanceComponent(FloorCollision);
    FloorCollision->SetBoxExtent(FVector(2000.0f, 2000.0f, 10.0f));
    FloorCollision->SetCollisionProfileName(TEXT("BlockAll"));
    FloorCollision->RegisterComponent();
    Floor->SetActorLocation(FVector(0.0f, 0.0f, -10.0f));

    AActor* RaisedFloor = World->SpawnActor<AActor>(FVector(3000.0f, 0.0f, 490.0f), FRotator::ZeroRotator);
    UBoxComponent* RaisedFloorCollision = NewObject<UBoxComponent>(RaisedFloor, TEXT("RaisedFloorCollision"));
    RaisedFloor->SetRootComponent(RaisedFloorCollision);
    RaisedFloor->AddInstanceComponent(RaisedFloorCollision);
    RaisedFloorCollision->SetBoxExtent(FVector(1000.0f, 1000.0f, 10.0f));
    RaisedFloorCollision->SetCollisionProfileName(TEXT("BlockAll"));
    RaisedFloorCollision->RegisterComponent();
    RaisedFloor->SetActorLocation(FVector(3000.0f, 0.0f, 490.0f));

    ACMRipperPawn* ScaledRipper = World->SpawnActor<ACMRipperPawn>(FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator);
    ScaledRipper->SetActorScale3D(FVector(1.2f));
    ScaledRipper->DispatchBeginPlay();
    TestEqual(TEXT("A scaled Ripper body center keeps the leg offset above the floor"), ScaledRipper->GetActorLocation().Z, 120.0, 0.1);
    TestEqual(TEXT("A scaled Ripper root bounds bottom rests on the floor"), ScaledRipper->GetRootComponent()->Bounds.GetBox().Min.Z, 0.0, 0.1);
    TestTrue(TEXT("A scaled grounded Ripper can activate a leg"), ScaledRipper->ActivateLeg(0));
    for (const UStaticMeshComponent* LegMesh : ScaledRipper->GetLegMeshes())
        TestEqual(TEXT("Every scaled Ripper leg bottom rests on the floor"), LegMesh ? LegMesh->Bounds.GetBox().Min.Z : -1.0, 0.0, 0.1);

    ACMRipperPawn* LowRipper = World->SpawnActor<ACMRipperPawn>(FVector(500.0f, 0.0f, 60.0f), FRotator::ZeroRotator);
    LowRipper->DispatchBeginPlay();
    TestEqual(TEXT("A low Ripper placement is raised to its required body height"), LowRipper->GetActorLocation().Z, 100.0, 0.1);
    TestTrue(TEXT("A corrected Ripper can activate a grounded leg"), LowRipper->ActivateLeg(0));

    UClass* RipperBlueprintClass = LoadClass<ACMRipperPawn>(nullptr, TEXT("/Game/Chimera/AI/Ripper/BP_CMRipperPawn.BP_CMRipperPawn_C"));
    ACMRipperPawn* BlueprintRipper = RipperBlueprintClass ? World->SpawnActor<ACMRipperPawn>(RipperBlueprintClass, FVector(-1000.0f, 0.0f, 100.0f), FRotator::ZeroRotator) : nullptr;
    if (TestNotNull(TEXT("Ripper Blueprint instance"), BlueprintRipper))
    {
        BlueprintRipper->DispatchBeginPlay();
        TestEqual(TEXT("A Ripper Blueprint root bounds bottom rests on the floor"), BlueprintRipper->GetRootComponent()->Bounds.GetBox().Min.Z, 0.0, 0.1);
        TestTrue(TEXT("A grounded Ripper Blueprint can activate a leg"), BlueprintRipper->ActivateLeg(0));
        for (const UStaticMeshComponent* LegMesh : BlueprintRipper->GetLegMeshes())
            TestEqual(TEXT("Every Ripper Blueprint leg bottom rests on the floor"), LegMesh ? LegMesh->Bounds.GetBox().Min.Z : -1.0, 0.0, 0.1);
    }

    ACMCentipedePawn* DroppedCentipede = World->SpawnActor<ACMCentipedePawn>(FVector(1000.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
    DroppedCentipede->DispatchBeginPlay();
    TestEqual(TEXT("A Centipede body center keeps the leg offset above the floor"), DroppedCentipede->GetActorLocation().Z, 100.0, 0.1);
    TestEqual(TEXT("A Centipede root bounds bottom rests on the floor"), DroppedCentipede->GetRootComponent()->Bounds.GetBox().Min.Z, 0.0, 0.1);
    TestTrue(TEXT("A grounded Centipede can activate a leg"), DroppedCentipede->ActivateLeg(0));
    for (const UStaticMeshComponent* LegMesh : DroppedCentipede->GetLegMeshes())
        TestEqual(TEXT("Every Centipede leg bottom rests on the floor"), LegMesh ? LegMesh->Bounds.GetBox().Min.Z : -1.0, 0.0, 0.1);

    ACMCentipedePawn* LowCentipede = World->SpawnActor<ACMCentipedePawn>(FVector(1250.0f, 0.0f, 60.0f), FRotator::ZeroRotator);
    LowCentipede->DispatchBeginPlay();
    TestEqual(TEXT("A low Centipede placement is raised to its required body height"), LowCentipede->GetActorLocation().Z, 100.0, 0.1);
    TestTrue(TEXT("A corrected Centipede can activate a grounded leg"), LowCentipede->ActivateLeg(0));

    UClass* CentipedeBlueprintClass = LoadClass<ACMCentipedePawn>(nullptr, TEXT("/Game/Chimera/AI/Centipede/BP_CMCentipedePawn.BP_CMCentipedePawn_C"));
    ACMCentipedePawn* BlueprintCentipede = CentipedeBlueprintClass ? World->SpawnActor<ACMCentipedePawn>(CentipedeBlueprintClass, FVector(1500.0f, 0.0f, 100.0f), FRotator::ZeroRotator) : nullptr;
    if (TestNotNull(TEXT("Centipede Blueprint instance"), BlueprintCentipede))
    {
        BlueprintCentipede->DispatchBeginPlay();
        TestEqual(TEXT("A Centipede Blueprint root bounds bottom rests on the floor"), BlueprintCentipede->GetRootComponent()->Bounds.GetBox().Min.Z, 0.0, 0.1);
        TestTrue(TEXT("A grounded Centipede Blueprint can activate a leg"), BlueprintCentipede->ActivateLeg(0));
        for (const UStaticMeshComponent* LegMesh : BlueprintCentipede->GetLegMeshes())
            TestEqual(TEXT("Every Centipede Blueprint leg bottom rests on the floor"), LegMesh ? LegMesh->Bounds.GetBox().Min.Z : -1.0, 0.0, 0.1);
    }

    ACMCentipedePawn* RaisedCentipede = World->SpawnActor<ACMCentipedePawn>(FVector(3000.0f, 0.0f, 600.0f), FRotator::ZeroRotator);
    RaisedCentipede->DispatchBeginPlay();
    TestEqual(TEXT("Centipede keeps its body offset above a Z=500 floor"), RaisedCentipede->GetActorLocation().Z, 600.0, 0.1);
    TestEqual(TEXT("Centipede leg bounds rest on a Z=500 floor"), RaisedCentipede->GetRootComponent()->Bounds.GetBox().Min.Z, 500.0, 0.1);
    TestTrue(TEXT("Centipede can activate a leg on a Z=500 floor"), RaisedCentipede->ActivateLeg(0));

    ACMTetraPawn* DroppedTetra = World->SpawnActor<ACMTetraPawn>(FVector(500.0f, 500.0f, 50.0f), FRotator::ZeroRotator);
    DroppedTetra->DispatchBeginPlay();
    TestEqual(TEXT("A Tetra body center stays above the floor"), DroppedTetra->GetActorLocation().Z, 50.0, 0.1);
    TestEqual(TEXT("A Tetra root bounds bottom rests on the floor"), DroppedTetra->GetRootComponent()->Bounds.GetBox().Min.Z, 0.0, 0.1);
    TestTrue(TEXT("A grounded Tetra accepts movement input"), DroppedTetra->SetManualMoveDirection(ECMAggressiveMoveDirection::Forward));
    for (const UStaticMeshComponent* LegMesh : DroppedTetra->GetLegMeshes())
        TestEqual(TEXT("Every Tetra leg bottom rests on the floor"), LegMesh ? LegMesh->Bounds.GetBox().Min.Z : -1.0, 0.0, 0.1);

    UClass* TetraBlueprintClass = LoadClass<ACMTetraPawn>(nullptr, TEXT("/Game/Chimera/AI/Tetra/BP_CMTetraPawn.BP_CMTetraPawn_C"));
    ACMTetraPawn* BlueprintTetra = TetraBlueprintClass ? World->SpawnActor<ACMTetraPawn>(TetraBlueprintClass, FVector(-500.0f, 500.0f, 50.0f), FRotator::ZeroRotator) : nullptr;
    if (TestNotNull(TEXT("Tetra Blueprint instance"), BlueprintTetra))
    {
        BlueprintTetra->DispatchBeginPlay();
        TestEqual(TEXT("A Tetra Blueprint root bounds bottom rests on the floor"), BlueprintTetra->GetRootComponent()->Bounds.GetBox().Min.Z, 0.0, 0.1);
        TestTrue(TEXT("A grounded Tetra Blueprint accepts movement input"), BlueprintTetra->SetManualMoveDirection(ECMAggressiveMoveDirection::Forward));
        for (const UStaticMeshComponent* LegMesh : BlueprintTetra->GetLegMeshes())
            TestEqual(TEXT("Every Tetra Blueprint leg bottom rests on the floor"), LegMesh ? LegMesh->Bounds.GetBox().Min.Z : -1.0, 0.0, 0.1);
    }

    World->DestroyWorld(false);
    return true;
}

#endif
