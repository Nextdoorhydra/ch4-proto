#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/CMChimera.h"
#include "Player/CMChimeraTrailComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMChimeraTrailStampPlanTest,
    "Chimera.VFX.Trail.StampPlan",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMChimeraTrailStampPlanTest::RunTest(const FString& Parameters)
{
    const CMChimeraTrail::FStampPlan Plan =
        CMChimeraTrail::BuildStampPlan(20.0f, 100.0f, 50.0f, 8);
    TestEqual(TEXT("Accumulated movement emits two stamps"),
        Plan.StampCount, 2);
    TestEqual(TEXT("First stamp consumes the remaining interval"),
        Plan.FirstStampDistance, 30.0f);
    TestEqual(TEXT("Unused movement carries into the next frame"),
        Plan.CarriedDistance, 20.0f);

    const CMChimeraTrail::FStampPlan CappedPlan =
        CMChimeraTrail::BuildStampPlan(0.0f, 260.0f, 50.0f, 3);
    TestEqual(TEXT("Per-frame stamp count is capped"),
        CappedPlan.StampCount, 3);
    TestEqual(TEXT("A capped plan keeps the newest trail samples"),
        CappedPlan.FirstStampDistance, 150.0f);
    TestEqual(TEXT("A capped plan keeps only interval remainder"),
        CappedPlan.CarriedDistance, 10.0f);

    TestTrue(TEXT("Large movement is classified as teleport"),
        CMChimeraTrail::IsTeleport(501.0f, 500.0f));
    TestFalse(TEXT("Threshold movement remains paintable"),
        CMChimeraTrail::IsTeleport(500.0f, 500.0f));

    const FVector SurfaceNormal = FVector(0.0f, 0.6f, 0.8f);
    const FVector MovementDirection = FVector::ForwardVector;
    const FQuat DecalRotation = CMChimeraTrail::BuildDecalRotation(
        SurfaceNormal,
        MovementDirection);
    TestTrue(TEXT("Decal projection X axis follows the surface normal"),
        FVector::DotProduct(
            DecalRotation.GetAxisX(),
            SurfaceNormal.GetSafeNormal()) > 0.999f);
    TestTrue(TEXT("Decal texture Z axis follows the surface tangent"),
        FVector::DotProduct(
            DecalRotation.GetAxisZ(),
            FVector::VectorPlaneProject(
                MovementDirection,
                SurfaceNormal).GetSafeNormal()) > 0.999f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMChimeraTrailOwnershipTest,
    "Chimera.VFX.Trail.Ownership",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMChimeraTrailOwnershipTest::RunTest(const FString& Parameters)
{
    const ACMChimera* Defaults = GetDefault<ACMChimera>();
    const UCMChimeraTrailComponent* Trail = Defaults
        ? Defaults->FindComponentByClass<UCMChimeraTrailComponent>()
        : nullptr;
    TestNotNull(TEXT("Every Chimera owns a trail component"), Trail);
    TestFalse(TEXT("Trail component is local cosmetic"),
        Trail && Trail->GetIsReplicated());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMChimeraTrailRuntimeTest,
    "Chimera.VFX.Trail.RuntimePool",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter)

bool FCMChimeraTrailRuntimeTest::RunTest(const FString& Parameters)
{
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
    TestNotNull(TEXT("Trail runtime world is created"), World);
    if (!World)
    {
        return false;
    }

    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(
        EWorldType::Game);
    WorldContext.SetCurrentWorld(World);

    AActor* Ground = World->SpawnActor<AActor>();
    UBoxComponent* GroundCollision = Ground
        ? NewObject<UBoxComponent>(Ground, TEXT("TrailTestGround"))
        : nullptr;
    if (Ground && GroundCollision)
    {
        Ground->SetRootComponent(GroundCollision);
        Ground->AddInstanceComponent(GroundCollision);
        GroundCollision->SetBoxExtent(FVector(1000.0f, 1000.0f, 10.0f));
        GroundCollision->SetCollisionProfileName(TEXT("BlockAll"));
        GroundCollision->RegisterComponent();
        Ground->SetActorLocation(FVector(0.0f, 0.0f, -10.0f));
    }

    AActor* TrailOwner = World->SpawnActor<AActor>();
    USceneComponent* Root = TrailOwner
        ? NewObject<USceneComponent>(TrailOwner, TEXT("TrailTestRoot"))
        : nullptr;
    UCMChimeraTrailComponent* Trail = TrailOwner
        ? NewObject<UCMChimeraTrailComponent>(
            TrailOwner,
            TEXT("TrailTestComponent"))
        : nullptr;
    TestNotNull(TEXT("Trail test ground is created"), GroundCollision);
    TestNotNull(TEXT("Trail test owner is created"), TrailOwner);
    TestNotNull(TEXT("Trail runtime component is created"), Trail);
    if (TrailOwner && Root && Trail)
    {
        TrailOwner->SetRootComponent(Root);
        TrailOwner->AddInstanceComponent(Root);
        Root->RegisterComponent();
        TrailOwner->SetActorLocation(FVector(0.0f, 0.0f, 50.0f));
        TrailOwner->AddInstanceComponent(Trail);
        Trail->RegisterComponent();
    }

    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    if (TrailOwner && !TrailOwner->HasActorBegunPlay())
    {
        TrailOwner->DispatchBeginPlay();
    }

    if (TrailOwner && Trail)
    {
        TestEqual(TEXT("Trail allocates its one-minute decal pool"),
            Trail->GetPoolSize(), 8192);
        TestEqual(TEXT("Trail lifetime is one minute"),
            Trail->Lifetime, 60.0f);
        TestEqual(TEXT("Trail fades slowly over its final twenty seconds"),
            Trail->FadeDuration, 20.0f);
        TestEqual(TEXT("Trail width is one quarter of the initial size"),
            Trail->TrailWidth, 37.5f);
        TestEqual(TEXT("Trail length preserves the square brush shape"),
            Trail->StampLength, 37.5f);
        TestEqual(TEXT("Trail brush keeps a square texture aspect ratio"),
            Trail->StampLength, Trail->TrailWidth);
        TestEqual(TEXT("Trail uses normalized material opacity"),
            Trail->TrailOpacity, 1.0f);
        TestEqual(TEXT("Requested splat texture is the trail brush"),
            GetPathNameSafe(Trail->BrushTexture),
            FString(TEXT("/Game/TPBDMat/Textures/"
                "T_splat0_wall_v2.T_splat0_wall_v2")));

        TestEqual(TEXT("Trail uses its dedicated decal material"),
            GetPathNameSafe(Trail->TrailMaterial),
            FString(TEXT("/Game/Chimera/Character/Chimera/Materials/"
                "M_CMChimeraTrailDecal.M_CMChimeraTrailDecal")));

        TrailOwner->SetActorLocation(FVector(100.0f, 0.0f, 50.0f));
        World->Tick(LEVELTICK_All, 1.0f / 60.0f);
        TestEqual(TEXT("100cm movement paints two overlapping stamps"),
            Trail->GetActiveStampCount(), 2);

        UMaterialInstanceDynamic* FirstStampMaterial =
            Trail->DecalMaterials.IsValidIndex(0)
                ? Trail->DecalMaterials[0].Get()
                : nullptr;
        UTexture* FloorBrush = nullptr;
        const bool bHasFloorBrush = FirstStampMaterial
            && FirstStampMaterial->GetTextureParameterValue(
                FMaterialParameterInfo(TEXT("BrushTexture")),
                FloorBrush);
        TestTrue(TEXT("Ground projection receives the requested brush"),
            bHasFloorBrush && FloorBrush == Trail->BrushTexture);

        float FullOpacity = 0.0f;
        const bool bHasFullOpacity = FirstStampMaterial
            && FirstStampMaterial->GetScalarParameterValue(
                FMaterialParameterInfo(TEXT("Opacity")),
                FullOpacity);
        TestTrue(TEXT("New trail stamp starts fully visible"),
            bHasFullOpacity
                && FMath::IsNearlyEqual(FullOpacity, 1.0f));

        if (FirstStampMaterial
            && Trail->ExpirationTimes.IsValidIndex(0))
        {
            Trail->UpdateExpiredStamps(
                Trail->ExpirationTimes[0]
                    - Trail->FadeDuration * 0.5f);
            float HalfFadeOpacity = 0.0f;
            const bool bHasOpacity =
                FirstStampMaterial->GetScalarParameterValue(
                    FMaterialParameterInfo(TEXT("Opacity")),
                    HalfFadeOpacity);
            TestTrue(TEXT("Halfway through fade halves opacity"),
                bHasOpacity
                    && FMath::IsNearlyEqual(HalfFadeOpacity, 0.5f));
        }

        Trail->SetEffectActive(false);
        TrailOwner->SetActorLocation(FVector(200.0f, 0.0f, 50.0f));
        World->Tick(LEVELTICK_All, 1.0f / 60.0f);
        TestEqual(TEXT("Inactive trail stops creating stamps"),
            Trail->GetActiveStampCount(), 2);
    }

    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
