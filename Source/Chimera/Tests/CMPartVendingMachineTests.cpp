#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Stage/Device/CMPartVendingMachine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMPartVendingMachineTest,
    "Chimera.Device.PartVendingMachine.Dispense",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMPartVendingMachineTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> ScriptExecutionGuard(
        GAllowActorScriptExecutionInEditor, true);
    const UWorld::InitializationValues InitValues =
        UWorld::InitializationValues()
        .AllowAudioPlayback(false)
        .CreatePhysicsScene(true)
        .CreateNavigation(false)
        .CreateAISystem(false)
        .ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
        nullptr, true, ERHIFeatureLevel::Num, &InitValues);
    if (!TestNotNull(TEXT("World"), World))
    {
        return false;
    }

    ACMPartVendingMachine* Machine =
        World->SpawnActor<ACMPartVendingMachine>();
    ACMArmPart* Arm = World->SpawnActor<ACMArmPart>();
    TestNotNull(TEXT("Machine"), Machine);
    TestNotNull(TEXT("Arm"), Arm);
    if (!Machine || !Arm)
    {
        World->DestroyWorld(false);
        return false;
    }
    Machine->PartClass = ACMArmPart::StaticClass();

    FCMCombatHitRequest Request;
    Request.SourcePart = Arm;
    Request.AttackId = FGuid::NewGuid();

    TestTrue(TEXT("Valid arm hit dispenses a Part"),
        ICMCombatHitTarget::Execute_ReceiveCombatHit(Machine, Request));

    TArray<ACMPartActorBase*> SpawnedParts;
    for (TActorIterator<ACMPartActorBase> It(World); It; ++It)
    {
        if (*It != Arm)
        {
            SpawnedParts.Add(*It);
        }
    }
    TestEqual(TEXT("One Part is spawned"), SpawnedParts.Num(), 1);

    TestTrue(TEXT("Repeated detection of the same swing is accepted"),
        ICMCombatHitTarget::Execute_ReceiveCombatHit(Machine, Request));
    SpawnedParts.Reset();
    for (TActorIterator<ACMPartActorBase> It(World); It; ++It)
    {
        if (*It != Arm)
        {
            SpawnedParts.Add(*It);
        }
    }
    TestEqual(TEXT("The same swing does not dispense twice"),
        SpawnedParts.Num(), 1);

    FCMCombatHitRequest InvalidRequest;
    InvalidRequest.SourcePart = World->SpawnActor<AActor>();
    InvalidRequest.AttackId = FGuid::NewGuid();
    TestFalse(TEXT("A non-arm source cannot dispense a Part"),
        ICMCombatHitTarget::Execute_ReceiveCombatHit(
            Machine, InvalidRequest));

    World->DestroyWorld(false);
    return true;
}

#endif
