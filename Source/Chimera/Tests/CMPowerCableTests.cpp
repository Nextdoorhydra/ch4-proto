#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Stage/Trigger/CMPowerCableActor.h"
#include "Stage/Trigger/Component/CMPowerSocketComponent.h"
#include "Stage/Trigger/Component/CMPowerSourceComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMPowerCableCheckpointResetTest,
    "Chimera.Power.CableCheckpointReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMPowerCableCheckpointResetTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> ScriptExecutionGuard(
        GAllowActorScriptExecutionInEditor, true);
    const UWorld::InitializationValues Init = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false)
        .ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
        nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("World"), World))
    {
        return false;
    }

    auto AddRoot = [](AActor* Owner, const FVector& Location)
    {
        USceneComponent* Root = NewObject<USceneComponent>(Owner);
        Owner->SetRootComponent(Root);
        Owner->AddInstanceComponent(Root);
        Root->RegisterComponent();
        Owner->SetActorLocation(Location);
    };

    AActor* SourceOwner = World->SpawnActor<AActor>();
    AddRoot(SourceOwner, FVector::ZeroVector);
    UCMPowerSourceComponent* Source =
        NewObject<UCMPowerSourceComponent>(SourceOwner);
    SourceOwner->AddInstanceComponent(Source);
    Source->SetupAttachment(SourceOwner->GetRootComponent());
    Source->RegisterComponent();

    AActor* InitialSocketOwner = World->SpawnActor<AActor>();
    AddRoot(InitialSocketOwner, FVector(300.0f, 0.0f, 0.0f));
    UCMPowerSocketComponent* InitialSocket =
        NewObject<UCMPowerSocketComponent>(InitialSocketOwner);
    InitialSocketOwner->AddInstanceComponent(InitialSocket);
    InitialSocket->SetupAttachment(InitialSocketOwner->GetRootComponent());
    InitialSocket->RegisterComponent();

    AActor* OtherSocketOwner = World->SpawnActor<AActor>();
    AddRoot(OtherSocketOwner, FVector(600.0f, 0.0f, 0.0f));
    UCMPowerSocketComponent* OtherSocket =
        NewObject<UCMPowerSocketComponent>(OtherSocketOwner);
    OtherSocketOwner->AddInstanceComponent(OtherSocket);
    OtherSocket->SetupAttachment(OtherSocketOwner->GetRootComponent());
    OtherSocket->RegisterComponent();

    const FVector InitialLocation(100.0f, 50.0f, 20.0f);
    ACMPowerCableActor* Cable = World->SpawnActor<ACMPowerCableActor>(
        InitialLocation, FRotator::ZeroRotator);
    Cable->bAutoConnectOnSpawn = true;
    Cable->InputActor = SourceOwner;
    Cable->OutputActor = InitialSocketOwner;
    Cable->DispatchBeginPlay();

    TestTrue(TEXT("Cable starts fully connected"),
        Cable->IsFullyConnected());
    TestTrue(TEXT("Initial source owns cable"),
        Source->GetConnectedCable() == Cable);
    TestTrue(TEXT("Initial socket owns cable"),
        InitialSocket->GetConnectedCable() == Cable);

    Cable->Disconnect();
    TestTrue(TEXT("Cable can move to another socket"),
        Cable->TryConnectToSocket(OtherSocket, true));
    Cable->SetActorLocation(FVector(900.0f, 200.0f, 100.0f));
    Cable->CableStartLocation = Cable->GetActorLocation();
    Cable->bCableHasBeenMoved = true;
    Cable->Grabber = World->SpawnActor<AActor>();

    Cable->ResetForCheckpoint();

    TestFalse(TEXT("Checkpoint reset releases cable"), Cable->IsGrabbed());
    TestTrue(TEXT("Checkpoint reset restores actor transform"),
        Cable->GetActorLocation().Equals(InitialLocation, 0.1f));
    TestTrue(TEXT("Checkpoint reset restores initial source"),
        Source->GetConnectedCable() == Cable);
    TestTrue(TEXT("Checkpoint reset restores initial socket"),
        InitialSocket->GetConnectedCable() == Cable);
    TestFalse(TEXT("Checkpoint reset clears later socket"),
        OtherSocket->IsPhysicallyConnected());
    TestTrue(TEXT("Checkpoint reset restores full connection"),
        Cable->IsFullyConnected());

    World->DestroyWorld(false);
    return true;
}

#endif
