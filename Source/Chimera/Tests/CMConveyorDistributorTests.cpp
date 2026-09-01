#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Parts/Leg/CMLegPart.h"
#include "Stage/CMStageDirector.h"
#include "Stage/CMStageElementComponent.h"
#include "Stage/Obstacle/CMConveyorDistributorActor.h"
#include "Stage/Obstacle/CMConveyorSegmentActor.h"
#include "Stage/Obstacle/CMConveyorSplineActor.h"
#include "Stage/Obstacle/CMConveyorTransportTags.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMConveyorDistributorTest,
    "Chimera.Obstacle.ConveyorDistributor.Routing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMConveyorDistributorTest::RunTest(const FString& Parameters)
{
    bool bReachedEnd = false;
    float Progress = ACMConveyorDistributorActor::CalculateNextProgress(0.4f, 100.0f, 20.0f, bReachedEnd);
    TestEqual(TEXT("Forward movement uses physical path length"), Progress, 0.6f);
    TestFalse(TEXT("Interior movement does not report an end"), bReachedEnd);
    TestTrue(TEXT("Cross mode changes lanes at the center"), ACMConveyorDistributorActor::ShouldCrossLane(0.4f, Progress, ECMConveyorDistributorRoutingMode::Cross));
    TestFalse(TEXT("Straight mode preserves the lane at the center"), ACMConveyorDistributorActor::ShouldCrossLane(0.4f, Progress, ECMConveyorDistributorRoutingMode::Straight));

    Progress = ACMConveyorDistributorActor::CalculateNextProgress(0.6f, 100.0f, -20.0f, bReachedEnd);
    TestEqual(TEXT("Reverse movement advances toward the start"), Progress, 0.4f);
    TestTrue(TEXT("Cross mode works in reverse"), ACMConveyorDistributorActor::ShouldCrossLane(0.6f, Progress, ECMConveyorDistributorRoutingMode::Cross));

    Progress = ACMConveyorDistributorActor::CalculateNextProgress(0.95f, 100.0f, 10.0f, bReachedEnd);
    TestEqual(TEXT("Forward movement clamps at the exit"), Progress, 1.0f);
    TestTrue(TEXT("Forward movement reports the exit"), bReachedEnd);

    Progress = ACMConveyorDistributorActor::CalculateNextProgress(0.05f, 100.0f, -10.0f, bReachedEnd);
    TestEqual(TEXT("Reverse movement clamps at the exit"), Progress, 0.0f);
    TestTrue(TEXT("Reverse movement reports the exit"), bReachedEnd);

    const ACMConveyorDistributorActor* DefaultDistributor = GetDefault<ACMConveyorDistributorActor>();
    TestFalse(TEXT("Distributor actor tick remains disabled"), DefaultDistributor->PrimaryActorTick.bCanEverTick);
    TestEqual(TEXT("Distributor defaults to straight routing"), DefaultDistributor->RoutingMode, ECMConveyorDistributorRoutingMode::Straight);
    TestEqual(TEXT("Distributor defaults to forward movement"), DefaultDistributor->Direction, ECMConveyorDistributorDirection::Forward);
    TestTrue(TEXT("Distributor preserves authored part placement by default"), DefaultDistributor->bPreserveInitialPartPlacement);
    TestEqual(TEXT("Distributor uses a grid-aligned two meter length"), DefaultDistributor->DistributorLength, 200.0f);
    TestEqual(TEXT("Distributor uses a grid-aligned lane spacing"), DefaultDistributor->LaneSpacing, 220.0f);

    TArray<USplineComponent*> Paths;
    DefaultDistributor->GetComponents(Paths);
    TestEqual(TEXT("Distributor owns two lane paths"), Paths.Num(), 2);
    for (const USplineComponent* Path : Paths)
    {
        TestEqual(TEXT("Each lane path has two points"), Path->GetNumberOfSplinePoints(), 2);
        TestFalse(TEXT("Distributor lane paths are open"), Path->IsClosedLoop());
        TestFalse(TEXT("Distributor lane paths follow the actor"), Path->IsUsingAbsoluteLocation());
    }

    TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
    const UWorld::InitializationValues InitValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &InitValues);
    if (!TestNotNull(TEXT("Distributor test world"), World)) return false;
    World->SpawnActor<ACMStageDirector>();

    UClass* DistributorClass = LoadClass<ACMConveyorDistributorActor>(nullptr, TEXT("/Game/Chimera/Environment/Obstacle/Conveyor/BP_CMConveyorDistributor.BP_CMConveyorDistributor_C"));
    TestNotNull(TEXT("Distributor Blueprint class loads"), DistributorClass);
    ACMConveyorDistributorActor* Distributor = DistributorClass ? World->SpawnActor<ACMConveyorDistributorActor>(DistributorClass, FVector(1000.0f, 2000.0f, 300.0f), FRotator::ZeroRotator) : nullptr;
    if (TestNotNull(TEXT("Distributor Blueprint spawns"), Distributor))
    {
        TestEqual(TEXT("Box04 is on the default left side"), Distributor->LeftMesh->GetStaticMesh()->GetName(), FString(TEXT("SM_AssemblyLineBox04")));
        TestEqual(TEXT("Box06 is on the default right side"), Distributor->RightMesh->GetStaticMesh()->GetName(), FString(TEXT("SM_AssemblyLineBox06")));
        TestEqual(TEXT("Left mesh preserves its source X pivot"), Distributor->LeftMesh->GetRelativeLocation().X, 0.0);
        TestEqual(TEXT("Small right mesh offsets away from the aligned input"), Distributor->RightMesh->GetRelativeLocation().X, -20.0);
        TestEqual(TEXT("Left mesh uses the grid-aligned lane position"), Distributor->LeftMesh->GetRelativeLocation().Y, -110.0);
        TestEqual(TEXT("Right mesh uses the grid-aligned lane position"), Distributor->RightMesh->GetRelativeLocation().Y, 110.0);
        const FVector LeftStart = Distributor->LeftPath->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
        const FVector RightStart = Distributor->RightPath->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
        const FVector LeftEnd = Distributor->LeftPath->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World);
        const FVector RightEnd = Distributor->RightPath->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World);
        TestEqual(TEXT("Both lane entrances share the same grid X"), LeftStart.X, RightStart.X);
        TestEqual(TEXT("Both lane exits share the same grid X"), LeftEnd.X, RightEnd.X);
        TestEqual(TEXT("Distributor entrance is one meter behind its pivot"), LeftStart.X, 900.0);
        TestEqual(TEXT("Distributor exit is one meter ahead of its pivot"), LeftEnd.X, 1100.0);
        TestEqual(TEXT("Left path uses actor-relative height"), LeftStart.Z, 423.18);
        TestEqual(TEXT("Right path uses actor-relative height"), RightStart.Z, 423.18);
        Distributor->SwapSideMeshes();
        TestEqual(TEXT("Box06 moves to the left side after swap"), Distributor->LeftMesh->GetStaticMesh()->GetName(), FString(TEXT("SM_AssemblyLineBox06")));
        TestEqual(TEXT("Box04 moves to the right side after swap"), Distributor->RightMesh->GetStaticMesh()->GetName(), FString(TEXT("SM_AssemblyLineBox04")));
        TestEqual(TEXT("Swapped small mesh offsets away from its rotated input"), Distributor->LeftMesh->GetRelativeLocation().X, 20.0);
        TestEqual(TEXT("Swapped left mesh rotates 180 degrees"), Distributor->LeftMesh->GetRelativeRotation().Yaw, 180.0);
        TestEqual(TEXT("Swapped right mesh rotates 180 degrees"), Distributor->RightMesh->GetRelativeRotation().Yaw, 180.0);

        Distributor->DispatchBeginPlay();
        ACMConveyorSplineActor* UpstreamConveyor = World->SpawnActor<ACMConveyorSplineActor>();
        ACMLegPart* TransferredPart = World->SpawnActor<ACMLegPart>(LeftStart + FVector(0.0, 0.0, 50.0), FRotator::ZeroRotator);
        if (TestNotNull(TEXT("Upstream conveyor spawns"), UpstreamConveyor) && TestNotNull(TEXT("Transfer test Part spawns"), TransferredPart))
        {
            TestTrue(TEXT("Upstream conveyor hands a Part directly to the distributor"), UpstreamConveyor->TryHandoffToDistributor(TransferredPart));
            TestEqual(TEXT("Distributor owns the transferred Part"), Distributor->GetTrackedPartCount(), 1);
            TestTrue(TEXT("Transferred Part starts the distributor movement timer"), World->GetTimerManager().IsTimerActive(Distributor->MovementTimerHandle));
            const ACMConveyorDistributorActor::FTrackedPartState* TransferredState = Distributor->TrackedParts.Find(TransferredPart);
            if (TestNotNull(TEXT("Transferred Part has distributor state"), TransferredState))
            {
                TestEqual(TEXT("Forward transfer starts at the distributor entrance"), TransferredState->Progress, 0.0f);
                TestEqual(TEXT("Transferred Part enters the closest left lane"), TransferredState->Lane, ECMConveyorDistributorLane::Left);
                ACMConveyorDistributorActor::FTrackedPartState& MutableTransferredState = Distributor->TrackedParts.FindChecked(TransferredPart);
                MutableTransferredState.Progress = 1.0f;
                Distributor->ApplyPartTransform(TransferredPart, MutableTransferredState);
                Distributor->ReleasePart(TransferredPart, true);
                TestEqual(TEXT("Failed output handoff keeps the Part on the distributor"), Distributor->GetTrackedPartCount(), 1);
                TestTrue(TEXT("Failed output handoff restores distributor ownership"), TransferredPart->ActorHasTag(CMConveyorTransportTags::Transporting));
                TestTrue(TEXT("Failed output handoff keeps the retry timer active"), World->GetTimerManager().IsTimerActive(Distributor->MovementTimerHandle));

                ACMConveyorSegmentActor* DownstreamSegment = World->SpawnActor<ACMConveyorSegmentActor>(FVector(LeftEnd.X + 120.0, LeftEnd.Y, 300.0), FRotator::ZeroRotator);
                ACMConveyorSplineActor* DownstreamConveyor = World->SpawnActor<ACMConveyorSplineActor>();
                if (TestNotNull(TEXT("Downstream conveyor segment spawns"), DownstreamSegment) && TestNotNull(TEXT("Downstream conveyor controller spawns"), DownstreamConveyor))
                {
                    DownstreamConveyor->FirstSegment = DownstreamSegment;
                    DownstreamConveyor->FindComponentByClass<UCMStageElementComponent>()->PlacementId = TEXT("Test.Conveyor.Downstream");
                    DownstreamConveyor->DispatchBeginPlay();
                    Distributor->ReleasePart(TransferredPart, true);
                    TestEqual(TEXT("Distributor releases the Part after output handoff"), Distributor->GetTrackedPartCount(), 0);
                    TestEqual(TEXT("Downstream conveyor owns the output Part"), DownstreamConveyor->GetTrackedPartCount(), 1);
                    TestTrue(TEXT("Output handoff starts the downstream conveyor timer"), World->GetTimerManager().IsTimerActive(DownstreamConveyor->MovementTimerHandle));

                    TransferredPart->Destroy();
                    DownstreamConveyor->Destroy();
                    ACMConveyorSegmentActor* NetworkInputSegment = World->SpawnActor<ACMConveyorSegmentActor>(FVector(LeftStart.X - 120.0, LeftStart.Y, 300.0), FRotator::ZeroRotator);
                    ACMConveyorSplineActor* NetworkController = World->SpawnActor<ACMConveyorSplineActor>();
                    if (TestNotNull(TEXT("Network input segment spawns"), NetworkInputSegment) && TestNotNull(TEXT("Network controller spawns"), NetworkController))
                    {
                        NetworkController->FirstSegment = NetworkInputSegment;
                        NetworkController->FindComponentByClass<UCMStageElementComponent>()->PlacementId = TEXT("Test.Conveyor.NetworkController");
                        NetworkController->DispatchBeginPlay();
                        TestEqual(TEXT("One controller discovers the connected distributor"), NetworkController->GetConnectedDistributorCount(), 1);
                        TestEqual(TEXT("One controller creates the output branch controller"), NetworkController->GetAutoGeneratedRouteControllerCount(), 1);
                        NetworkController->SetConveyorDirection(ECMConveyorDirection::Reverse);
                        TestEqual(TEXT("Network direction propagates to the distributor"), Distributor->GetDistributorDirection(), ECMConveyorDistributorDirection::Reverse);
                        TestEqual(TEXT("Network direction propagates to the output branch"), NetworkController->AutoGeneratedRouteControllers[0]->GetConveyorDirection(), ECMConveyorDirection::Reverse);
                    }
                }
            }
        }
    }
    World->DestroyWorld(false);
    return true;
}

#endif
