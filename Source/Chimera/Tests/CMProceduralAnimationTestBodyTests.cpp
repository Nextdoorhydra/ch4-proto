#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/CMProceduralAnimationTestBody.h"

#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Parts/Leg/CMLegPart.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMProceduralAnimationTestBodyDefaultsTest,
    "Chimera.Animation.ProceduralTestBody.Defaults",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMProceduralAnimationTestBodyDefaultsTest::RunTest(
    const FString& Parameters
)
{
    UClass* TestBodyClass = LoadClass<ACMProceduralAnimationTestBody>(
        nullptr,
        TEXT("/Game/Chimera/Character/Test/"
            "BP_CMProceduralAnimationTestBody."
            "BP_CMProceduralAnimationTestBody_C")
    );
    TestNotNull(TEXT("Test body Blueprint class loads"), TestBodyClass);
    const ACMProceduralAnimationTestBody* TestBody = TestBodyClass
        ? Cast<ACMProceduralAnimationTestBody>(
            TestBodyClass->GetDefaultObject())
        : nullptr;
    TestNotNull(TEXT("Test body Blueprint CDO exists"), TestBody);
    if (!TestBody)
    {
        return false;
    }

    const UBoxComponent* PhysicsBody = TestBody->GetPhysicsBody();
    const USceneComponent* VisualRoot = TestBody->GetVisualRoot();
    const UCameraComponent* TopViewCamera = TestBody->GetTopViewCamera();
    const UCMPartSlotComponent* LeftSlot = TestBody->GetLeftPartSlot();
    const UCMPartSlotComponent* RightSlot = TestBody->GetRightPartSlot();
    const UChildActorComponent* LeftPreview =
        TestBody->GetLeftPartPreview();
    const UChildActorComponent* RightPreview =
        TestBody->GetRightPartPreview();

    TestNotNull(TEXT("Physics proxy exists"), PhysicsBody);
    TestNotNull(TEXT("Blueprint visual attachment root exists"), VisualRoot);
    TestNotNull(TEXT("Top-view observation camera exists"), TopViewCamera);
    TestNotNull(TEXT("Left slot exists"), LeftSlot);
    TestNotNull(TEXT("Right slot exists"), RightSlot);
    TestNotNull(TEXT("Left construction Part preview exists"), LeftPreview);
    TestNotNull(TEXT("Right construction Part preview exists"), RightPreview);
    if (!PhysicsBody || !VisualRoot
        || !LeftSlot || !RightSlot || !LeftPreview || !RightPreview)
    {
        return false;
    }

    TestFalse(TEXT("Physics simulation is deferred until BeginPlay"),
        PhysicsBody->GetBodyInstance()->bSimulatePhysics);
    TestTrue(TEXT("Body gravity is enabled before simulation starts"),
        PhysicsBody->IsGravityEnabled());
    TestEqual(TEXT("Eight independent body segments are assembled"),
        TestBody->GetBodySegmentCount(),
        8);
    TestEqual(TEXT("Each body segment has two part slots"),
        TestBody->GetPartSlotCount(),
        16);
    for (int32 SlotIndex = 0;
        SlotIndex < TestBody->GetPartSlotCount();
        ++SlotIndex)
    {
        const USceneComponent* RigAnchor =
            TestBody->GetLegRigControlAnchor(SlotIndex);
        TestNotNull(
            *FString::Printf(
                TEXT("Leg rig anchor %d exists"),
                SlotIndex),
            RigAnchor);
        if (RigAnchor)
        {
            TestTrue(
                *FString::Printf(
                    TEXT("Leg rig anchor %d follows its part slot"),
                    SlotIndex),
                RigAnchor->GetAttachParent()
                    == TestBody->GetPartSlot(SlotIndex));
        }
    }

    const ACMProceduralAnimationTestBody* NativeTestBody =
        GetDefault<ACMProceduralAnimationTestBody>();
    TInlineComponentArray<USkeletalMeshComponent*> NativeVisuals(
        NativeTestBody);
    TestEqual(TEXT("Native test body forces no skeletal visuals"),
        NativeVisuals.Num(),
        0);

    TestEqual(TEXT("Each segment collision uses the configured half length"),
        PhysicsBody->GetUnscaledBoxExtent().X,
        50.0);

    TestEqual(TEXT("Left slot segment"),
        LeftSlot->GetSlotAddress().SegmentIndex, 0);
    TestEqual(TEXT("Left slot index"),
        LeftSlot->GetSlotAddress().PartSlotIndex, 0);
    TestEqual(TEXT("Right slot segment"),
        RightSlot->GetSlotAddress().SegmentIndex, 0);
    TestEqual(TEXT("Right slot index"),
        RightSlot->GetSlotAddress().PartSlotIndex, 1);
    TestEqual(TEXT("Slot locations are mirrored"),
        LeftSlot->GetRelativeLocation().Y,
        -RightSlot->GetRelativeLocation().Y);
    TestTrue(TEXT("Left Part preview follows left slot"),
        LeftPreview->GetAttachParent() == LeftSlot);
    TestTrue(TEXT("Right Part preview follows right slot"),
        RightPreview->GetAttachParent() == RightSlot);
    TestTrue(TEXT("Left preview uses configured Part Blueprint"),
        LeftPreview->GetChildActorClass() != nullptr);
    TestTrue(TEXT("Right preview uses configured Part Blueprint"),
        RightPreview->GetChildActorClass() != nullptr);
    TestEqual(TEXT("Right construction preview is mirrored"),
        RightPreview->GetRelativeScale3D().Y,
        -1.0);

    TestTrue(TEXT("Head is supported"),
        ACMProceduralAnimationTestBody::IsSupportedPartType(
            ECMPartSlotType::Head));
    TestTrue(TEXT("Arm is supported"),
        ACMProceduralAnimationTestBody::IsSupportedPartType(
            ECMPartSlotType::Arm));
    TestTrue(TEXT("Leg is supported"),
        ACMProceduralAnimationTestBody::IsSupportedPartType(
            ECMPartSlotType::Leg));
    TestFalse(TEXT("Organ is rejected"),
        ACMProceduralAnimationTestBody::IsSupportedPartType(
            ECMPartSlotType::Organ));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMProceduralAnimationTestBodySpawnTest,
    "Chimera.Animation.ProceduralTestBody.SpawnParts",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMProceduralAnimationTestBodySpawnTest::RunTest(
    const FString& Parameters
)
{
    AddExpectedError(
        TEXT("Head Definition failed."),
        EAutomationExpectedErrorFlags::Contains,
        1);
    UClass* TestBodyClass = LoadClass<ACMProceduralAnimationTestBody>(
        nullptr,
        TEXT("/Game/Chimera/Character/Test/"
            "BP_CMProceduralAnimationTestBody."
            "BP_CMProceduralAnimationTestBody_C")
    );
    TestNotNull(TEXT("Test body Blueprint class loads"), TestBodyClass);
    if (!TestBodyClass || !GEngine)
    {
        return false;
    }

    const FName WorldName = MakeUniqueObjectName(
        nullptr,
        UWorld::StaticClass(),
        TEXT("ProceduralAnimationTestWorld"),
        EUniqueObjectNameOptions::GloballyUnique
    );
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(
        EWorldType::Game
    );
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Game,
        false,
        WorldName,
        GetTransientPackage()
    );
    TestNotNull(TEXT("Transient test world exists"), World);
    if (!World)
    {
        return false;
    }

    WorldContext.SetCurrentWorld(World);

    AActor* Ground = World->SpawnActor<AActor>();
    UBoxComponent* GroundCollision = Ground
        ? NewObject<UBoxComponent>(Ground, TEXT("GroundCollision"))
        : nullptr;
    TestNotNull(TEXT("Ground test actor spawns"), Ground);
    TestNotNull(TEXT("Ground collision exists"), GroundCollision);
    if (Ground && GroundCollision)
    {
        Ground->SetRootComponent(GroundCollision);
        Ground->AddInstanceComponent(GroundCollision);
        GroundCollision->SetBoxExtent(FVector(2000.0, 1000.0, 10.0));
        GroundCollision->SetCollisionProfileName(TEXT("BlockAll"));
        GroundCollision->RegisterComponent();
        Ground->SetActorLocation(FVector(0.0, 0.0, -10.0));
    }

    ACMProceduralAnimationTestBody* TestBody = World->SpawnActor<
        ACMProceduralAnimationTestBody>(TestBodyClass, FTransform::Identity);
    TestNotNull(TEXT("Test body spawns"), TestBody);
    if (TestBody)
    {
        // Exercise the diagnostic path that deliberately leans one side so a
        // planted foot outside its reach window must enter visual replant.
        TestBody->SetOneSidedLeanTestMode(true);
    }
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    if (TestBody && !TestBody->HasActorBegunPlay())
    {
        // Transient automation worlds do not always route BeginPlay to actors
        // spawned after InitializeActorsForPlay.
        TestBody->DispatchBeginPlay();
    }
    if (TestBody)
    {
        TestEqual(TEXT("Spawned body keeps eight physical segments"),
            TestBody->GetBodySegmentCount(),
            8);
        TestEqual(TEXT("Spawned body keeps two slots per segment"),
            TestBody->GetPartSlotCount(),
            16);
        TestEqual(TEXT("One of four Arm slots is replaced by a Head"),
            TestBody->GetSimulatedOperationalArmCount(),
            3);
        UCMPartSlotComponent* HeadSlot = TestBody->GetPartSlot(11);
        TestNotNull(TEXT("Head test slot exists"), HeadSlot);
        TestNotNull(TEXT("Head test Part occupies segment 5 right slot"),
            HeadSlot
                ? Cast<ACMHeadPartActor>(HeadSlot->GetAttachedPart())
                : nullptr);

        TInlineComponentArray<USkeletalMeshComponent*> BlueprintVisuals(
            TestBody);
        TestEqual(TEXT("Blueprint owns eight editable skeletal visuals"),
            BlueprintVisuals.Num(),
            8);
        USkeletalMeshComponent* LastBodyVisual = nullptr;
        for (USkeletalMeshComponent* Visual : BlueprintVisuals)
        {
            TestEqual(*FString::Printf(
                TEXT("%s is owned by the Blueprint construction script"),
                *GetNameSafe(Visual)),
                Visual->CreationMethod,
                EComponentCreationMethod::SimpleConstructionScript);
            TestNotNull(*FString::Printf(
                TEXT("%s has an editor-assigned skeletal mesh"),
                *GetNameSafe(Visual)),
                Visual->GetSkeletalMeshAsset());
            const FString VisualName = Visual->GetName();
            const int32 VisualIndex = FCString::Atoi(
                *VisualName.RightChop(FString(TEXT("BodyVisual_")).Len())) - 1;
            TestTrue(*FString::Printf(
                TEXT("%s is attached to its matching body collision"),
                *VisualName),
                TestBody->GetBodySegment(VisualIndex)
                    && Visual->GetAttachParent()
                        == TestBody->GetBodySegment(VisualIndex));
            if (VisualName == TEXT("BodyVisual_08"))
            {
                LastBodyVisual = Visual;
            }
        }
        TestNotNull(TEXT("Last Blueprint visual exists"), LastBodyVisual);

        for (int32 Index = 0; Index < TestBody->GetBodySegmentCount(); ++Index)
        {
            const UBoxComponent* SegmentBody =
                TestBody->GetBodySegment(Index);
            TestNotNull(*FString::Printf(
                TEXT("Spawned body segment %d exists"), Index), SegmentBody);
            if (!SegmentBody)
            {
                continue;
            }
            TestEqual(*FString::Printf(
                TEXT("Body segment %d is connected at its chain X"), Index),
                SegmentBody->GetRelativeLocation().X,
                -static_cast<double>(Index) * 120.0);
            TestEqual(*FString::Printf(
                TEXT("Body segment %d uses the configured collision height"),
                Index),
                SegmentBody->GetUnscaledBoxExtent().Z,
                35.0);
            TestTrue(*FString::Printf(
                TEXT("Body segment %d has gravity enabled"), Index),
                SegmentBody->IsGravityEnabled());
            TestFalse(*FString::Printf(
                TEXT("Body segment %d keeps terrain pitch unlocked"), Index),
                SegmentBody->GetBodyInstance()->bLockYRotation);
        }

        const UBoxComponent* PhysicsBody = TestBody->GetPhysicsBody();
        TestNotNull(TEXT("Runtime physics proxy exists"), PhysicsBody);
        if (PhysicsBody)
        {
            TestTrue(TEXT("Authority enables physics after ground correction"),
                PhysicsBody->IsSimulatingPhysics());
            const float BodyBottom = PhysicsBody->GetComponentLocation().Z
                - PhysicsBody->GetScaledBoxExtent().Z;
            TestTrue(TEXT("Physics proxy starts at or above the ground"),
                BodyBottom >= -KINDA_SMALL_NUMBER);
        }
        TestEqual(TEXT("Seven constraints join eight body segments"),
            TestBody->GetSegmentConstraintCount(),
            7);
        for (int32 ConstraintIndex = 0;
            ConstraintIndex < TestBody->GetSegmentConstraintCount();
            ++ConstraintIndex)
        {
            const UPhysicsConstraintComponent* SegmentConstraint =
                TestBody->GetSegmentConstraint(ConstraintIndex);
            TestNotNull(*FString::Printf(
                TEXT("Segment constraint %d exists"), ConstraintIndex),
                SegmentConstraint);
            if (!SegmentConstraint)
            {
                continue;
            }
            TestEqual(*FString::Printf(
                TEXT("Segment constraint %d allows limited terrain pitch"),
                ConstraintIndex),
                SegmentConstraint->ConstraintInstance
                    .GetAngularSwing2Motion(),
                EAngularConstraintMotion::ACM_Limited);
            TestTrue(*FString::Printf(
                TEXT("Segment constraint %d has a non-zero pitch range"),
                ConstraintIndex),
                SegmentConstraint->ConstraintInstance
                    .GetAngularSwing2Limit() > 0.0f);
        }

        TestNull(TEXT("Left construction preview is removed for play"),
            TestBody->GetLeftPartPreview()->GetChildActor());
        TestNull(TEXT("Right construction preview is removed for play"),
            TestBody->GetRightPartPreview()->GetChildActor());

        ACMPartActorBase* LeftPart = TestBody->GetLeftPart();
        ACMPartActorBase* RightPart = TestBody->GetRightPart();
        TestNotNull(TEXT("Left configured Part spawns"), LeftPart);
        TestNotNull(TEXT("Right configured Part spawns"), RightPart);
        if (LeftPart && RightPart)
        {
            const FVector InitialBodyLocation =
                TestBody->GetPhysicsBody()->GetComponentLocation();
            TestEqual(TEXT("Left Part attaches to body"),
                LeftPart->GetAttachParentActor(),
                static_cast<AActor*>(TestBody));
            TestEqual(TEXT("Right Part attaches to body"),
                RightPart->GetAttachParentActor(),
                static_cast<AActor*>(TestBody));
            TestEqual(TEXT("Right Part is mirrored"),
                RightPart->GetActorRelativeScale3D().Y,
                -1.0);
            TestFalse(TEXT("Left visual Part collision is disabled"),
                LeftPart->GetActorEnableCollision());
            TestFalse(TEXT("Right visual Part collision is disabled"),
                RightPart->GetActorEnableCollision());

            float MaxForwardDelta = 0.0f;
            float MaxHeightRise = 0.0f;
            float MaxVerticalSpeed = 0.0f;
            float MaxRollAngularSpeed = 0.0f;
            // A transient automation world can advance physics without
            // registering actors that were spawned just before play. Invoke
            // the same actor Tick explicitly as a deterministic supplement
            // to the normal world tick.
            constexpr int32 SimulationTickCount = 180;
            for (int32 TickIndex = 0;
                TickIndex < SimulationTickCount;
                ++TickIndex)
            {
                // Queue the standalone actor's button forces before Chaos
                // advances; this transient world does not retain forces that
                // are submitted after its physics frame has completed.
                TestBody->Tick(1.0f / 60.0f);
                World->Tick(ELevelTick::LEVELTICK_All, 1.0f / 60.0f);
                MaxForwardDelta = FMath::Max(
                    MaxForwardDelta,
                    TestBody->GetPhysicsBody()->GetComponentLocation().X
                        - InitialBodyLocation.X
                );
                MaxHeightRise = FMath::Max(
                    MaxHeightRise,
                    TestBody->GetPhysicsBody()->GetComponentLocation().Z
                        - InitialBodyLocation.Z
                );
                MaxVerticalSpeed = FMath::Max(
                    MaxVerticalSpeed,
                    FMath::Abs(TestBody->GetPhysicsBody()
                        ->GetPhysicsLinearVelocity().Z)
                );
                MaxRollAngularSpeed = FMath::Max(
                    MaxRollAngularSpeed,
                    FMath::Abs(TestBody->GetPhysicsBody()
                        ->GetPhysicsAngularVelocityInRadians().X)
                );
            }
            const ACMLegPart* LeftLeg = Cast<ACMLegPart>(LeftPart);
            const ACMLegPart* RightLeg = Cast<ACMLegPart>(RightPart);
            TestNotNull(TEXT("Left default Part is a Leg"), LeftLeg);
            TestNotNull(TEXT("Right default Part is a Leg"), RightLeg);
            const FVector FinalBodyLocation =
                TestBody->GetPhysicsBody()->GetComponentLocation();
            const FVector FinalBodyVelocity =
                TestBody->GetPhysicsBody()->GetPhysicsLinearVelocity();
            AddInfo(FString::Printf(
                TEXT("Four-player input delta=%s velocity=%s presses=%d accepted=%d reverse=%d heightRise=%.2f verticalSpeed=%.2f rollSpeed=%.3f roll=%.2f"),
                *(FinalBodyLocation - InitialBodyLocation).ToCompactString(),
                *FinalBodyVelocity.ToCompactString(),
                TestBody->GetSimulatedButtonPressCount(),
                TestBody->GetSimulatedAcceptedInputCount(),
                TestBody->GetSimulatedReverseButtonPressCount(),
                MaxHeightRise,
                MaxVerticalSpeed,
                MaxRollAngularSpeed,
                TestBody->GetPhysicsBody()
                    ? TestBody->GetPhysicsBody()->GetComponentRotation().Roll
                    : 0.0f
            ));
            TestTrue(TEXT("Automatic movement continues while Arm slots are skipped"),
                TestBody->GetSimulatedButtonPressCount() > 0);
            TestTrue(TEXT("Assigned Part key input is accepted"),
                TestBody->GetSimulatedAcceptedInputCount() > 0);
            TestTrue(TEXT("Delayed turn uses reverse Part buttons"),
                TestBody->GetSimulatedReverseButtonPressCount() > 0);
            TestTrue(TEXT("Part key input drives the body forward"),
                MaxForwardDelta > 2.0f);
            TestEqual(TEXT("IK observation pause lasts five seconds"),
                TestBody->GetSimulatedInputPauseDuration(),
                5.0f);
            TestTrue(TEXT("Movement phase enters the input pause"),
                TestBody->IsSimulatedInputPaused());
            const int32 PressCountAtPause =
                TestBody->GetSimulatedButtonPressCount();
            constexpr int32 PauseObservationTickCount = 30;
            for (int32 TickIndex = 0;
                TickIndex < PauseObservationTickCount;
                ++TickIndex)
            {
                TestBody->Tick(1.0f / 60.0f);
                World->Tick(ELevelTick::LEVELTICK_All, 1.0f / 60.0f);
            }
            TestTrue(TEXT("Input remains paused during IK observation"),
                TestBody->IsSimulatedInputPaused());
            TestEqual(TEXT("Regular movement buttons remain paused"),
                TestBody->GetSimulatedButtonPressCount(),
                PressCountAtPause);
            TestTrue(TEXT("Pause turn calls selected segment legs"),
                TestBody->GetSimulatedPauseTurnButtonPressCount() > 0);
            TestTrue(TEXT("Pause turn accepts selected segment leg inputs"),
                TestBody->GetSimulatedPauseTurnAcceptedInputCount() > 0);
            const float PausedPlanarSpeed = TestBody->GetPhysicsBody()
                ->GetPhysicsLinearVelocity().Size2D();
            TestTrue(TEXT("Body slows down during the IK observation pause"),
                PausedPlanarSpeed < FinalBodyVelocity.Size2D());
            TestTrue(TEXT("Body does not launch vertically"),
                MaxHeightRise < 200.0f && MaxVerticalSpeed < 500.0f);
            TestTrue(TEXT("One-sided lean stays within the diagnostic speed limit"),
                MaxRollAngularSpeed
                    < FMath::DegreesToRadians(120.0f));
            TestTrue(TEXT("Leaning planted feet enter visual replant"),
                TestBody->GetSimulatedVisualReplantCount() > 0);
            AddInfo(FString::Printf(
                TEXT("One-sided lean max planted-foot reach error=%.2fcm visual replants=%d planted=%d operational=%d"),
                TestBody->GetSimulatedMaxReachError(),
                TestBody->GetSimulatedVisualReplantCount(),
                TestBody->GetSimulatedPlantedLegCount(),
                TestBody->GetSimulatedOperationalLegCount()
            ));
        }

        for (int32 SlotIndex = 0;
            SlotIndex < TestBody->GetPartSlotCount();
            ++SlotIndex)
        {
            const UCMPartSlotComponent* PartSlot =
                TestBody->GetPartSlot(SlotIndex);
            TestNotNull(*FString::Printf(
                TEXT("Configured runtime slot %d exists"), SlotIndex),
                PartSlot);
            if (PartSlot)
            {
                TestNotNull(*FString::Printf(
                    TEXT("Configured runtime Part %d is attached"), SlotIndex),
                    PartSlot->GetAttachedPart());
            }
        }

        TestBody->Destroy(true);
    }

    GEngine->ShutdownWorldNetDriver(World);
    World->DestroyWorld(true);
    World->SetPhysicsScene(nullptr);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
