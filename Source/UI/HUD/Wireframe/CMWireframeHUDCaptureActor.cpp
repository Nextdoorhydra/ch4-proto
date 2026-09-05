#include "HUD/Wireframe/CMWireframeHUDCaptureActor.h"

#include "Camera/CameraTypes.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"
#include "SceneView.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraWireframeHUD, Log, All);

namespace
{
const FName WireColorParameter(TEXT("Color"));
constexpr float CaptureDistance = 2000.0f;
}

ACMWireframeHUDCaptureActor::ACMWireframeHUDCaptureActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    SetReplicates(false);
    SetActorEnableCollision(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(
        TEXT("WireframeSceneCapture"));
    SceneCapture->SetupAttachment(SceneRoot);
    SceneCapture->ProjectionType = ECameraProjectionMode::Orthographic;
    SceneCapture->OrthoWidth = SmoothedOrthoWidth;
    SceneCapture->PrimitiveRenderMode =
        ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    // SceneColorHDR writes inverse opacity to alpha. The HUD paint pass flips
    // it so empty pixels remain transparent without a black capture quad.
    SceneCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
    SceneCapture->bCaptureEveryFrame = false;
    SceneCapture->bCaptureOnMovement = false;
    SceneCapture->bAlwaysPersistRenderingState = true;

    SceneCapture->ShowFlags.SetAtmosphere(false);
    SceneCapture->ShowFlags.SetBloom(false);
    SceneCapture->ShowFlags.SetDecals(false);
    SceneCapture->ShowFlags.SetFog(false);
    SceneCapture->ShowFlags.SetLighting(false);
    SceneCapture->ShowFlags.SetMotionBlur(false);
    SceneCapture->ShowFlags.SetParticles(false);
    SceneCapture->ShowFlags.SetPostProcessing(false);
    SceneCapture->ShowFlags.SetSkyLighting(false);
    SceneCapture->ShowFlags.SetTranslucency(true);
}

void ACMWireframeHUDCaptureActor::Initialize(
    ACMChimera* InChimera,
    UCurveLinearColor* InHealthColorCurve,
    const FRotator& InCaptureRotation,
    int32 RenderTargetSize
)
{
    Chimera = InChimera;
    HealthColorCurve = InHealthColorCurve;
    CaptureRotation = InCaptureRotation;

    const int32 ClampedSize = FMath::Clamp(RenderTargetSize, 128, 1024);
    RenderTarget = NewObject<UTextureRenderTarget2D>(this);
    RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
    RenderTarget->ClearColor = FLinearColor::Transparent;
    RenderTarget->bAutoGenerateMips = false;
    RenderTarget->InitAutoFormat(ClampedSize, ClampedSize);
    RenderTarget->UpdateResourceImmediate(true);
    SceneCapture->TextureTarget = RenderTarget;

    if (!GEngine || !GEngine->WireframeMaterial)
    {
        UE_LOG(LogChimeraWireframeHUD, Error,
            TEXT("Wireframe HUD is missing its engine wireframe material."));
    }

    PartProxies.SetNum(CMControl::MaxPartSlots);
    SynchronizeBodyProxies();
    SynchronizePartProxies();
    UpdateCaptureView(0.0f);
    SceneCapture->CaptureSceneDeferred();
    UE_LOG(LogChimeraWireframeHUD, Log,
        TEXT("Wireframe HUD capture initialized. Chimera=%s Segments=%d BodyVisuals=%d RT=%dx%d"),
        *GetNameSafe(InChimera),
        InChimera ? InChimera->GetActiveSegmentCount() : 0,
        BodyProxies.Num(),
        RenderTarget->SizeX,
        RenderTarget->SizeY);
}

void ACMWireframeHUDCaptureActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!Chimera.IsValid() || !RenderTarget)
    {
        return;
    }

    SynchronizeBodyProxies();
    SynchronizePartProxies();
    UpdateCaptureView(DeltaSeconds);
    SceneCapture->CaptureSceneDeferred();
}

void ACMWireframeHUDCaptureActor::SetCameraView(
    const FRotator& InRotation,
    float InZoom
)
{
    CaptureRotation = InRotation;
    CaptureZoom = FMath::Max(InZoom, 0.01f);
}

void ACMWireframeHUDCaptureActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (SceneCapture)
    {
        SceneCapture->ClearShowOnlyComponents();
        SceneCapture->TextureTarget = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

void ACMWireframeHUDCaptureActor::SynchronizeBodyProxies()
{
    ACMChimera* CurrentChimera = Chimera.Get();
    if (!CurrentChimera || !SceneCapture)
    {
        return;
    }

    const int32 ActiveCount = CurrentChimera->GetActiveSegmentCount();
    const TArray<FCMBodySegmentHealthState> HealthStates =
        CurrentChimera->GetSegmentHealthStates();

    TInlineComponentArray<USkeletalMeshComponent*> SourceMeshes(
        CurrentChimera);
    TSet<USkeletalMeshComponent*> ActiveSourceMeshes;
    for (USkeletalMeshComponent* SourceMesh : SourceMeshes)
    {
        if (!SourceMesh || !SourceMesh->GetSkeletalMeshAsset())
        {
            continue;
        }

        int32 SegmentIndex = INDEX_NONE;
        for (USceneComponent* Parent = SourceMesh->GetAttachParent();
            Parent && SegmentIndex == INDEX_NONE;
            Parent = Parent->GetAttachParent())
        {
            for (int32 CandidateIndex = 0;
                CandidateIndex < CMControl::MaxSegments;
                ++CandidateIndex)
            {
                if (Parent == CurrentChimera->GetBodySegmentComponent(
                        CandidateIndex))
                {
                    SegmentIndex = CandidateIndex;
                    break;
                }
            }
        }
        if (SegmentIndex == INDEX_NONE || SegmentIndex >= ActiveCount)
        {
            continue;
        }

        ActiveSourceMeshes.Add(SourceMesh);
        FBodyProxy* Proxy = BodyProxies.FindByPredicate(
            [SourceMesh](const FBodyProxy& Candidate)
            {
                return Candidate.SourceMesh.Get() == SourceMesh;
            });
        if (!Proxy)
        {
            Proxy = &BodyProxies.AddDefaulted_GetRef();
            Proxy->SegmentIndex = SegmentIndex;
            Proxy->SourceMesh = SourceMesh;
            Proxy->Mesh = NewObject<USkeletalMeshComponent>(
                this,
                *FString::Printf(TEXT("WireBody_%d_%d"),
                    SegmentIndex, BodyProxies.Num()));
            Proxy->Mesh->SetSkeletalMeshAsset(
                SourceMesh->GetSkeletalMeshAsset());
            Proxy->Mesh->RegisterComponent();
            ConfigureProxy(*Proxy->Mesh);
            Proxy->Mesh->SetForceWireframe(true);
            Proxy->Mesh->SetLeaderPoseComponent(SourceMesh, true, false);
            Proxy->Material = CreateWireframeMaterial(Proxy->Mesh);
            if (Proxy->Material)
            {
                const int32 MaterialCount =
                    FMath::Max(Proxy->Mesh->GetNumMaterials(), 1);
                for (int32 MaterialIndex = 0;
                    MaterialIndex < MaterialCount;
                    ++MaterialIndex)
                {
                    Proxy->Mesh->SetMaterial(MaterialIndex, Proxy->Material);
                }
            }
            SceneCapture->ShowOnlyComponent(Proxy->Mesh);
        }

        Proxy->SegmentIndex = SegmentIndex;
        Proxy->Mesh->SetVisibility(SourceMesh->IsVisible());
        Proxy->Mesh->SetWorldTransform(SourceMesh->GetComponentTransform());

        const FCMBodySegmentHealthState* State =
            HealthStates.IsValidIndex(SegmentIndex)
                ? &HealthStates[SegmentIndex]
                : nullptr;
        if (Proxy->Material)
        {
            Proxy->Material->SetVectorParameterValue(
                WireColorParameter,
                EvaluateHealthColor(
                    State ? State->Health : 0.0f,
                    State ? State->MaxHealth : 0.0f,
                    State && State->bDead));
        }
    }

    for (int32 ProxyIndex = BodyProxies.Num() - 1;
        ProxyIndex >= 0;
        --ProxyIndex)
    {
        if (!ActiveSourceMeshes.Contains(
                BodyProxies[ProxyIndex].SourceMesh.Get()))
        {
            RemoveBodyProxy(ProxyIndex);
        }
    }
}

void ACMWireframeHUDCaptureActor::SynchronizePartProxies()
{
    ACMChimera* CurrentChimera = Chimera.Get();
    if (!CurrentChimera || !SceneCapture)
    {
        return;
    }

    const int32 ActiveSegmentCount = CurrentChimera->GetActiveSegmentCount();
    for (int32 FlatSlotIndex = 0;
        FlatSlotIndex < PartProxies.Num();
        ++FlatSlotIndex)
    {
        const FCMPartSlotAddress Address =
            CMControl::FromFlatPartSlotIndex(FlatSlotIndex);
        UCMPartSlotComponent* Slot =
            CMControl::IsValidPartSlot(Address, ActiveSegmentCount)
                ? CurrentChimera->GetPartSlotComponent(Address)
                : nullptr;
        ACMPartActorBase* Part = Slot
            ? Cast<ACMPartActorBase>(Slot->GetAttachedPart())
            : nullptr;
        USkeletalMeshComponent* SourceMesh = Part
            ? Part->GetPartMesh()
            : nullptr;
        if (!Part || !Part->IsAlive() || !SourceMesh
            || !SourceMesh->GetSkeletalMeshAsset())
        {
            RemovePartProxy(FlatSlotIndex);
            continue;
        }

        FPartProxy& Proxy = PartProxies[FlatSlotIndex];
        if (Proxy.SourcePart.Get() != Part || !Proxy.Mesh
            || Proxy.Mesh->GetSkeletalMeshAsset()
                != SourceMesh->GetSkeletalMeshAsset())
        {
            RemovePartProxy(FlatSlotIndex);
            Proxy.Address = Address;
            Proxy.SourcePart = Part;
            Proxy.Mesh = NewObject<USkeletalMeshComponent>(
                this,
                *FString::Printf(TEXT("WirePart_%d"), FlatSlotIndex));
            Proxy.Mesh->SetSkeletalMeshAsset(
                SourceMesh->GetSkeletalMeshAsset());
            Proxy.Mesh->RegisterComponent();
            ConfigureProxy(*Proxy.Mesh);
            Proxy.Mesh->SetForceWireframe(true);
            Proxy.Mesh->SetLeaderPoseComponent(SourceMesh, true, false);
            Proxy.Material = CreateWireframeMaterial(Proxy.Mesh);
            if (Proxy.Material)
            {
                const int32 MaterialCount =
                    FMath::Max(Proxy.Mesh->GetNumMaterials(), 1);
                for (int32 MaterialIndex = 0;
                    MaterialIndex < MaterialCount;
                    ++MaterialIndex)
                {
                    Proxy.Mesh->SetMaterial(MaterialIndex, Proxy.Material);
                }
            }
            SceneCapture->ShowOnlyComponent(Proxy.Mesh);
        }

        Proxy.Mesh->SetVisibility(true);
        Proxy.Mesh->SetWorldTransform(SourceMesh->GetComponentTransform());
        if (Proxy.Material)
        {
            Proxy.Material->SetVectorParameterValue(
                WireColorParameter,
                EvaluateHealthColor(
                    Part->GetHealth(), Part->GetMaxHealth(), false));
        }
    }
}

void ACMWireframeHUDCaptureActor::UpdateCaptureView(float DeltaSeconds)
{
    ACMChimera* CurrentChimera = Chimera.Get();
    if (!CurrentChimera || !SceneCapture)
    {
        return;
    }

    FBox Bounds(ForceInit);
    for (const FBodyProxy& Proxy : BodyProxies)
    {
        if (Proxy.Mesh && Proxy.Mesh->IsVisible())
        {
            Bounds += Proxy.Mesh->Bounds.GetBox();
        }
    }
    for (const FPartProxy& Proxy : PartProxies)
    {
        if (Proxy.Mesh && Proxy.Mesh->IsVisible())
        {
            Bounds += Proxy.Mesh->Bounds.GetBox();
        }
    }
    if (!Bounds.IsValid)
    {
        return;
    }

    const FVector TargetCenter = Bounds.GetCenter();
    const float TargetWidth = FMath::Max(
        Bounds.GetExtent().GetMax() * 2.5f,
        300.0f) * CaptureZoom;
    if (!bViewInitialized || DeltaSeconds <= 0.0f)
    {
        SmoothedViewCenter = TargetCenter;
        SmoothedOrthoWidth = TargetWidth;
        bViewInitialized = true;
    }
    else
    {
        SmoothedViewCenter = FMath::VInterpTo(
            SmoothedViewCenter, TargetCenter, DeltaSeconds, 8.0f);
        SmoothedOrthoWidth = FMath::FInterpTo(
            SmoothedOrthoWidth, TargetWidth, DeltaSeconds, 6.0f);
    }

    const FVector ViewDirection = CaptureRotation.Vector();
    SceneCapture->SetWorldLocationAndRotation(
        SmoothedViewCenter - ViewDirection * CaptureDistance,
        CaptureRotation);
    SceneCapture->OrthoWidth = SmoothedOrthoWidth;
}

void ACMWireframeHUDCaptureActor::ConfigureProxy(
    UPrimitiveComponent& Proxy
) const
{
    Proxy.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Proxy.SetCastShadow(false);
    Proxy.SetReceivesDecals(false);
    Proxy.SetVisibleInSceneCaptureOnly(true);
    Proxy.SetCanEverAffectNavigation(false);
    Proxy.SetTranslucentSortPriority(1);
}

UMaterialInstanceDynamic*
ACMWireframeHUDCaptureActor::CreateWireframeMaterial(UObject* Outer) const
{
    return GEngine && GEngine->WireframeMaterial
        ? UMaterialInstanceDynamic::Create(
            GEngine->WireframeMaterial,
            Outer)
        : nullptr;
}

FLinearColor ACMWireframeHUDCaptureActor::EvaluateHealthColor(
    float Health,
    float MaxHealth,
    bool bDestroyedBody
) const
{
    if (bDestroyedBody)
    {
        return FLinearColor::Black;
    }

    const float NormalizedHealth = MaxHealth > 0.0f
        ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f)
        : 0.0f;
    if (HealthColorCurve)
    {
        return HealthColorCurve->GetLinearColorValue(NormalizedHealth);
    }

    const FLinearColor DarkRed(0.18f, 0.005f, 0.005f);
    const FLinearColor Red(1.0f, 0.025f, 0.01f);
    const FLinearColor Green(0.025f, 1.0f, 0.06f);
    return NormalizedHealth < 0.5f
        ? FMath::Lerp(DarkRed, Red, NormalizedHealth * 2.0f)
        : FMath::Lerp(Red, Green, (NormalizedHealth - 0.5f) * 2.0f);
}

void ACMWireframeHUDCaptureActor::RemovePartProxy(int32 FlatSlotIndex)
{
    if (!PartProxies.IsValidIndex(FlatSlotIndex))
    {
        return;
    }

    FPartProxy& Proxy = PartProxies[FlatSlotIndex];
    if (Proxy.Mesh)
    {
        SceneCapture->RemoveShowOnlyComponent(Proxy.Mesh);
        Proxy.Mesh->SetLeaderPoseComponent(nullptr);
        Proxy.Mesh->DestroyComponent();
    }
    Proxy = FPartProxy();
}

void ACMWireframeHUDCaptureActor::RemoveBodyProxy(int32 ProxyIndex)
{
    if (!BodyProxies.IsValidIndex(ProxyIndex))
    {
        return;
    }

    FBodyProxy& Proxy = BodyProxies[ProxyIndex];
    if (Proxy.Mesh)
    {
        SceneCapture->RemoveShowOnlyComponent(Proxy.Mesh);
        Proxy.Mesh->SetLeaderPoseComponent(nullptr);
        Proxy.Mesh->DestroyComponent();
    }
    BodyProxies.RemoveAtSwap(ProxyIndex);
}

bool ACMWireframeHUDCaptureActor::ProjectWorldLocation(
    const FVector& WorldLocation,
    FVector2D& OutNormalizedPosition
) const
{
    if (!SceneCapture || !RenderTarget)
    {
        return false;
    }

    FMinimalViewInfo ViewInfo;
    ViewInfo.Location = SceneCapture->GetComponentLocation();
    ViewInfo.Rotation = SceneCapture->GetComponentRotation();
    ViewInfo.ProjectionMode = SceneCapture->ProjectionType;
    ViewInfo.OrthoWidth = SceneCapture->OrthoWidth;
    ViewInfo.AspectRatio = static_cast<float>(RenderTarget->SizeX)
        / FMath::Max(RenderTarget->SizeY, 1);
    ViewInfo.bConstrainAspectRatio = true;

    FMatrix ViewMatrix;
    FMatrix ProjectionMatrix;
    FMatrix ViewProjectionMatrix;
    UGameplayStatics::CalculateViewProjectionMatricesFromMinimalView(
        ViewInfo,
        SceneCapture->bUseCustomProjectionMatrix
            ? TOptional<FMatrix>(SceneCapture->CustomProjectionMatrix)
            : TOptional<FMatrix>(),
        ViewMatrix,
        ProjectionMatrix,
        ViewProjectionMatrix);

    FVector2D PixelPosition;
    const FIntRect ViewRect(0, 0, RenderTarget->SizeX, RenderTarget->SizeY);
    if (!FSceneView::ProjectWorldToScreen(
            WorldLocation,
            ViewRect,
            ViewProjectionMatrix,
            PixelPosition,
            true))
    {
        return false;
    }

    OutNormalizedPosition = FVector2D(
        PixelPosition.X / FMath::Max(RenderTarget->SizeX, 1),
        PixelPosition.Y / FMath::Max(RenderTarget->SizeY, 1));
    return true;
}
