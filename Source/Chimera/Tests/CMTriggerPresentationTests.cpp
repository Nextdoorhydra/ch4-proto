#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Stage/CMStageDirector.h"
#include "Stage/CMStageElementComponent.h"
#include "Stage/Obstacle/CMPushBox.h"
#include "Stage/Obstacle/CMLaserObstacleBase.h"
#include "Stage/Puzzle/CMStagePuzzleController.h"
#include "Stage/Trigger/CMStageButtonBase.h"
#include "Stage/Trigger/CMPressurePlateBase.h"
#include "Stage/Trigger/Component/CMActivationTriggerComponent.h"
#include "Stage/Trigger/Component/CMMechanismWeightComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMTriggerPresentationTest,
    "Chimera.Trigger.Presentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMTriggerPresentationTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
    const UWorld::InitializationValues InitValues = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
        nullptr, true, ERHIFeatureLevel::Num, &InitValues);
    if (!TestNotNull(TEXT("World"), World)) return false;
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->SpawnActor<ACMStageDirector>();
    ACMStageButtonBase* Button = World->SpawnActor<ACMStageButtonBase>();
    Button->FindComponentByClass<UCMStageElementComponent>()->PlacementId = TEXT("Test.UI.Button");
    Button->SetDirectTargetCommandEnabled(false);
    TestFalse(TEXT("Not ready before initialization"), Button->GetPresentationState().bReady);
    Button->DispatchBeginPlay();
    TestTrue(TEXT("Ready for initial UI query"), Button->GetPresentationState().bReady);
    TestFalse(TEXT("Button has no weight display"), Button->GetPresentationState().bSupportsWeight);
    Button->PressButton(Button);
    TestTrue(TEXT("Pressed snapshot"), Button->GetPresentationState().bTriggered);
    TestFalse(TEXT("One shot blocks activation"), Button->GetPresentationState().bCanActivate);
    Button->DeactivateElement();
    TestFalse(TEXT("Disabled snapshot"), Button->GetPresentationState().bEnabled);
    TestTrue(TEXT("Disabling does not release"), Button->GetPresentationState().bTriggered);
    Button->ResetElement();
    TestFalse(TEXT("Reset clears pressed presentation"), Button->GetPresentationState().bTriggered);
    TestTrue(TEXT("Reset restores eligibility"), Button->GetPresentationState().bCanActivate);

    ACMPressurePlateBase* Plate = World->SpawnActor<ACMPressurePlateBase>();
    Plate->FindComponentByClass<UCMStageElementComponent>()->PlacementId = TEXT("Test.UI.Plate");
    Plate->SetDirectTargetCommandEnabled(false);
    Plate->DispatchBeginPlay();
    ACMPushBox* PushBox = World->SpawnActor<ACMPushBox>(FVector(1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
    PushBox->DispatchBeginPlay();
    UStaticMeshComponent* PushBoxMesh = PushBox->FindComponentByClass<UStaticMeshComponent>();
    TestTrue(TEXT("Pressure volume generates overlap events"), Plate->PressureVolume->GetGenerateOverlapEvents());
    TestEqual(TEXT("Pressure volume uses query collision"), Plate->PressureVolume->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
    TestTrue(TEXT("Push box generates overlap events"), PushBoxMesh && PushBoxMesh->GetGenerateOverlapEvents());
    PushBox->SetActorLocation(Plate->GetActorLocation());
    PushBoxMesh->UpdateOverlaps();
    Plate->PressureVolume->UpdateOverlaps();
    Plate->RefreshOverlaps();
    TestEqual(TEXT("Moving push box into pressure volume contributes weight"), Plate->GetCurrentWeight(), 40.0f);
    PushBox->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f));
    PushBoxMesh->UpdateOverlaps();
    Plate->PressureVolume->UpdateOverlaps();
    Plate->RefreshOverlaps();
    TestEqual(TEXT("Moving push box out of pressure volume removes weight"), Plate->GetCurrentWeight(), 0.0f);

    UCMMechanismWeightComponent* FirstPushBoxWeight = PushBox->FindComponentByClass<UCMMechanismWeightComponent>();
    ACMPushBox* SecondPushBox = World->SpawnActor<ACMPushBox>(FVector(1000.0f, 200.0f, 0.0f), FRotator::ZeroRotator);
    SecondPushBox->DispatchBeginPlay();
    UStaticMeshComponent* SecondPushBoxMesh = SecondPushBox->FindComponentByClass<UStaticMeshComponent>();
    UCMMechanismWeightComponent* SecondPushBoxWeight = SecondPushBox->FindComponentByClass<UCMMechanismWeightComponent>();
    TestNotNull(TEXT("First push box weight"), FirstPushBoxWeight);
    TestNotNull(TEXT("Second push box mesh"), SecondPushBoxMesh);
    TestNotNull(TEXT("Second push box weight"), SecondPushBoxWeight);
    if (!FirstPushBoxWeight || !SecondPushBoxMesh || !SecondPushBoxWeight)
    {
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        return false;
    }
    Plate->RequiredWeight = 1000.0f;
    Plate->ReleaseWeight = 999.0f;
    FirstPushBoxWeight->MechanismWeight = 500.0f;
    SecondPushBoxWeight->MechanismWeight = 500.0f;
    ACMLaserObstacleBase* Laser = World->SpawnActor<ACMLaserObstacleBase>();
    Laser->FindComponentByClass<UCMStageElementComponent>()->PlacementId = TEXT("Test.Pressure.Laser");
    Laser->DispatchBeginPlay();
    ACMStagePuzzleController* PuzzleController = World->SpawnActor<ACMStagePuzzleController>();
    PuzzleController->FindComponentByClass<UCMStageElementComponent>()->PlacementId = TEXT("Test.Pressure.Controller");
    FCMPuzzleTargetCommand LaserCommand;
    LaserCommand.Command = ECMPuzzleElementCommand::Toggle;
    LaserCommand.Targets.Add(Laser);
    FCMPuzzleStep PressureStep;
    PressureStep.Commands.Add(LaserCommand);
    FCMPuzzleChannel PressureChannel;
    PressureChannel.ChannelId = TEXT("Pressure");
    PressureChannel.Triggers.Add(Plate);
    PressureChannel.Steps.Add(PressureStep);
    PuzzleController->PuzzleChannels.Add(PressureChannel);
    PuzzleController->DispatchBeginPlay();
    TestTrue(TEXT("Laser starts active"), Laser->IsElementActive());
    PushBox->SetActorLocation(Plate->GetActorLocation() + FVector(0.0f, -20.0f, 0.0f));
    Plate->RefreshOverlaps();
    TestEqual(TEXT("One 500 kg push box contributes 500 kg"), Plate->GetCurrentWeight(), 500.0f);
    TestFalse(TEXT("One 500 kg push box does not meet 1000 kg requirement"), Plate->GetPresentationState().bTriggered);
    SecondPushBox->SetActorLocation(Plate->GetActorLocation() + FVector(0.0f, 20.0f, 0.0f));
    Plate->RefreshOverlaps();
    TestEqual(TEXT("Two 500 kg push boxes contribute 1000 kg"), Plate->GetCurrentWeight(), 1000.0f);
    TestTrue(TEXT("Two 500 kg push boxes meet 1000 kg requirement"), Plate->GetPresentationState().bTriggered);
    TestFalse(TEXT("Pressure plate signal toggles laser off through puzzle controller"), Laser->IsElementActive());
    PushBox->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f));
    SecondPushBox->SetActorLocation(FVector(1000.0f, 200.0f, 0.0f));
    Plate->RefreshOverlaps();
    Plate->RequiredWeight = 100.0f;
    Plate->ReleaseWeight = 90.0f;
    AActor* WeightActor = World->SpawnActor<AActor>();
    UCMMechanismWeightComponent* Weight = NewObject<UCMMechanismWeightComponent>(WeightActor);
    WeightActor->AddInstanceComponent(Weight);
    Weight->RegisterComponent();
    Plate->OverlapCounts.Add(WeightActor, 2); // Multiple colliders contribute once.
    auto SetWeight = [&](float Value)
    {
        Weight->MechanismWeight = Value;
        Plate->RecalculatePressure(WeightActor);
        return Plate->GetPresentationState();
    };
    FCMTriggerPresentationState State = SetWeight(100.0f);
    TestTrue(TEXT("Weight supported"), State.bSupportsWeight);
    TestEqual(TEXT("Current weight"), State.CurrentWeight, 100.0f);
    TestEqual(TEXT("Required weight"), State.RequiredWeight, 100.0f);
    TestEqual(TEXT("Release weight"), State.ReleaseWeight, 90.0f);
    TestTrue(TEXT("Threshold press"), State.bTriggered);
    TestTrue(TEXT("95 stays pressed"), SetWeight(95.0f).bTriggered);
    TestTrue(TEXT("90 stays pressed"), SetWeight(90.0f).bTriggered);
    TestFalse(TEXT("89 releases"), SetWeight(89.0f).bTriggered);
    Plate->ResetElement();
    TestEqual(TEXT("Reset weight snapshot"), Plate->GetPresentationState().CurrentWeight, 0.0f);
    TestFalse(TEXT("Reset pressed snapshot"), Plate->GetPresentationState().bTriggered);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

#endif
