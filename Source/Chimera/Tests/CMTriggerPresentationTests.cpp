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
#include "Stage/Trigger/CMBasicButtonBase.h"
#include "TimerManager.h"
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
    LaserCommand.Targets.Add(Laser); // Duplicate target entries must not cancel a Toggle.
    FCMPuzzleStep PressureStep;
    PressureStep.Commands.Add(LaserCommand);
    FCMPuzzleChannel PressureChannel;
    PressureChannel.ChannelId = TEXT("Pressure");
    PressureChannel.EndBehavior = ECMPuzzleStepEndBehavior::Loop;
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
    TestTrue(TEXT("OFF transition toggles the laser back on"), Laser->IsElementActive());
    PuzzleController->HandleTriggerSignal(Plate, ECMStageTriggerSignal::Deactivated);
    TestTrue(TEXT("Duplicate OFF signal does not toggle twice"), Laser->IsElementActive());
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

    // Option semantics remain explicit; OFF is not silently accepted by ON-only channels.
    FCMPuzzleChannel Filter;
    TestTrue(TEXT("Default accepts ON"), PuzzleController->IsSignalAccepted(Filter, ECMStageTriggerSignal::Activated));
    TestTrue(TEXT("Default accepts OFF"), PuzzleController->IsSignalAccepted(Filter, ECMStageTriggerSignal::Deactivated));
    TestFalse(TEXT("State Changed rejects Pulse"), PuzzleController->IsSignalAccepted(Filter, ECMStageTriggerSignal::Pulse));
    Filter.AcceptedSignal = ECMPuzzleAcceptedSignal::ActivatedOnly;
    TestFalse(TEXT("ON Only rejects OFF"), PuzzleController->IsSignalAccepted(Filter, ECMStageTriggerSignal::Deactivated));
    Filter.AcceptedSignal = ECMPuzzleAcceptedSignal::DeactivatedOnly;
    TestFalse(TEXT("OFF Only rejects ON"), PuzzleController->IsSignalAccepted(Filter, ECMStageTriggerSignal::Activated));
    Filter.AcceptedSignal = ECMPuzzleAcceptedSignal::PulseOrActivated;
    TestFalse(TEXT("Legacy filter still rejects OFF"), PuzzleController->IsSignalAccepted(Filter, ECMStageTriggerSignal::Deactivated));
    TestFalse(TEXT("Legacy ON filter no longer accepts Pulse"), PuzzleController->IsSignalAccepted(Filter, ECMStageTriggerSignal::Pulse));
    Filter.AcceptedSignal = ECMPuzzleAcceptedSignal::Any;
    TestFalse(TEXT("Legacy Any no longer accepts Pulse"), PuzzleController->IsSignalAccepted(Filter, ECMStageTriggerSignal::Pulse));
    TestTrue(TEXT("Legacy Any still accepts OFF"), PuzzleController->IsSignalAccepted(Filter, ECMStageTriggerSignal::Deactivated));
    PuzzleController->PuzzleChannels[0].AcceptedSignal = ECMPuzzleAcceptedSignal::PulseOrActivated;
    PuzzleController->NormalizeAcceptedSignals();
    TestEqual(TEXT("Legacy PulseOrActivated migrates to ON Only"), PuzzleController->PuzzleChannels[0].AcceptedSignal, ECMPuzzleAcceptedSignal::ActivatedOnly);
    PuzzleController->PuzzleChannels[0].AcceptedSignal = ECMPuzzleAcceptedSignal::Any;
    PuzzleController->NormalizeAcceptedSignals();
    TestEqual(TEXT("Legacy Any migrates to ON/OFF"), PuzzleController->PuzzleChannels[0].AcceptedSignal, ECMPuzzleAcceptedSignal::StateChanged);
    const UEnum* SignalEnum = StaticEnum<ECMPuzzleAcceptedSignal>();
    TestTrue(TEXT("Legacy Pulse option is hidden"), SignalEnum->HasMetaData(TEXT("Hidden"), SignalEnum->GetIndexByValue(0)));
    TestTrue(TEXT("Legacy Any option is hidden"), SignalEnum->HasMetaData(TEXT("Hidden"), SignalEnum->GetIndexByValue(3)));

    ACMBasicButtonBase* BasicButton = World->SpawnActor<ACMBasicButtonBase>();
    BasicButton->FindComponentByClass<UCMStageElementComponent>()->PlacementId = TEXT("Test.Basic.State");
    BasicButton->SetDirectTargetCommandEnabled(false);
    BasicButton->DispatchBeginPlay();
    PuzzleController->PuzzleChannels[0].Triggers.Add(BasicButton);
    BasicButton->OnTriggerSignal.AddUniqueDynamic(PuzzleController, &ACMStagePuzzleController::HandleTriggerSignal);
    PuzzleController->ResetPuzzle();
    Laser->ActivateElement();
    BasicButton->HandleValidButtonInput(Button);
    TestTrue(TEXT("Momentary button is logically ON while lit"), BasicButton->GetPresentationState().bTriggered);
    TestFalse(TEXT("Momentary ON toggles laser off"), Laser->IsElementActive());
    // Exercise the actual timer scheduled by valid button input.
    World->GetTimerManager().Tick(1.0f);
    ++GFrameCounter; // TimerManager only ticks once per engine frame.
    World->GetTimerManager().Tick(1.0f);
    TestFalse(TEXT("Momentary auto-return is logically OFF"), BasicButton->GetPresentationState().bTriggered);
    TestTrue(TEXT("Momentary OFF toggles laser on"), Laser->IsElementActive());
    TestFalse(TEXT("OneShot remains used after auto-return"), BasicButton->GetPresentationState().bCanActivate);

    BasicButton->ResetElement();
    BasicButton->bToggleOnHit = true;
    BasicButton->FindComponentByClass<UCMActivationTriggerComponent>()->bOneShot = false;
    PuzzleController->ResetPuzzle();
    Laser->ActivateElement();
    BasicButton->HandleValidButtonInput(Button);
    TestFalse(TEXT("Toggle button ON changes target"), Laser->IsElementActive());
    PuzzleController->HandleTriggerSignal(BasicButton, ECMStageTriggerSignal::Activated);
    TestFalse(TEXT("Duplicate ON is ignored"), Laser->IsElementActive());
    BasicButton->HandleValidButtonInput(Button);
    TestTrue(TEXT("Toggle button OFF changes target"), Laser->IsElementActive());
    BasicButton->HandleValidButtonInput(Button);
    TestFalse(TEXT("Next ON is still accepted after OFF"), Laser->IsElementActive());

    // Stop is intentionally one-shot even when both state edges are accepted.
    PuzzleController->PuzzleChannels[0].EndBehavior = ECMPuzzleStepEndBehavior::Stop;
    PuzzleController->ResetPuzzle();
    Laser->ActivateElement();
    BasicButton->ReleaseButton(Button);
    TestFalse(TEXT("Stop executes the first allowed edge"), Laser->IsElementActive());
    BasicButton->PressButton(Button);
    TestFalse(TEXT("Stop does not execute later edges"), Laser->IsElementActive());

    Button->FindComponentByClass<UCMActivationTriggerComponent>()->bOneShot = false;
    Button->OnTriggerSignal.AddUniqueDynamic(PuzzleController, &ACMStagePuzzleController::HandleTriggerSignal);
    FCMPuzzleChannel& Channel = PuzzleController->PuzzleChannels[0];
    Channel.Triggers = { Button, BasicButton };
    Channel.TriggerCondition = ECMPuzzleTriggerCondition::All;
    Channel.AllConditionMode = ECMPuzzleAllConditionMode::Simultaneous;
    Channel.SimultaneousMatchState = ECMPuzzleSimultaneousMatchState::AllActive;
    Channel.AcceptedSignal = ECMPuzzleAcceptedSignal::StateChanged;
    Channel.EndBehavior = ECMPuzzleStepEndBehavior::Loop;
    Button->ResetElement();
    BasicButton->ResetElement();
    // First switch is already ON when the controller starts observing this condition.
    Button->PressButton(Button);
    PuzzleController->ResetPuzzle();
    Laser->ActivateElement();
    BasicButton->PressButton(Button);
    TestFalse(TEXT("Simultaneous includes switches already ON before reset"), Laser->IsElementActive());
    BasicButton->ReleaseButton(Button);
    TestFalse(TEXT("Losing AllActive does not implicitly run inverse commands"), Laser->IsElementActive());
    BasicButton->PressButton(Button);
    TestTrue(TEXT("AllActive can be satisfied again in Loop"), Laser->IsElementActive());

    Channel.SimultaneousMatchState = ECMPuzzleSimultaneousMatchState::AllInactive;
    PuzzleController->ResetPuzzle();
    Laser->ActivateElement();
    Button->ReleaseButton(Button);
    TestTrue(TEXT("AllInactive waits for both switches"), Laser->IsElementActive());
    BasicButton->ReleaseButton(Button);
    TestFalse(TEXT("AllInactive accepts OFF transition"), Laser->IsElementActive());

    Channel.AllConditionMode = ECMPuzzleAllConditionMode::Latched;
    Channel.AcceptedSignal = ECMPuzzleAcceptedSignal::ActivatedOnly;
    PuzzleController->ResetPuzzle();
    Laser->ActivateElement();
    Button->PressButton(Button);
    Button->ReleaseButton(Button);
    TestTrue(TEXT("Latched waits for the other switch"), Laser->IsElementActive());
    BasicButton->PressButton(Button);
    TestFalse(TEXT("Latched retains earlier ON after its OFF"), Laser->IsElementActive());

    Channel.TriggerCondition = ECMPuzzleTriggerCondition::Sequence;
    Channel.ExpectedTriggerSequence = { Button, BasicButton };
    Channel.WrongInputBehavior = ECMPuzzleSequenceWrongInputBehavior::ResetSequence;
    Button->ResetElement();
    BasicButton->ResetElement();
    PuzzleController->ResetPuzzle();
    Laser->ActivateElement();
    BasicButton->PressButton(Button);
    TestTrue(TEXT("Wrong sequence does not execute"), Laser->IsElementActive());
    BasicButton->ReleaseButton(Button);
    Button->PressButton(Button);
    BasicButton->PressButton(Button);
    TestFalse(TEXT("Correct ON sequence executes"), Laser->IsElementActive());

    Channel.TriggerCondition = ECMPuzzleTriggerCondition::Any;
    Channel.AcceptedSignal = ECMPuzzleAcceptedSignal::StateChanged;
    Channel.EndBehavior = ECMPuzzleStepEndBehavior::RepeatCurrent;
    Channel.Steps.Add(PressureStep);
    PuzzleController->ResetPuzzle();
    Button->ReleaseButton(Button);
    TestEqual(TEXT("RepeatCurrent advances to last step first"), PuzzleController->GetCurrentStepIndex(Channel.ChannelId), 1);
    Button->PressButton(Button);
    TestEqual(TEXT("RepeatCurrent remains at the last step"), PuzzleController->GetCurrentStepIndex(Channel.ChannelId), 1);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

#endif
