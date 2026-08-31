#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Stage/CMStageDirector.h"
#include "Stage/CMStageElementComponent.h"
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
    World->DestroyWorld(false);
    return true;
}

#endif
