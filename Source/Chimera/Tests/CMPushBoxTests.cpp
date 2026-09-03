#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Parts/Arm/CMArmPart.h"
#include "Player/CMChimera.h"
#include "Stage/Obstacle/CMPushBox.h"
#include "Stage/Trigger/Component/CMMechanismWeightComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMPushBoxMovementTest, "Chimera.Obstacle.PushBox.Movement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMPushBoxMovementTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
    const UWorld::InitializationValues InitValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &InitValues);
    if (!TestNotNull(TEXT("World"), World)) return false;

    AActor* Floor = World->SpawnActor<AActor>(FVector(0.0f, 0.0f, 40.0f), FRotator::ZeroRotator);
    UBoxComponent* FloorCollision = NewObject<UBoxComponent>(Floor);
    Floor->SetRootComponent(FloorCollision);
    Floor->AddInstanceComponent(FloorCollision);
    FloorCollision->SetBoxExtent(FVector(1000.0f, 1000.0f, 10.0f));
    FloorCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    FloorCollision->RegisterComponent();
    Floor->SetActorLocation(FVector(0.0f, 0.0f, 40.0f));

    UClass* PushBoxBlueprintClass = LoadClass<ACMPushBox>(nullptr, TEXT("/Game/Chimera/Environment/Obstacle/Box/BP_CMPushBox.BP_CMPushBox_C"));
    TestNotNull(TEXT("Push box Blueprint class"), PushBoxBlueprintClass);
    ACMPushBox* BlueprintPushBox = PushBoxBlueprintClass ? World->SpawnActor<ACMPushBox>(PushBoxBlueprintClass, FVector(-500.0f, 0.0f, 100.0f), FRotator::ZeroRotator) : nullptr;
    UStaticMeshComponent* BlueprintBoxMesh = BlueprintPushBox ? BlueprintPushBox->FindComponentByClass<UStaticMeshComponent>() : nullptr;
    if (BlueprintPushBox && BlueprintBoxMesh)
    {
        const float CurrentBottom = BlueprintBoxMesh->Bounds.Origin.Z - BlueprintBoxMesh->Bounds.BoxExtent.Z;
        BlueprintPushBox->AddActorWorldOffset(FVector(0.0f, 0.0f, 50.0f - CurrentBottom));
    }
    if (BlueprintPushBox) BlueprintPushBox->DispatchBeginPlay();
    TestTrue(TEXT("Push box Blueprint tick is enabled"), BlueprintPushBox && BlueprintPushBox->IsActorTickEnabled());
    TestTrue(TEXT("Push box Blueprint generates overlap events"), BlueprintBoxMesh && BlueprintBoxMesh->GetGenerateOverlapEvents());

    ACMPushBox* PushBox = World->SpawnActor<ACMPushBox>(FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
    ACMArmPart* ArmPart = World->SpawnActor<ACMArmPart>();
    AActor* Attacker = World->SpawnActor<AActor>(FVector(80.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
    UBoxComponent* AttackerCollision = NewObject<UBoxComponent>(Attacker);
    Attacker->SetRootComponent(AttackerCollision);
    Attacker->AddInstanceComponent(AttackerCollision);
    AttackerCollision->SetBoxExtent(FVector(40.0f));
    AttackerCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    AttackerCollision->RegisterComponent();
    Attacker->SetActorLocation(FVector(80.0f, 0.0f, 100.0f));
    PushBox->DispatchBeginPlay();

    UStaticMeshComponent* BoxMesh = PushBox->FindComponentByClass<UStaticMeshComponent>();
    UCMMechanismWeightComponent* Weight = PushBox->FindComponentByClass<UCMMechanismWeightComponent>();
    TestNotNull(TEXT("Box mesh"), BoxMesh);
    TestNotNull(TEXT("Mechanism weight"), Weight);
    if (!BoxMesh || !Weight)
    {
        World->DestroyWorld(false);
        return false;
    }
    TestEqual(TEXT("Default gameplay weight"), Weight->GetMechanismWeight(), 40.0f);

    FCMCombatHitRequest Request;
    Request.Attacker = Attacker;
    Request.SourcePart = ArmPart;
    Request.AttackId = FGuid::NewGuid();
    Request.ImpactDirection = FVector::ForwardVector;
    if (BlueprintPushBox)
    {
        const FVector BlueprintStartLocation = BlueprintPushBox->GetActorLocation();
        FCMCombatHitRequest BlueprintRequest = Request;
        BlueprintRequest.AttackId = FGuid::NewGuid();
        TestTrue(TEXT("Push box Blueprint arm hit accepted"), ICMCombatHitTarget::Execute_ReceiveCombatHit(BlueprintPushBox, BlueprintRequest));
        BlueprintPushBox->Tick(0.1f);
        TestTrue(TEXT("Push box Blueprint moves on the floor"), FMath::IsNearlyEqual(BlueprintPushBox->GetActorLocation().X - BlueprintStartLocation.X, 30.0f, 0.1f));
    }
    TestTrue(TEXT("Arm hit accepted"), ICMCombatHitTarget::Execute_ReceiveCombatHit(PushBox, Request));
    TestTrue(TEXT("Push source ignored during movement"), BoxMesh->GetMoveIgnoreActors().Contains(Attacker));

    PushBox->Tick(0.1f);
    TestTrue(TEXT("Fixed speed moves 30 cm in 0.1 seconds"), FMath::IsNearlyEqual(PushBox->GetActorLocation().X, 30.0f, 0.1f));
    PushBox->Tick(1.0f);
    TestTrue(TEXT("Fixed push distance is 100 cm"), FMath::IsNearlyEqual(PushBox->GetActorLocation().X, 100.0f, 0.1f));
    TestFalse(TEXT("Push source ignore is cleared after movement"), BoxMesh->GetMoveIgnoreActors().Contains(Attacker));

    AActor* EnvironmentBlocker = World->SpawnActor<AActor>(FVector(180.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
    UBoxComponent* EnvironmentCollision = NewObject<UBoxComponent>(EnvironmentBlocker);
    EnvironmentBlocker->SetRootComponent(EnvironmentCollision);
    EnvironmentBlocker->AddInstanceComponent(EnvironmentCollision);
    EnvironmentCollision->SetBoxExtent(FVector(10.0f));
    EnvironmentCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    EnvironmentCollision->RegisterComponent();
    EnvironmentBlocker->SetActorLocation(FVector(180.0f, 0.0f, 100.0f));
    Request.AttackId = FGuid::NewGuid();
    TestTrue(TEXT("Second arm hit accepted"), ICMCombatHitTarget::Execute_ReceiveCombatHit(PushBox, Request));
    PushBox->Tick(1.0f);
    TestTrue(TEXT("Environment blocker stops the box"), PushBox->GetActorLocation().X < 130.0f);

    ACMPushBox* WallAdjacentPushBox = World->SpawnActor<ACMPushBox>(FVector(0.0f, 400.0f, 100.0f), FRotator::ZeroRotator);
    WallAdjacentPushBox->DispatchBeginPlay();
    AActor* AdjacentWall = World->SpawnActor<AActor>(FVector(0.0f, 342.0f, 100.0f), FRotator::ZeroRotator);
    UBoxComponent* AdjacentWallCollision = NewObject<UBoxComponent>(AdjacentWall);
    AdjacentWall->SetRootComponent(AdjacentWallCollision);
    AdjacentWall->AddInstanceComponent(AdjacentWallCollision);
    AdjacentWallCollision->SetBoxExtent(FVector(200.0f, 10.0f, 200.0f));
    AdjacentWallCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    AdjacentWallCollision->RegisterComponent();
    AdjacentWall->SetActorLocation(FVector(0.0f, 342.0f, 100.0f));
    FCMCombatHitRequest WallAdjacentRequest = Request;
    WallAdjacentRequest.AttackId = FGuid::NewGuid();
    WallAdjacentRequest.ImpactDirection = FVector::ForwardVector;
    TestTrue(TEXT("Wall-adjacent arm hit accepted"), ICMCombatHitTarget::Execute_ReceiveCombatHit(WallAdjacentPushBox, WallAdjacentRequest));
    WallAdjacentPushBox->Tick(0.1f);
    TestTrue(TEXT("Wall contact does not block movement parallel to the wall"), FMath::IsNearlyEqual(WallAdjacentPushBox->GetActorLocation().X, 30.0f, 0.1f));

    AActor* BackWall = World->SpawnActor<AActor>(FVector(-58.0f, 700.0f, 100.0f), FRotator::ZeroRotator);
    UBoxComponent* BackWallCollision = NewObject<UBoxComponent>(BackWall);
    BackWall->SetRootComponent(BackWallCollision);
    BackWall->AddInstanceComponent(BackWallCollision);
    BackWallCollision->SetBoxExtent(FVector(10.0f, 200.0f, 200.0f));
    BackWallCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    BackWallCollision->RegisterComponent();
    BackWall->SetActorLocation(FVector(-58.0f, 700.0f, 100.0f));
    ACMPushBox* BackWallPushBox = World->SpawnActor<ACMPushBox>(FVector(0.0f, 700.0f, 100.0f), FRotator::ZeroRotator);
    BackWallPushBox->DispatchBeginPlay();
    FCMCombatHitRequest BackWallRequest = Request;
    BackWallRequest.AttackId = FGuid::NewGuid();
    BackWallRequest.ImpactDirection = FVector::ForwardVector;
    TestTrue(TEXT("Back-wall arm hit accepted"), ICMCombatHitTarget::Execute_ReceiveCombatHit(BackWallPushBox, BackWallRequest));
    BackWallPushBox->Tick(0.1f);
    TestTrue(TEXT("Wall contact does not block movement away from the wall"), FMath::IsNearlyEqual(BackWallPushBox->GetActorLocation().X, 30.0f, 0.1f));

    ACMPushBox* IntoWallPushBox = World->SpawnActor<ACMPushBox>(FVector(0.0f, 900.0f, 100.0f), FRotator::ZeroRotator);
    IntoWallPushBox->DispatchBeginPlay();
    AActor* IntoWall = World->SpawnActor<AActor>(FVector(-58.0f, 900.0f, 100.0f), FRotator::ZeroRotator);
    UBoxComponent* IntoWallCollision = NewObject<UBoxComponent>(IntoWall);
    IntoWall->SetRootComponent(IntoWallCollision);
    IntoWall->AddInstanceComponent(IntoWallCollision);
    IntoWallCollision->SetBoxExtent(FVector(10.0f, 200.0f, 200.0f));
    IntoWallCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    IntoWallCollision->RegisterComponent();
    IntoWall->SetActorLocation(FVector(-58.0f, 900.0f, 100.0f));
    FCMCombatHitRequest IntoWallRequest = Request;
    IntoWallRequest.AttackId = FGuid::NewGuid();
    IntoWallRequest.ImpactDirection = FVector::BackwardVector;
    TestTrue(TEXT("Into-wall arm hit accepted"), ICMCombatHitTarget::Execute_ReceiveCombatHit(IntoWallPushBox, IntoWallRequest));
    IntoWallPushBox->Tick(0.1f);
    TestTrue(TEXT("Wall still blocks movement into the wall"), FMath::IsNearlyZero(IntoWallPushBox->GetActorLocation().X, 0.1f));

    ACMPushBox* PlayerPushBox = World->SpawnActor<ACMPushBox>(FVector(0.0f, 200.0f, 100.0f), FRotator::ZeroRotator);
    ACMChimera* Chimera = World->SpawnActor<ACMChimera>();
    UStaticMeshComponent* PlayerPushBoxMesh = PlayerPushBox->FindComponentByClass<UStaticMeshComponent>();
    UStaticMeshComponent* PlayerBody = Chimera->FindComponentByClass<UStaticMeshComponent>();
    PlayerPushBox->DispatchBeginPlay();
    TestNotNull(TEXT("Player push box mesh"), PlayerPushBoxMesh);
    TestNotNull(TEXT("Player body"), PlayerBody);
    if (PlayerPushBoxMesh && PlayerBody)
    {
        PlayerBody->SetWorldLocation(FVector(-100.0f, 200.0f, 100.0f));
        PlayerBody->SetSimulatePhysics(true);
        PlayerBody->SetPhysicsLinearVelocity(FVector(100.0f, 0.0f, 0.0f));
        FHitResult PlayerHit;
        PlayerHit.ImpactPoint = FVector(-50.0f, 200.0f, 100.0f);
        PlayerPushBoxMesh->OnComponentHit.Broadcast(PlayerPushBoxMesh, Chimera, PlayerBody, FVector(100.0f, 0.0f, 0.0f), PlayerHit);
        PlayerPushBox->Tick(0.1f);
        TestTrue(TEXT("Moving player contact pushes the box"), FMath::IsNearlyEqual(PlayerPushBox->GetActorLocation().X, 30.0f, 0.1f));
    }

    World->DestroyWorld(false);
    return true;
}

#endif
