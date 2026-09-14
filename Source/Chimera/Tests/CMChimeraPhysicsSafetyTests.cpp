#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Player/CMChimera.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMChimeraPhysicsSafetyMathTest,
    "Chimera.Physics.SafetyMath",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMChimeraPhysicsSafetyMathTest::RunTest(
    const FString& Parameters)
{
    TestEqual(TEXT("Extreme data speed is clamped"),
        CMChimeraPhysics::ResolveLinearSpeedLimit(
            1000000.0f, 1200.0f),
        1200.0f);
    TestEqual(TEXT("Normal data speed is preserved"),
        CMChimeraPhysics::ResolveLinearSpeedLimit(
            900.0f, 1200.0f),
        900.0f);
    TestEqual(TEXT("Negative speed resolves to zero"),
        CMChimeraPhysics::ResolveLinearSpeedLimit(
            -100.0f, 1200.0f),
        0.0f);

    TestTrue(TEXT("Three-dimensional velocity uses the safety ceiling"),
        CMChimeraPhysics::ClampLinearVelocity(
            FVector(1200.0f, 0.0f, 1600.0f),
            1000.0f).Size() <= 1000.01f);

    TestTrue(TEXT("Opposing wall contact stops planar knockback"),
        CMChimeraPhysics::IsBlockingPlanarContact(
            FVector::ForwardVector,
            -FVector::ForwardVector));
    TestFalse(TEXT("Floor contact does not stop planar knockback"),
        CMChimeraPhysics::IsBlockingPlanarContact(
            FVector::ForwardVector,
            FVector::UpVector));
    TestFalse(TEXT("Glancing side contact does not stop planar knockback"),
        CMChimeraPhysics::IsBlockingPlanarContact(
            FVector::ForwardVector,
            FVector::RightVector));

    const ACMChimera* ChimeraDefaults = GetDefault<ACMChimera>();
    const UBoxComponent* FirstBody = ChimeraDefaults
        ? ChimeraDefaults->GetBodySegmentComponent(0)
        : nullptr;
    TestNotNull(TEXT("Default Chimera owns its first physics body"),
        FirstBody);
    TestTrue(TEXT("Default Chimera physics body has CCD enabled"),
        FirstBody && FirstBody->BodyInstance.bUseCCD);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMChimeraPhysicsRecoveryTest,
    "Chimera.Physics.StuckRecovery",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMChimeraPhysicsRecoveryTest::RunTest(
    const FString& Parameters)
{
    const UWorld::InitializationValues InitValues =
        UWorld::InitializationValues()
            .AllowAudioPlayback(false)
            .CreatePhysicsScene(true)
            .CreateNavigation(false)
            .CreateAISystem(false)
            .ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Game,
        false,
        NAME_None,
        nullptr,
        true,
        ERHIFeatureLevel::Num,
        &InitValues);
    if (!TestNotNull(TEXT("Recovery test world"), World))
    {
        return false;
    }

    ACMChimera* Chimera = World->SpawnActor<ACMChimera>(
        FVector(0.0f, 0.0f, 100.0f),
        FRotator::ZeroRotator);
    TestNotNull(TEXT("Recovery test Chimera"), Chimera);
    if (!Chimera)
    {
        World->DestroyWorld(false);
        return false;
    }

    Chimera->ActiveSegmentCount = 1;
    Chimera->ConfigureSegments();
    UBoxComponent* Body = Chimera->GetBodySegmentComponent(0);
    TestNotNull(TEXT("Recovery test body"), Body);
    TestTrue(TEXT("Runtime body has CCD enabled"),
        Body && Body->BodyInstance.bUseCCD);
    if (!Body)
    {
        World->DestroyWorld(false);
        return false;
    }

    Chimera->SaveSafeAssemblySnapshot();
    TestEqual(TEXT("One safe assembly state is recorded"),
        Chimera->SafeAssemblySnapshots.Num(),
        1);

    AActor* Blocker = World->SpawnActor<AActor>(
        FVector(300.0f, 0.0f, 100.0f),
        FRotator::ZeroRotator);
    UBoxComponent* BlockerCollision = Blocker
        ? NewObject<UBoxComponent>(Blocker, TEXT("RecoveryBlocker"))
        : nullptr;
    TestNotNull(TEXT("Recovery blocker"), BlockerCollision);
    if (Blocker && BlockerCollision)
    {
        Blocker->SetRootComponent(BlockerCollision);
        Blocker->AddInstanceComponent(BlockerCollision);
        BlockerCollision->SetBoxExtent(FVector(40.0f));
        BlockerCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
        BlockerCollision->RegisterComponent();
        Blocker->SetActorLocation(FVector(300.0f, 0.0f, 100.0f));

        Body->SetWorldLocation(
            FVector(300.0f, 0.0f, 100.0f),
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        Body->SetPhysicsLinearVelocity(FVector(500.0f, 0.0f, 0.0f));
        Chimera->UpdateStuckRecovery(
            Chimera->StuckRecoveryDetectionTime + 0.01f);

        TestTrue(TEXT("Persistent penetration restores the last safe pose"),
            Body->GetComponentLocation().Equals(
                FVector(0.0f, 0.0f, 100.0f),
                0.1f));
        TestTrue(TEXT("Recovery removes dangerous residual velocity"),
            Body->GetPhysicsLinearVelocity().IsNearlyZero());
        TestTrue(TEXT("Recovery starts its force suppression cooldown"),
            Chimera->StuckRecoveryCooldownRemaining > 0.0f);
    }

    World->DestroyWorld(false);
    return true;
}

#endif
