#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Collision/CMCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NiagaraSystem.h"
#include "Parts/Core/CMDroppedPartActor.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Tentacle/CMTentacleSegmentActor.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMTentacleAttachmentDefaultsTest,
    "Chimera.Tentacle.AttachmentDefaults",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMTentacleAttachmentDefaultsTest::RunTest(
    const FString& Parameters)
{
    const ACMTentacleSegmentActor* TentacleDefaults =
        GetDefault<ACMTentacleSegmentActor>();
    const USphereComponent* DetectionSphere = TentacleDefaults
        ? TentacleDefaults->FindComponentByClass<USphereComponent>()
        : nullptr;
    TestNotNull(TEXT("Tentacle has an overlap detector"), DetectionSphere);
    if (DetectionSphere)
    {
        TestTrue(
            TEXT("Tentacle overlap detector generates overlap events"),
            DetectionSphere->GetGenerateOverlapEvents());
        TestEqual(
            TEXT("Tentacle overlap detector is query-only"),
            DetectionSphere->GetCollisionEnabled(),
            ECollisionEnabled::QueryOnly);
        TestEqual(
            TEXT("Tentacle detects Shift-detached Part hurtboxes"),
            DetectionSphere->GetCollisionResponseToChannel(
                CMCollision::ChimeraHurtbox),
            ECR_Overlap);
    }

    const ACMDroppedPartActor* DroppedPartDefaults =
        GetDefault<ACMDroppedPartActor>();
    TestTrue(
        TEXT("Every dropped victim Part is tentacle-interactive"),
        DroppedPartDefaults
            && DroppedPartDefaults->ActorHasTag(
                ACMTentacleSegmentActor::TentacleInteractiveActorTag));
    const ACMArmPart* PartDefaults = GetDefault<ACMArmPart>();
    TestTrue(
        TEXT("Every usable Part inherits the tentacle-interactive tag"),
        PartDefaults
            && PartDefaults->ActorHasTag(
                ACMTentacleSegmentActor::TentacleInteractiveActorTag));
    TestEqual(
        TEXT("Usable Part hurtbox overlaps the tentacle detector"),
        PartDefaults->GetDamageHurtbox()->GetCollisionResponseToChannel(
            ECC_WorldDynamic),
        ECR_Overlap);
    TestTrue(
        TEXT("Dropped Part mesh contributes overlap queries"),
        DroppedPartDefaults
            && DroppedPartDefaults->GetPartMesh()
            && DroppedPartDefaults->GetPartMesh()
                ->GetGenerateOverlapEvents());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMTentacleReservationTest,
    "Chimera.Tentacle.ReservationFirstRequesterWins",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMTentacleReservationTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues InitValues;
    InitValues.AllowAudioPlayback(false)
        .RequiresHitProxies(false)
        .CreateNavigation(false)
        .CreateAISystem(false)
        .ShouldSimulatePhysics(false)
        .EnableTraceCollision(false)
        .SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Game,
        false,
        NAME_None,
        nullptr,
        true,
        ERHIFeatureLevel::Num,
        &InitValues);
    TestNotNull(TEXT("Reservation test world is created"), World);
    if (!World)
    {
        return false;
    }

    ACMDroppedPartActor* DroppedPart =
        World->SpawnActor<ACMDroppedPartActor>();
    AActor* FirstRequester = World->SpawnActor<AActor>();
    AActor* SecondRequester = World->SpawnActor<AActor>();
    TestTrue(
        TEXT("First server requester reserves the dropped Part"),
        DroppedPart
            && DroppedPart->TryReserveForTentacle(FirstRequester));
    TestTrue(
        TEXT("Reservation remembers its owner"),
        DroppedPart
            && DroppedPart->IsReservedByTentacle(FirstRequester));
    TestFalse(
        TEXT("Later requester cannot steal the same dropped Part"),
        DroppedPart
            && DroppedPart->TryReserveForTentacle(SecondRequester));

    if (DroppedPart)
    {
        DroppedPart->ReleaseTentacleReservation(SecondRequester);
    }
    TestFalse(
        TEXT("Non-owner cannot release the reservation"),
        DroppedPart
            && DroppedPart->TryReserveForTentacle(SecondRequester));

    if (DroppedPart)
    {
        DroppedPart->ReleaseTentacleReservation(FirstRequester);
    }
    TestTrue(
        TEXT("Next requester may reserve after the owner releases"),
        DroppedPart
            && DroppedPart->TryReserveForTentacle(SecondRequester));

    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMTentacleBlueprintIntegrationTest,
    "Chimera.Tentacle.BlueprintIntegration",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMTentacleBlueprintIntegrationTest::RunTest(
    const FString& Parameters)
{
    UClass* TentacleClass = LoadClass<ACMTentacleSegmentActor>(
        nullptr,
        TEXT("/Game/Chimera/Character/Tentacle/BP_CMTentacleSegment.BP_CMTentacleSegment_C"));
    const ACMTentacleSegmentActor* TentacleDefaults = TentacleClass
        ? TentacleClass->GetDefaultObject<ACMTentacleSegmentActor>()
        : nullptr;
    TestNotNull(TEXT("Tentacle Blueprint class loads"), TentacleDefaults);

    TArray<UStaticMeshComponent*> GooMeshes;
    if (TentacleDefaults)
    {
        TentacleDefaults->GetComponents(GooMeshes);
    }
    const UStaticMeshComponent* GooBody = GooMeshes.IsEmpty()
        ? nullptr
        : GooMeshes[0];
    TestNotNull(TEXT("Tentacle Blueprint contains BP_Goo body"), GooBody);
    if (GooBody)
    {
        TestEqual(
            TEXT("BP_Goo sphere mesh is copied"),
            GetPathNameSafe(GooBody->GetStaticMesh()),
            FString(TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/SM/SM_VFX_Smooth_Sphere_01.SM_VFX_Smooth_Sphere_01")));
        TestEqual(
            TEXT("BP_Goo material is copied"),
            GetPathNameSafe(GooBody->GetMaterial(0)),
            FString(TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/Materials/MI_VFX_Goo_01.MI_VFX_Goo_01")));
    }

    if (TentacleDefaults)
    {
        TestEqual(
            TEXT("BP_Goo drips Niagara is copied"),
            GetPathNameSafe(TentacleDefaults->GooDripsNiagaraSystem),
            FString(TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/Particles/NS_Goo_Drips.NS_Goo_Drips")));
        TestEqual(
            TEXT("BP_Goo upward Niagara is copied"),
            GetPathNameSafe(TentacleDefaults->SourceNiagaraSystem),
            FString(TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/Particles/NS_Goo_Up.NS_Goo_Up")));
        TestEqual(
            TEXT("BP_Spline tip Niagara is copied"),
            GetPathNameSafe(TentacleDefaults->TargetNiagaraSystem),
            FString(TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/Particles/NS_Goo_Small_Drips.NS_Goo_Small_Drips")));
        TestEqual(
            TEXT("BP_Spline material is copied"),
            GetPathNameSafe(TentacleDefaults->TentacleMaterial),
            FString(TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/Materials/MI_VFX_Goo_Arm_01.MI_VFX_Goo_Arm_01")));

        TSet<FString> ActualTentacleMeshes;
        for (const UStaticMesh* Mesh : TentacleDefaults->TentacleMeshVariants)
        {
            ActualTentacleMeshes.Add(GetPathNameSafe(Mesh));
        }
        const TCHAR* ExpectedTentacleMeshes[] = {
            TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/SM/SM_VFX_Arm_03.SM_VFX_Arm_03"),
            TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/SM/SM_VFX_Arm_Double_01.SM_VFX_Arm_Double_01"),
            TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/SM/SM_VFX_Arm_Double_02.SM_VFX_Arm_Double_02")
        };
        TestEqual(
            TEXT("All three BP_Spline mesh variants are copied"),
            ActualTentacleMeshes.Num(),
            static_cast<int32>(UE_ARRAY_COUNT(ExpectedTentacleMeshes)));
        for (const TCHAR* ExpectedMesh : ExpectedTentacleMeshes)
        {
            TestTrue(
                FString::Printf(TEXT("BP_Spline mesh is copied: %s"), ExpectedMesh),
                ActualTentacleMeshes.Contains(ExpectedMesh));
        }
    }

    const TCHAR* PartClassPaths[] = {
        TEXT("/Game/Chimera/Character/Part/Arm/BP_CMArmPart.BP_CMArmPart_C"),
        TEXT("/Game/Chimera/Character/Part/Arm/BP_CMSpringArmPart.BP_CMSpringArmPart_C"),
        TEXT("/Game/Chimera/Character/Part/Head/BluePrint/BP_CMHead01HeadPart.BP_CMHead01HeadPart_C"),
        TEXT("/Game/Chimera/Character/Part/Leg/BP_CMLegPart.BP_CMLegPart_C")
    };
    for (const TCHAR* PartClassPath : PartClassPaths)
    {
        UClass* PartClass = LoadClass<ACMPartActorBase>(
            nullptr, PartClassPath);
        const ACMPartActorBase* PartDefaults = PartClass
            ? PartClass->GetDefaultObject<ACMPartActorBase>()
            : nullptr;
        TestTrue(
            FString::Printf(TEXT("Part Blueprint has tentacle tag: %s"),
                PartClassPath),
            PartDefaults
                && PartDefaults->ActorHasTag(
                    ACMTentacleSegmentActor::TentacleInteractiveActorTag));
    }

    UWorld::InitializationValues InitValues;
    InitValues.AllowAudioPlayback(false)
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
    TestNotNull(TEXT("Chimera integration world is created"), World);
    if (!World)
    {
        return false;
    }

    UClass* ChimeraClass = LoadClass<ACMChimera>(
        nullptr,
        TEXT("/Game/Chimera/Character/BP_CMChimera.BP_CMChimera_C"));
    ACMChimera* Chimera = ChimeraClass
        ? World->SpawnActor<ACMChimera>(ChimeraClass)
        : nullptr;
    TestNotNull(TEXT("BP_CMChimera spawns"), Chimera);
    if (Chimera && !Chimera->HasActorBegunPlay())
    {
		// Exercise only the production tentacle synchronization path. Dispatching the
		// complete Chimera BeginPlay in this synthetic world also initializes GAS,
		// whose project attribute sets are intentionally absent from this fixture.
		Chimera->InitializeSegmentHealth(100.0f);
		Chimera->RefreshTentacleSegments();
    }

    int32 OwnedTentacleCount = 0;
    TSet<int32> SegmentIndices;
    for (TActorIterator<ACMTentacleSegmentActor> It(World); It; ++It)
    {
        if (It->GetOwner() == Chimera)
        {
            ++OwnedTentacleCount;
            SegmentIndices.Add(It->GetSegmentIndex());
        }
    }
    if (Chimera)
    {
        TestEqual(
            TEXT("BP_CMChimera creates one tentacle per active segment"),
            OwnedTentacleCount,
            Chimera->GetActiveSegmentCount());
        TestEqual(
            TEXT("Each tentacle maps to one distinct BodyMesh_n"),
            SegmentIndices.Num(),
            Chimera->GetActiveSegmentCount());
    }

    ACMTentacleSegmentActor* RuntimeTentacle = nullptr;
    for (TActorIterator<ACMTentacleSegmentActor> It(World); It; ++It)
    {
        if (It->GetOwner() == Chimera)
        {
            RuntimeTentacle = *It;
            break;
        }
    }
    UClass* RuntimeArmClass = LoadClass<ACMArmPart>(
        nullptr,
        TEXT("/Game/Chimera/Character/Part/Arm/BP_CMArmPart.BP_CMArmPart_C"));
    ACMArmPart* RuntimeTarget = World->SpawnActor<ACMArmPart>(
        RuntimeArmClass,
        RuntimeTentacle
            ? RuntimeTentacle->GetActorLocation() + FVector(200.0f, 0.0f, 0.0f)
            : FVector::ZeroVector,
        FRotator::ZeroRotator);
    TestNotNull(TEXT("Runtime tentacle target spawns"), RuntimeTarget);
    if (RuntimeTentacle && RuntimeTarget)
    {
        const ECollisionEnabled::Type ExpectedMountedMeshCollision =
            RuntimeTarget->GetPartMesh()->GetCollisionEnabled();
        const ECollisionEnabled::Type ExpectedMountedHurtboxCollision =
            RuntimeTarget->GetDamageHurtbox()->GetCollisionEnabled();
        if (!RuntimeTarget->HasActorBegunPlay())
        {
            RuntimeTarget->DispatchBeginPlay();
        }
        TestTrue(
            TEXT("Loose usable Part runs skeletal-mesh ragdoll"),
            RuntimeTarget->GetPartMesh()->IsSimulatingPhysics());
        TestEqual(
            TEXT("Loose usable Part disables its separate hurtbox"),
            RuntimeTarget->GetDamageHurtbox()->GetCollisionEnabled(),
            ECollisionEnabled::NoCollision);
        TestEqual(
            TEXT("Loose usable Part uses skeletal-mesh collision"),
            RuntimeTarget->GetPartMesh()->GetCollisionEnabled(),
            ECollisionEnabled::QueryAndPhysics);

        RuntimeTentacle->SetTetheredActor(RuntimeTarget);
        RuntimeTentacle->UpdateVisual(1.0f);

        const USplineMeshComponent* RuntimeSpline =
            RuntimeTentacle->RuntimeSplineMesh;
        TestNotNull(TEXT("Overlap target creates a runtime spline mesh"), RuntimeSpline);
        if (RuntimeSpline)
        {
            TestTrue(
                TEXT("Runtime spline mesh is registered"),
                RuntimeSpline->IsRegistered());
            TestEqual(
                TEXT("Runtime spline mesh is movable"),
                RuntimeSpline->Mobility,
                EComponentMobility::Movable);
            TestEqual(
                TEXT("Runtime spline mesh attaches to its body tentacle root"),
                RuntimeSpline->GetAttachParent(),
                RuntimeTentacle->SceneRoot.Get());
            TestEqual(
                TEXT("Runtime spline uses SM_VFX_Arm_03"),
                GetPathNameSafe(RuntimeSpline->GetStaticMesh()),
                FString(TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/SM/SM_VFX_Arm_03.SM_VFX_Arm_03")));
            TestNotNull(
                TEXT("Runtime spline uses the Goo Arm material"),
                RuntimeSpline->GetMaterial(0));
        }

        FCMPartSlotAddress PullSlotAddress;
        PullSlotAddress.SegmentIndex = RuntimeTentacle->GetSegmentIndex();
        PullSlotAddress.PartSlotIndex = 0;
        UCMPartSlotComponent* PullSlot =
            Chimera->GetPartSlotComponent(PullSlotAddress);
        TestNotNull(TEXT("Pull destination slot exists"), PullSlot);
        TestTrue(
            TEXT("Empty-slot input starts pulling a loose usable Part"),
            RuntimeTentacle->TryBeginPartAttachment(PullSlotAddress));
        TestTrue(
            TEXT("Loose usable Part is reserved during the pull"),
            RuntimeTarget->IsReservedByTentacle(RuntimeTentacle));
        TestFalse(
            TEXT("Tentacle pull temporarily stops loose Part ragdoll"),
            RuntimeTarget->GetPartMesh()->IsSimulatingPhysics());

        RuntimeTentacle->UpdatePull(RuntimeTentacle->PullDuration);
        TestEqual(
            TEXT("Part attaches after entering the slot acceptance distance"),
            PullSlot ? PullSlot->GetAttachedPart() : nullptr,
            static_cast<AActor*>(RuntimeTarget));
        TestFalse(
            TEXT("Part reservation clears after attachment"),
            RuntimeTarget->IsReservedByTentacle(RuntimeTentacle));
        TestFalse(
            TEXT("Attached Part keeps ragdoll disabled"),
            RuntimeTarget->GetPartMesh()->IsSimulatingPhysics());
        TestEqual(
            TEXT("Attached Part restores its authored mesh collision"),
            RuntimeTarget->GetPartMesh()->GetCollisionEnabled(),
            ExpectedMountedMeshCollision);
        TestEqual(
            TEXT("Attached Part restores its authored gameplay hurtbox"),
            RuntimeTarget->GetDamageHurtbox()->GetCollisionEnabled(),
            ExpectedMountedHurtboxCollision);

        AActor* DetachedPart = Chimera->DetachPartFromSlot(
            PullSlotAddress);
        TestEqual(
            TEXT("Mounted Part can be detached again"),
            DetachedPart,
            static_cast<AActor*>(RuntimeTarget));
        TestTrue(
            TEXT("Detached Part returns to skeletal-mesh ragdoll"),
            RuntimeTarget->GetPartMesh()->IsSimulatingPhysics());
        TestEqual(
            TEXT("Detached Part disables its gameplay hurtbox again"),
            RuntimeTarget->GetDamageHurtbox()->GetCollisionEnabled(),
            ECollisionEnabled::NoCollision);
    }

    World->DestroyWorld(false);
    return true;
}

#endif
