#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Parts/Arm/CMArmPart.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"
#include "Stage/CMStageDirector.h"
#include "Stage/CMStageElementComponent.h"
#include "Stage/Trigger/CMLeverBase.h"
#include "Stage/Trigger/Component/CMActivationTriggerComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMLeverInteractionRegressionTest,
    "Chimera.Lever.InteractionRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMLeverInteractionRegressionTest::RunTest(const FString& Parameters)
{
    // This isolated world does not run a GameMode startup; allow reflected events explicitly.
    TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
    const UWorld::InitializationValues InitValues = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
        nullptr, true, ERHIFeatureLevel::Num, &InitValues);
    if (!TestNotNull(TEXT("Test world"), World)) return false;

    World->SpawnActor<ACMStageDirector>();
    ACMLeverBase* Lever = World->SpawnActor<ACMLeverBase>();
    AActor* Puller = World->SpawnActor<AActor>();
    Lever->FindComponentByClass<UCMStageElementComponent>()->PlacementId = TEXT("Test.Lever");
    Lever->SetDirectTargetCommandEnabled(false);
    Lever->DispatchBeginPlay();

    TestTrue(TEXT("External press accepted"), Lever->PressButton(Puller));
    TestEqual(TEXT("External press updates target"), Lever->LeverAlpha, 1.0f);
    TestTrue(TEXT("External release accepted"), Lever->ReleaseButton(Puller));
    TestEqual(TEXT("External release updates target"), Lever->LeverAlpha, -1.0f);

    Lever->LeverAlpha = 0.85f;
    {
        TGuardValue<bool> Guard(Lever->bUpdatingFromHold, true);
        TestTrue(TEXT("Hold threshold press"), Lever->SetLeverPressed(true, Puller));
    }
    TestEqual(TEXT("Hold keeps continuous angle"), Lever->LeverAlpha, 0.85f);
    Lever->ReleaseButton(Puller);

    Lever->bRequiresHoldToStayActivated = false;
    Lever->PressButton(Puller);
    Lever->StopArmHold();
    TestTrue(TEXT("Default lever stays active after release"),
        Lever->FindComponentByClass<UCMActivationTriggerComponent>()->IsTriggered());
    TestEqual(TEXT("Default lever stays at active pose"), Lever->LeverAlpha, 1.0f);
    Lever->ReleaseButton(Puller);

    Lever->bRequiresHoldToStayActivated = true;
    {
        TGuardValue<bool> Guard(Lever->bUpdatingFromHold, true);
        TestTrue(TEXT("Hold-required lever activates while held"),
            Lever->SetLeverPressed(true, Puller));
    }
    TestTrue(TEXT("Hold-required lever remains active during hold"),
        Lever->FindComponentByClass<UCMActivationTriggerComponent>()->IsTriggered());
    Lever->StopArmHold();
    TestFalse(TEXT("Hold-required lever deactivates after release"),
        Lever->FindComponentByClass<UCMActivationTriggerComponent>()->IsTriggered());
    TestEqual(TEXT("Hold-required lever returns to default pose"), Lever->LeverAlpha, -1.0f);

    const FVector PositiveOrigin = Lever->GetActorLocation() + FVector(200, 0, 0);
    const FVector NegativeOrigin = Lever->GetActorLocation() - FVector(200, 0, 0);
    auto Pull = [&](FVector Origin, float Strength = 100.0f)
    {
        return ICMGrabPullTarget::Execute_HandlePullWithResult(Lever, Puller, Origin, Strength);
    };
    TestEqual(TEXT("Hold-required lever rejects impulse activation"), Pull(PositiveOrigin), ECMGrabPullResult::HandledNoChange);
    Lever->bRequiresHoldToStayActivated = false;
    TestEqual(TEXT("Positive pull applies"), Pull(PositiveOrigin), ECMGrabPullResult::Applied);
    TestEqual(TEXT("Repeated positive consumes without change"), Pull(PositiveOrigin), ECMGrabPullResult::HandledNoChange);
    TestEqual(TEXT("Negative pull applies"), Pull(NegativeOrigin), ECMGrabPullResult::Applied);
    TestEqual(TEXT("Weak pull consumes"), Pull(PositiveOrigin, 0.0f), ECMGrabPullResult::HandledNoChange);
    TestEqual(TEXT("Sideways pull consumes"), Pull(Lever->GetActorLocation() + FVector(0, 200, 0)), ECMGrabPullResult::HandledNoChange);
    Lever->DeactivateElement();
    TestEqual(TEXT("Inactive lever consumes"), Pull(PositiveOrigin), ECMGrabPullResult::HandledNoChange);
    Lever->ActivateElement();
    Lever->ResetElement();
    Lever->VisualLeverAlpha = -1.0f;
    Lever->RotationTransitionDuration = 0.3f;
    Pull(PositiveOrigin);
    TestEqual(TEXT("Logical state changes immediately"), Lever->LeverAlpha, 1.0f);
    Lever->UpdateVisualRotation(0.15f);
    TestTrue(TEXT("Visual rotation is halfway"), FMath::IsNearlyZero(Lever->VisualLeverAlpha));
    Lever->UpdateVisualRotation(0.15f);
    TestEqual(TEXT("Visual rotation finishes"), Lever->VisualLeverAlpha, 1.0f);
    TestFalse(TEXT("Idle tick stops"), Lever->IsActorTickEnabled());

    ACMChimera* Chimera = World->SpawnActor<ACMChimera>();
    ACMArmPart* Arm = World->SpawnActor<ACMArmPart>();
    Lever->MaximumHoldDistance = 200.0f;
    Arm->SetActorLocation(Lever->GetActorLocation() + FVector(0.0f, 0.0f, 1000.0f));
    TestFalse(TEXT("Vertical separation does not release lever hold"),
        Lever->IsHoldDistanceExceeded(*Arm));
    Arm->SetActorLocation(Lever->GetActorLocation() + FVector(1000.0f, 0.0f, 1000.0f));
    TestTrue(TEXT("Distant arm exceeds lever hold distance"),
        Lever->IsHoldDistanceExceeded(*Arm));
    Lever->MaximumHoldDistance = 0.0f;
    TestFalse(TEXT("Zero lever hold distance disables release"),
        Lever->IsHoldDistanceExceeded(*Arm));

    Lever->InteractionMode = ECMLeverInteractionMode::LinearPull;
    Lever->LocalPullAxis = FVector::ForwardVector;
    Lever->FullTravelDistance = 100.0f;
    Lever->GrabStartArmLocation = Lever->GetActorLocation();
    Lever->GrabStartAlpha = -1.0f;
    TestTrue(TEXT("Linear lever ignores vertical arm movement"),
        FMath::IsNearlyEqual(Lever->CalculateLeverAlphaFromArmLocation(
            Lever->GetActorLocation() + FVector(50.0f, 0.0f, 1000.0f)), 0.0f));

    Lever->InteractionMode = ECMLeverInteractionMode::WheelRotation;
    Lever->LocalRotationAxis = FVector::UpVector;
    Lever->RotationHalfAngle = 45.0f;
    Lever->GrabStartAlpha = -1.0f;
    Lever->GrabStartWheelDirection = FVector::ForwardVector;
    const FVector WheelPivot = Lever->LeverPivot->GetComponentLocation();
    TestTrue(TEXT("Wheel lever follows hand angle around its axis"),
        FMath::IsNearlyEqual(Lever->CalculateLeverAlphaFromArmLocation(
            WheelPivot + FVector(0.0f, 100.0f, 0.0f)), 1.0f));
    FCMPartSlotAddress Slot;
    Slot.SegmentIndex = 0;
    Slot.PartSlotIndex = 0;
    UCMPartSlotComponent* PartSlot = Chimera->GetPartSlotComponent(Slot);
    if (TestNotNull(TEXT("Arm slot exists"), PartSlot))
    {
        TestTrue(TEXT("Attach arm"), PartSlot->AttachPart(Arm));
        TestTrue(TEXT("Ordinary release permits attack"), Chimera->ShouldActivateBasicArmOnRelease(Slot));
        // Emulate a successful interaction followed by destruction of its anchor.
        Chimera->PressedPartSlotMask = 1u;
        Chimera->InteractionConsumedPartSlotMask = 1u;
        TestFalse(TEXT("Ended interaction still suppresses release attack"), Chimera->ShouldActivateBasicArmOnRelease(Slot));
        Chimera->SetPartSlotPressed(Slot, true);
        TestFalse(TEXT("Repeated press does not erase history"), Chimera->ShouldActivateBasicArmOnRelease(Slot));
        Chimera->SetPartSlotPressed(Slot, false);
        TestTrue(TEXT("Release clears history for next input"), Chimera->ShouldActivateBasicArmOnRelease(Slot));
        Chimera->InteractionConsumedPartSlotMask = 1u;
        Chimera->ClearPressedControlParts();
        TestEqual(TEXT("Global input reset clears history"), Chimera->InteractionConsumedPartSlotMask, 0u);
    }

    World->DestroyWorld(false);
    return true;
}
#endif
