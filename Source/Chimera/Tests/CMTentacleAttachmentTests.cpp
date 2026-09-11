#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Collision/CMCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Parts/Core/CMDroppedPartActor.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Tentacle/CMTentacleSegmentActor.h"
#include "Player/CMChimera.h"
#include "Player/CMChimeraBodySegmentActor.h"
#include "Player/CMChimeraIdleTentacleComponent.h"
#include "Player/CMRuntimeChildActorComponent.h"
#include "Player/CMPartSlotComponent.h"
#include "Stage/Device/Component/CMInteractionHighlightComponent.h"

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
    const UCMInteractionHighlightComponent* DroppedHighlight =
        DroppedPartDefaults
            ? DroppedPartDefaults->FindComponentByClass<
                UCMInteractionHighlightComponent>()
            : nullptr;
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
    TestNotNull(TEXT("Dropped Part has an interaction highlight"),
        DroppedHighlight);
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
    UClass* SegmentPresentationClass =
        LoadClass<ACMChimeraBodySegmentActor>(
            nullptr,
            TEXT("/Game/Chimera/Character/Chimera/Blueprint/BP_CMChimeraBodySegment.BP_CMChimeraBodySegment_C"));
    const ACMChimeraBodySegmentActor* SegmentPresentationDefaults =
        SegmentPresentationClass
            ? SegmentPresentationClass
                ->GetDefaultObject<ACMChimeraBodySegmentActor>()
            : nullptr;
    TestNotNull(
        TEXT("Body segment presentation Blueprint class loads"),
        SegmentPresentationDefaults);
    TestNotNull(
        TEXT("Body segment Blueprint contains BodyVisual"),
        SegmentPresentationDefaults
            ? SegmentPresentationDefaults
                ->FindComponentByClass<USkeletalMeshComponent>()
            : nullptr);
    const UChildActorComponent* SegmentTentacleSlot =
        SegmentPresentationDefaults
            ? SegmentPresentationDefaults
                ->FindComponentByClass<UChildActorComponent>()
            : nullptr;
    TestNotNull(
        TEXT("Body segment Blueprint explicitly contains TentacleActor"),
        SegmentTentacleSlot);
    TestTrue(
        TEXT("Only the nested TentacleActor is runtime-only in editor previews"),
        SegmentTentacleSlot
            && SegmentTentacleSlot->IsA<
                UCMRuntimeChildActorComponent>());
    TestNotNull(
        TEXT("Body segment Blueprint contains the surface-sampled IdleTentacles component"),
        SegmentPresentationDefaults
            ? SegmentPresentationDefaults->FindComponentByClass<
                UCMChimeraIdleTentacleComponent>()
            : nullptr);

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
            TEXT("Legacy BP_Goo drips fallback remains available"),
            GetPathNameSafe(TentacleDefaults->GooDripsNiagaraSystem),
            FString(TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/Particles/NS_Goo_Drips.NS_Goo_Drips")));
        TestEqual(
            TEXT("BP_Goo upward Niagara is copied"),
            GetPathNameSafe(TentacleDefaults->SourceNiagaraSystem),
            FString(TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/Particles/NS_Goo_Up.NS_Goo_Up")));
        TestEqual(
            TEXT("Blueprint Goo drips component keeps its authored Niagara asset"),
            GetPathNameSafe(TentacleDefaults->GooDripsEffect
                ? TentacleDefaults->GooDripsEffect->GetAsset()
                : nullptr),
            GetPathNameSafe(TentacleDefaults->SourceEffect
                ? TentacleDefaults->SourceEffect->GetAsset()
                : nullptr));
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
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(
        EWorldType::Game);
    WorldContext.SetCurrentWorld(World);

    UClass* ChimeraClass = LoadClass<ACMChimera>(
        nullptr,
        TEXT("/Game/Chimera/Character/BP_CMChimera.BP_CMChimera_C"));
    const ACMChimera* ChimeraDefaults = ChimeraClass
        ? ChimeraClass->GetDefaultObject<ACMChimera>()
        : nullptr;
    TArray<UChildActorComponent*> PresentationSlots;
    if (ChimeraDefaults)
    {
        ChimeraDefaults->GetComponents(PresentationSlots);
        PresentationSlots.RemoveAll([](const UChildActorComponent* Component)
        {
            return !Component
                || !Component->GetName().StartsWith(
                    TEXT("SegmentPresentation_"));
        });
    }
    TestEqual(
        TEXT("BP_CMChimera exposes every segment presentation in its editor viewport"),
        PresentationSlots.Num(),
        CMControl::MaxSegments);
    for (const UChildActorComponent* PresentationSlot : PresentationSlots)
    {
        TestFalse(
            TEXT("Outer presentation slots are not suppressed in editor worlds"),
            PresentationSlot->IsA<UCMRuntimeChildActorComponent>());
    }
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
    int32 ActiveTentacleCount = 0;
    TSet<int32> SegmentIndices;
    TSet<int32> ActiveSegmentIndices;
    TArray<ACMChimeraBodySegmentActor*> InitialPresentations;
    for (TActorIterator<ACMTentacleSegmentActor> It(World); It; ++It)
    {
        if (It->GetOwner() == Chimera)
        {
            ++OwnedTentacleCount;
            SegmentIndices.Add(It->GetSegmentIndex());
            if (It->IsSegmentActive())
            {
                ++ActiveTentacleCount;
                ActiveSegmentIndices.Add(It->GetSegmentIndex());
            }
        }
    }
    if (Chimera)
    {
        TestEqual(
            TEXT("BP_CMChimera keeps one pre-authored tentacle per segment"),
            OwnedTentacleCount,
            CMControl::MaxSegments);
        TestEqual(
            TEXT("Each tentacle maps to one distinct BodyMesh_n"),
            SegmentIndices.Num(),
            CMControl::MaxSegments);
        TestEqual(
            TEXT("Only active body segments enable their tentacle"),
            ActiveTentacleCount,
            Chimera->GetActiveSegmentCount());
        TestEqual(
            TEXT("Each active tentacle maps to one distinct BodyMesh_n"),
            ActiveSegmentIndices.Num(),
            Chimera->GetActiveSegmentCount());

        for (int32 SegmentIndex = 0;
            SegmentIndex < CMControl::MaxSegments;
            ++SegmentIndex)
        {
            ACMChimeraBodySegmentActor* Presentation =
                Chimera->GetBodySegmentPresentation(SegmentIndex);
            InitialPresentations.Add(Presentation);
            TestNotNull(
                *FString::Printf(
                    TEXT("Segment %d owns a presentation"),
                    SegmentIndex),
                Presentation);
            if (Presentation)
            {
                TestTrue(
                    *FString::Printf(
                        TEXT("Segment %d uses the body segment Blueprint"),
                        SegmentIndex),
                    SegmentPresentationClass
                        && Presentation->IsA(SegmentPresentationClass));
                TestEqual(
                    *FString::Printf(
                        TEXT("Segment %d keeps a stable index"),
                        SegmentIndex),
                    Presentation->GetSegmentIndex(),
                    SegmentIndex);
                TestEqual(
                    *FString::Printf(
                        TEXT("Segment %d active state follows the Chimera"),
                        SegmentIndex),
                    Presentation->IsSegmentActive(),
                    SegmentIndex < Chimera->GetActiveSegmentCount());
            }
        }

        if (!InitialPresentations.Contains(nullptr))
        {
            TestEqual(
                TEXT("The first active segment is Head"),
                InitialPresentations[0]->GetVisualRole(),
                ECMChimeraSegmentVisualRole::Head);
            TestEqual(
                TEXT("The last active segment is Tail"),
                InitialPresentations[Chimera->GetActiveSegmentCount() - 1]
                    ->GetVisualRole(),
                ECMChimeraSegmentVisualRole::Tail);
        }
    }

    ACMTentacleSegmentActor* RuntimeTentacle = nullptr;
    for (TActorIterator<ACMTentacleSegmentActor> It(World); It; ++It)
    {
        if (It->GetOwner() == Chimera && It->IsSegmentActive())
        {
            RuntimeTentacle = *It;
            break;
        }
    }
    UCMChimeraIdleTentacleComponent* RuntimeIdleTentacles = nullptr;
    ACMChimeraBodySegmentActor* RuntimePresentation = nullptr;
    if (Chimera && RuntimeTentacle)
    {
        RuntimePresentation = Chimera->GetBodySegmentPresentation(
            RuntimeTentacle->GetSegmentIndex());
        if (RuntimePresentation)
        {
            RuntimeIdleTentacles = RuntimePresentation->FindComponentByClass<
                UCMChimeraIdleTentacleComponent>();
        }
    }
    TestNotNull(
        TEXT("Active segment contains IdleTentacles"),
        RuntimeIdleTentacles);
    if (RuntimeIdleTentacles && RuntimeTentacle)
    {
        UMeshComponent* ExpectedIdleSource =
            RuntimeTentacle->GetGooBodyComponent();
        TestEqual(
            TEXT("IdleTentacles samples the tentacle segment GooBody"),
            RuntimeIdleTentacles->GetSourceMeshComponent(),
            ExpectedIdleSource);
        RuntimeIdleTentacles->SetEffectActive(false);
        RuntimeIdleTentacles->ConfigureSource(
            nullptr, RuntimeTentacle->GetSegmentIndex());
        RuntimeIdleTentacles->SetEffectActive(true);
        for (int32 RetryIndex = 0; RetryIndex < 10; ++RetryIndex)
        {
            RuntimeIdleTentacles->TickComponent(
                0.25f,
                LEVELTICK_All,
                nullptr);
        }
        TestTrue(
            TEXT("Client idle tentacles keep retrying while their source mesh is pending"),
            RuntimeIdleTentacles->IsComponentTickEnabled());
        RuntimeIdleTentacles->ConfigureSource(
            ExpectedIdleSource, RuntimeTentacle->GetSegmentIndex());
        RuntimeIdleTentacles->TickComponent(
            0.3f,
            LEVELTICK_All,
            nullptr);
        TestTrue(
            TEXT("SM_VFX_Smooth_Sphere_01 produces upper-surface candidates"),
            RuntimeIdleTentacles->GetSurfaceCandidateCount() > 0);
        TestEqual(
            TEXT("Idle tentacles use the shortened ninety-centimeter length"),
            RuntimeIdleTentacles->GetTentacleLength(),
            90.0f);
        RuntimeIdleTentacles->TickComponent(
            0.01f,
            LEVELTICK_All,
            nullptr);
        TestEqual(
            TEXT("Each active segment grows four idle tentacles"),
            RuntimeIdleTentacles->GetActiveTentacleCount(),
            4);
        RuntimeIdleTentacles->TickComponent(
            0.3f,
            LEVELTICK_All,
            nullptr);
        TArray<USplineMeshComponent*> IdleSplineMeshes;
        RuntimePresentation->GetComponents<USplineMeshComponent>(
            IdleSplineMeshes);
        int32 VisibleNonDegenerateIdleSpans = 0;
        const USplineMeshComponent* VisibleIdleSplineMesh = nullptr;
        for (const USplineMeshComponent* IdleSplineMesh
            : IdleSplineMeshes)
        {
            if (IdleSplineMesh
                && IdleSplineMesh->IsVisible()
                && !IdleSplineMesh->bHiddenInGame
                && !IdleSplineMesh->GetStartPosition().Equals(
                    IdleSplineMesh->GetEndPosition()))
            {
                ++VisibleNonDegenerateIdleSpans;
                VisibleIdleSplineMesh = IdleSplineMesh;
            }
        }
        TestTrue(
            TEXT("Growing idle tentacles render non-degenerate spline spans"),
            VisibleNonDegenerateIdleSpans > 0);
        TestNotNull(
            TEXT("Growing idle tentacles expose a visible spline mesh"),
            VisibleIdleSplineMesh);
        if (VisibleIdleSplineMesh)
        {
            TestEqual(
                TEXT("Idle spline uses the existing arm mesh"),
                GetPathNameSafe(VisibleIdleSplineMesh->GetStaticMesh()),
                FString(TEXT("/Game/Vefects/Tentacles_VFX/VFX/Goo/SM/SM_VFX_Arm_03.SM_VFX_Arm_03")));
            TestNotNull(
                TEXT("Idle spline uses a dynamic Goo Arm material"),
                Cast<UMaterialInstanceDynamic>(
                    VisibleIdleSplineMesh->GetMaterial(0)));
            TestTrue(
                TEXT("Idle spline is rendered with a thin cross-section"),
                VisibleIdleSplineMesh->GetStartScale().GetMax()
                    <= 0.6f
                && VisibleIdleSplineMesh->GetEndScale().GetMax()
                    <= 0.6f);
            TestFalse(
                TEXT("Idle spline start tangent follows the animated curve"),
                VisibleIdleSplineMesh->GetStartTangent().IsNearlyZero());
            TestFalse(
                TEXT("Idle spline end tangent follows the animated curve"),
                VisibleIdleSplineMesh->GetEndTangent().IsNearlyZero());
            const FTransform& SplineMeshTransform =
                VisibleIdleSplineMesh->GetComponentTransform();
            const FBox SplineMeshBounds =
                VisibleIdleSplineMesh->Bounds.GetBox();
            TestTrue(
                TEXT("Animated start remains inside spline render bounds"),
                SplineMeshBounds.IsInsideOrOn(
                    SplineMeshTransform.TransformPosition(
                        VisibleIdleSplineMesh->GetStartPosition())));
            TestTrue(
                TEXT("Animated end remains inside spline render bounds"),
                SplineMeshBounds.IsInsideOrOn(
                    SplineMeshTransform.TransformPosition(
                        VisibleIdleSplineMesh->GetEndPosition())));
        }

        USkeletalMeshComponent* RuntimeBodyVisual =
            RuntimePresentation->FindComponentByClass<
                USkeletalMeshComponent>();
        TestEqual(
            TEXT("Idle tentacles attach independently from BodyVisual"),
            RuntimeIdleTentacles->GetAttachParent(),
            RuntimePresentation->GetRootComponent());
        if (RuntimeBodyVisual)
        {
            RuntimeBodyVisual->SetVisibility(false, true);
            RuntimeBodyVisual->SetHiddenInGame(true, true);
            TestTrue(
                TEXT("BodyVisual visibility does not hide idle spline meshes"),
                VisibleIdleSplineMesh
                && VisibleIdleSplineMesh->IsVisible()
                && !VisibleIdleSplineMesh->bHiddenInGame);
            RuntimeBodyVisual->SetVisibility(true, true);
            RuntimeBodyVisual->SetHiddenInGame(false, true);
        }

        RuntimeIdleTentacles->TickComponent(
            3.0f,
            LEVELTICK_All,
            nullptr);
        RuntimeIdleTentacles->TickComponent(
            1.0f,
            LEVELTICK_All,
            nullptr);
        RuntimeIdleTentacles->TickComponent(
            0.3f,
            LEVELTICK_All,
            nullptr);
        int32 VisibleRespawnedIdleSpans = 0;
        for (const USplineMeshComponent* IdleSplineMesh
            : IdleSplineMeshes)
        {
            if (IdleSplineMesh
                && IdleSplineMesh->IsVisible()
                && !IdleSplineMesh->bHiddenInGame
                && !IdleSplineMesh->GetStartPosition().Equals(
                    IdleSplineMesh->GetEndPosition()))
            {
                ++VisibleRespawnedIdleSpans;
            }
        }
        TestTrue(
            TEXT("Idle spline meshes remain visible after respawning"),
            VisibleRespawnedIdleSpans > 0);
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
        ECollisionEnabled::Type ExpectedMountedMeshCollision =
            RuntimeTarget->GetPartMesh()->GetCollisionEnabled();
        if (ExpectedMountedMeshCollision
            == ECollisionEnabled::QueryAndPhysics)
        {
            ExpectedMountedMeshCollision = ECollisionEnabled::QueryOnly;
        }
        else if (ExpectedMountedMeshCollision
            == ECollisionEnabled::PhysicsOnly)
        {
            ExpectedMountedMeshCollision = ECollisionEnabled::NoCollision;
        }
        const ECollisionEnabled::Type ExpectedMountedHurtboxCollision =
            RuntimeTarget->GetDamageHurtbox()->GetCollisionEnabled();
        const ACMArmPart* RuntimeArmDefaults =
            RuntimeArmClass->GetDefaultObject<ACMArmPart>();
        const FTransform ExpectedMountedMeshRelativeTransform =
            RuntimeArmDefaults->GetPartMesh()->GetRelativeTransform();
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

        USkeletalMeshComponent* RuntimeTargetMesh =
            RuntimeTarget->GetPartMesh();
        const FVector TargetActorOrigin = RuntimeTarget->GetActorLocation();
        RuntimeTargetMesh->SetWorldLocation(
            TargetActorOrigin + FVector(0.0f, 0.0f, 150.0f),
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        RuntimeTargetMesh->UpdateBounds();
        RuntimeTarget->UpdateReplicatedLoosePartLocation();
        TestTrue(
            TEXT("Gameplay resolves the server-authored loose Part center"),
            RuntimeTentacle->ResolveAuthoritativePickupLocation(
                RuntimeTarget).Equals(
                    RuntimeTargetMesh->Bounds.Origin,
                    1.0f));
        const FVector ResolvedMeshTarget =
            RuntimeTentacle->ResolveVisualTargetLocation(RuntimeTarget);
        TestTrue(
            TEXT("Ragdoll target resolves toward its displaced skeletal mesh"),
            FVector::DistSquared(
                ResolvedMeshTarget,
                RuntimeTargetMesh->Bounds.Origin)
            < FVector::DistSquared(
                TargetActorOrigin,
                RuntimeTargetMesh->Bounds.Origin));

        RuntimeTentacle->TargetEffect = NewObject<UNiagaraComponent>(
            RuntimeTentacle,
            TEXT("TestTargetContactEffect"));
        RuntimeTentacle->TargetEffect->SetupAttachment(RuntimeTargetMesh);
        RuntimeTentacle->AddInstanceComponent(
            RuntimeTentacle->TargetEffect);
        RuntimeTentacle->TargetEffect->RegisterComponent();
        RuntimeTentacle->SetTetheredActor(RuntimeTarget);
        RuntimeTentacle->UpdateVisual(1.0f);
        TestNotNull(
            TEXT("Tethered Part creates its contact Niagara effect"),
            RuntimeTentacle->TargetEffect.Get());
        if (RuntimeTentacle->TargetEffect)
        {
            TestTrue(
                TEXT("Contact Niagara stays on the skeletal-mesh contact point"),
                RuntimeTentacle->TargetEffect->GetComponentLocation().Equals(
                    ResolvedMeshTarget,
                    1.0f));
        }

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
            TestTrue(
                TEXT("Runtime spline ends on the ragdoll skeletal mesh"),
                RuntimeSpline->GetComponentTransform().TransformPosition(
                    RuntimeSpline->GetEndPosition()).Equals(
                        ResolvedMeshTarget,
                        1.0f));
            TestEqual(
                TEXT("Part-targeting tentacle uses the enlarged width"),
                RuntimeSpline->GetStartScale(),
                FVector2D(1.8f));
            TestEqual(
                TEXT("Part-targeting tentacle keeps its enlarged target width"),
                RuntimeSpline->GetEndScale(),
                FVector2D(1.8f));
        }

        FCMPartSlotAddress PullSlotAddress;
        PullSlotAddress.SegmentIndex = RuntimeTentacle->GetSegmentIndex();
        PullSlotAddress.PartSlotIndex = 0;
        RuntimeTargetMesh->UpdateBounds();
        const FVector AuthoritativeRagdollCenter =
            RuntimeTargetMesh->Bounds.Origin;
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
        TestEqual(
            TEXT("Tentacle pull disables Physics Asset collision before entering the body"),
            RuntimeTarget->GetPartMesh()->GetCollisionEnabled(),
            ECollisionEnabled::NoCollision);
        TestTrue(
            TEXT("Tentacle pull starts from the authoritative ragdoll center"),
            RuntimeTarget->GetActorLocation().Equals(
                AuthoritativeRagdollCenter,
                1.0f));

        RuntimeTentacle->UpdatePull(RuntimeTentacle->PullDuration);
        TestEqual(
            TEXT("Part attaches after entering the slot acceptance distance"),
            PullSlot ? PullSlot->GetAttachedPart() : nullptr,
            static_cast<AActor*>(RuntimeTarget));
        TestEqual(
            TEXT("Attached Part is owned by the authoritative Chimera"),
            RuntimeTarget->GetOwner(),
            static_cast<AActor*>(Chimera));
        TestFalse(
            TEXT("Part reservation clears after attachment"),
            RuntimeTarget->IsReservedByTentacle(RuntimeTentacle));
        TestFalse(
            TEXT("Attached Part keeps ragdoll disabled"),
            RuntimeTarget->GetPartMesh()->IsSimulatingPhysics());
        TestEqual(
            TEXT("Attached Part mesh returns to its authored root"),
            RuntimeTarget->GetPartMesh()->GetAttachParent(),
            RuntimeTarget->GetRootComponent());
        TestTrue(
            TEXT("Attached Part restores its authored relative transform"),
            RuntimeTarget->GetPartMesh()->GetRelativeTransform().Equals(
                ExpectedMountedMeshRelativeTransform));
        TestEqual(
            TEXT("Attached Part restores query-only authored mesh collision"),
            RuntimeTarget->GetPartMesh()->GetCollisionEnabled(),
            ExpectedMountedMeshCollision);
        TestEqual(
            TEXT("Attached Part restores its authored gameplay hurtbox"),
            RuntimeTarget->GetDamageHurtbox()->GetCollisionEnabled(),
            ExpectedMountedHurtboxCollision);
        const FTransform ExpectedAttachedRootTransform =
            RuntimeTarget->GetRootComponent()->GetRelativeTransform();
        RuntimeTarget->GetPartMesh()->SetVisibility(false, true);
        RuntimeTarget->GetPartMesh()->SetHiddenInGame(true, true);
        RuntimeTarget->SetRole(ROLE_SimulatedProxy);
        RuntimeTarget->PrepareForPartSlotAttachment();
        TestTrue(
            TEXT("Client attachment restores a living Part mesh visibility"),
            RuntimeTarget->GetPartMesh()->IsVisible());
        TestFalse(
            TEXT("Client attachment clears a living Part mesh hidden state"),
            RuntimeTarget->GetPartMesh()->bHiddenInGame);
        RuntimeTarget->DetachFromActor(
            FDetachmentTransformRules::KeepWorldTransform);
        RuntimeTarget->AttachedSlotAddress = PullSlotAddress;
        RuntimeTarget->OnRep_AttachmentPhysicsState();
        TestEqual(
            TEXT("Client Part mount address restores the slot attachment"),
            RuntimeTarget->GetRootComponent()->GetAttachParent(),
            static_cast<USceneComponent*>(PullSlot));
        TestTrue(
            TEXT("Client Part mount address restores the server-relative transform"),
            RuntimeTarget->GetRootComponent()->GetRelativeTransform().Equals(
                ExpectedAttachedRootTransform));
        RuntimeTarget->SetRole(ROLE_Authority);

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

        RuntimeTarget->SetRole(ROLE_SimulatedProxy);
        TestFalse(
            TEXT("Test Part can emulate a client role"),
            RuntimeTarget->HasAuthority());
        TestFalse(
            TEXT("Detached Part has no replicated mount address"),
            CMControl::IsValidPartSlot(
                RuntimeTarget->GetAttachedSlotAddress()));
        RuntimeTarget->OnRep_AttachmentPhysicsState();
        TestFalse(
            TEXT("Client presentation never simulates loose Part physics"),
            RuntimeTarget->GetPartMesh()->IsSimulatingPhysics());
        TestEqual(
            TEXT("Client loose Part Physics Asset cannot affect local bodies"),
            RuntimeTarget->GetPartMesh()->GetCollisionEnabled(),
            ECollisionEnabled::NoCollision);
        const FVector ReplicatedServerCenter(375.0f, -125.0f, 240.0f);
        RuntimeTarget->ReplicatedLoosePartLocation = ReplicatedServerCenter;
        RuntimeTarget->OnRep_ReplicatedLoosePartLocation();
        RuntimeTarget->GetPartMesh()->UpdateBounds();
        TestTrue(
            TEXT("Client loose Part is centered on the replicated server position"),
            RuntimeTarget->GetPartMesh()->Bounds.Origin.Equals(
                ReplicatedServerCenter,
                1.0f));
        TestTrue(
            TEXT("Client gameplay reads the same replicated pickup position"),
            RuntimeTarget->GetAuthoritativePickupLocation().Equals(
                ReplicatedServerCenter,
                0.1f));

        UClass* RuntimeHeadClass = LoadClass<ACMPartActorBase>(
            nullptr,
            TEXT("/Game/Chimera/Character/Part/Head/BluePrint/BP_CMHead01HeadPart.BP_CMHead01HeadPart_C"));
        ACMPartActorBase* RuntimeHead = PullSlot
            ? World->SpawnActor<ACMPartActorBase>(
                RuntimeHeadClass,
                PullSlot->GetComponentTransform())
            : nullptr;
        TestNotNull(TEXT("Runtime Head Part spawns"), RuntimeHead);
        TestTrue(
            TEXT("Runtime Head Part attaches on the server"),
            RuntimeHead && PullSlot->AttachPart(RuntimeHead));
        if (RuntimeHead)
        {
            const FTransform ServerHeadRelativeTransform =
                RuntimeHead->GetRootComponent()->GetRelativeTransform();
            RuntimeHead->SetRole(ROLE_SimulatedProxy);
            RuntimeHead->DetachFromActor(
                FDetachmentTransformRules::KeepWorldTransform);
            RuntimeHead->AttachedSlotAddress = PullSlotAddress;
            RuntimeHead->OnRep_AttachmentPhysicsState();
            TestTrue(
                TEXT("Client Head Part remains visible after attachment replication"),
                RuntimeHead->GetPartMesh()->IsVisible()
                    && !RuntimeHead->GetPartMesh()->bHiddenInGame);
            TestEqual(
                TEXT("Client Head Part attaches to the server slot"),
                RuntimeHead->GetRootComponent()->GetAttachParent(),
                static_cast<USceneComponent*>(PullSlot));
            TestTrue(
                TEXT("Client Head Part matches the server-relative transform"),
                RuntimeHead->GetRootComponent()->GetRelativeTransform().Equals(
                    ServerHeadRelativeTransform));
            RuntimeHead->SetRole(ROLE_Authority);
            PullSlot->DetachPart();
            RuntimeHead->Destroy();
        }

        ACMDroppedPartActor* RuntimeDrop =
            World->SpawnActor<ACMDroppedPartActor>();
        TestNotNull(
            TEXT("Runtime severed Part pickup spawns"),
            RuntimeDrop);
        if (RuntimeDrop)
        {
            RuntimeDrop->InitializeDroppedPart(
                ECMBodyPart::ArmLeft,
                RuntimeArmClass,
                RuntimeTargetMesh->GetSkeletalMeshAsset(),
                RuntimeTargetMesh->GetPhysicsAsset(),
                TEXT("Ragdoll"),
                FVector::ZeroVector);
            TestTrue(TEXT("Dropped arm is always highlighted"),
                RuntimeDrop->InteractionHighlight->IsHighlighted());
            RuntimeDrop->SetRole(ROLE_SimulatedProxy);
            RuntimeDrop->OnRep_VisualDefinition();
            const FVector ReplicatedDropCenter(-225.0f, 410.0f, 90.0f);
            RuntimeDrop->AuthoritativePickupLocation =
                ReplicatedDropCenter;
            RuntimeDrop->OnRep_AuthoritativePickupLocation();
            RuntimeDrop->GetPartMesh()->UpdateBounds();
            TestFalse(
                TEXT("Client never simulates severed Part pickup physics"),
                RuntimeDrop->GetPartMesh()->IsSimulatingPhysics());
            TestTrue(
                TEXT("Severed Part visual and gameplay share the server point"),
                RuntimeDrop->GetPartMesh()->Bounds.Origin.Equals(
                    RuntimeDrop->GetAuthoritativePickupLocation(),
                    1.0f));
        }
    }

    if (RuntimeTentacle && RuntimeArmClass)
    {
        ACMArmPart* DestroyedTarget = World->SpawnActor<ACMArmPart>(
            RuntimeArmClass,
            RuntimeTentacle->GetActorLocation() + FVector(250.0f, 0.0f, 0.0f),
            FRotator::ZeroRotator);
        TestNotNull(TEXT("Disposable tentacle target spawns"), DestroyedTarget);
        if (DestroyedTarget)
        {
            RuntimeTentacle->SetTetheredActor(DestroyedTarget);
            TestTrue(TEXT("Tethered target enters pending destruction"),
                DestroyedTarget->Destroy());
            RuntimeTentacle->SetTetheredActor(nullptr);
            TestNull(TEXT("Destroyed target can be cleared safely"),
                RuntimeTentacle->GetTetheredActor());
        }
    }

    if (Chimera
        && InitialPresentations.Num() == CMControl::MaxSegments
        && !InitialPresentations.Contains(nullptr))
    {
        Chimera->SetActiveSegmentCountForPlayers(1);
        TestEqual(
            TEXT("One player activates two segments"),
            Chimera->GetActiveSegmentCount(),
            2);
        for (int32 SegmentIndex = 0;
            SegmentIndex < CMControl::MaxSegments;
            ++SegmentIndex)
        {
            ACMChimeraBodySegmentActor* Presentation =
                Chimera->GetBodySegmentPresentation(SegmentIndex);
            TestEqual(
                *FString::Printf(
                    TEXT("Segment %d presentation survives shrink"),
                    SegmentIndex),
                Presentation,
                InitialPresentations[SegmentIndex]);
            TestEqual(
                *FString::Printf(
                    TEXT("Segment %d active state follows one-player layout"),
                    SegmentIndex),
                Presentation && Presentation->IsSegmentActive(),
                SegmentIndex < 2);
        }
        TestEqual(
            TEXT("One-player layout begins with Head"),
            InitialPresentations[0]->GetVisualRole(),
            ECMChimeraSegmentVisualRole::Head);
        TestEqual(
            TEXT("One-player layout ends with Tail"),
            InitialPresentations[1]->GetVisualRole(),
            ECMChimeraSegmentVisualRole::Tail);

        Chimera->SetActiveSegmentCountForPlayers(2);
        TestEqual(
            TEXT("Two players reactivate four segments"),
            Chimera->GetActiveSegmentCount(),
            4);
        TestEqual(
            TEXT("Previous Tail changes to Body without replacement"),
            InitialPresentations[1]->GetVisualRole(),
            ECMChimeraSegmentVisualRole::Body);
        TestEqual(
            TEXT("Reactivated final segment becomes Tail"),
            InitialPresentations[3]->GetVisualRole(),
            ECMChimeraSegmentVisualRole::Tail);
    }

    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
