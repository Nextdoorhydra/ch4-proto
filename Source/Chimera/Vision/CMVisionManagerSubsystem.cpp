#include "Vision/CMVisionManagerSubsystem.h"

#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Parts/Head/CMVisionComponent.h"
#include "Vision/CMVisionRenderConfig.h"
#include "Vision/CMVisionSettings.h"

namespace
{
    const FName OccluderVisionMaskParameterName(
        TEXT("OccluderVisionMask")
    );
    const FName BaseVisionMaskParameterName(TEXT("BaseVisionMask"));
    const FName VisionTintMaskParameterName(
        TEXT("VisionTintMask")
    );
    const FName VisionMaskWorldCenterParameterName(
        TEXT("VisionMaskWorldCenter")
    );
    const FName VisionMaskWorldSizeParameterName(
        TEXT("VisionMaskWorldSize")
    );
    const FName OccluderStencilValueParameterName(
        TEXT("OccluderStencilValue")
    );
    const FName VisionTintColorParameterName(TEXT("VisionTintColor"));
    const FName VisionTintStrengthParameterName(
        TEXT("VisionTintStrength")
    );
    const FName VisionMaskWorldMinHeightParameterName(
        TEXT("VisionMaskWorldMinHeight")
    );
    const FName VisionMaskWorldHeightRangeParameterName(
        TEXT("VisionMaskWorldHeightRange")
    );
    const FName CeilingSurfaceNormalZThresholdParameterName(
        TEXT("CeilingSurfaceNormalZThreshold")
    );
    const FName VisionHeightToleranceParameterName(
        TEXT("VisionHeightTolerance")
    );

#if !UE_BUILD_SHIPPING
    TAutoConsoleVariable<int32> CVarVisionDebugDraw(
        TEXT("CM.Vision.DebugDraw"),
        0,
        TEXT("Draws slot vision origins (green), component locations (red), and aim rays (cyan)."),
        ECVF_Cheat
    );
#endif
}

DEFINE_LOG_CATEGORY_STATIC(LogChimeraVisionManager, Log, All);

void UCMVisionManagerSubsystem::Initialize(
    FSubsystemCollectionBase& Collection
)
{
    Super::Initialize(Collection);
    LoadRenderConfig();
}

void UCMVisionManagerSubsystem::Deinitialize()
{
    RestoreOccluderRenderStates();
    RemovePostProcessBinding();
    PostProcessMaterialAsset = nullptr;
    RenderConfig = nullptr;
    OccluderVisibilityMask = nullptr;
    BaseVisibilityMask = nullptr;
    VisionTintMask = nullptr;
    MaskDrawTexture = nullptr;
    VisionSources.Reset();
    CachedVisionMaskData.Reset();
    Super::Deinitialize();
}

void UCMVisionManagerSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (!bVisionSystemEnabled)
    {
        RemovePostProcessBinding();
        RestoreOccluderRenderStates();
        return;
    }

    if (!RenderConfig && !LoadRenderConfig())
    {
        return;
    }

    VisionSources.RemoveAllSwap([](
        const TWeakObjectPtr<UCMVisionComponent>& Source)
    {
        return !Source.IsValid();
    });

    EnsureVisibilityMask();
    EnsurePostProcessBinding();
    if (!OccluderVisibilityMask || !BaseVisibilityMask || !VisionTintMask)
    {
        return;
    }

    TimeUntilMaskUpdate -= DeltaTime;
    if (TimeUntilMaskUpdate > 0.0f)
    {
        return;
    }

    TArray<UCMVisionComponent*> ActiveSources;
    GetActiveVisionSources(ActiveSources);
    UpdateVisibilityMaskBounds(ActiveSources);
    BuildVisionRayCache(ActiveSources);

    if (PostProcessMaterialInstance)
    {
        PostProcessMaterialInstance->SetVectorParameterValue(
            VisionMaskWorldCenterParameterName,
            FLinearColor(
                MaskWorldCenter.X,
                MaskWorldCenter.Y,
                0.0f,
                0.0f
            )
        );
        PostProcessMaterialInstance->SetScalarParameterValue(
            VisionMaskWorldSizeParameterName,
            MaskWorldHalfExtent * 2.0f
        );
        PostProcessMaterialInstance->SetScalarParameterValue(
            VisionMaskWorldMinHeightParameterName,
            MaskWorldMinHeight
        );
        PostProcessMaterialInstance->SetScalarParameterValue(
            VisionMaskWorldHeightRangeParameterName,
            MaskWorldHeightRange
        );
    }

#if !UE_BUILD_SHIPPING
    if (CVarVisionDebugDraw.GetValueOnGameThread() != 0)
    {
        for (const UCMVisionComponent* VisionSource : ActiveSources)
        {
            const FVector Origin = VisionSource->GetVisionOrigin();
            DrawDebugSphere(
                World,
                Origin,
                18.0f,
                12,
                FColor::Green,
                false,
                RenderConfig->MaskUpdateInterval
            );
            DrawDebugSphere(
                World,
                VisionSource->GetComponentLocation(),
                12.0f,
                12,
                FColor::Red,
                false,
                RenderConfig->MaskUpdateInterval
            );
            DrawDebugLine(
                World,
                Origin,
                Origin + VisionSource->GetRenderedAimDirection()
                    * VisionSource->GetVisionDistance(),
                FColor::Cyan,
                false,
                RenderConfig->MaskUpdateInterval,
                0,
                2.0f
            );
        }
    }
#endif

    BaseVisibilityMask->UpdateResource();
    OccluderVisibilityMask->UpdateResource();
    VisionTintMask->UpdateResource();
    TimeUntilMaskUpdate = FMath::Max(
        RenderConfig->MaskUpdateInterval,
        0.01f
    );

}

bool UCMVisionManagerSubsystem::LoadRenderConfig()
{
    const UCMVisionSettings* Settings = GetDefault<UCMVisionSettings>();
    RenderConfig = Settings
        ? Settings->DefaultRenderConfig.LoadSynchronous()
        : nullptr;
    PostProcessMaterialAsset = RenderConfig
        ? RenderConfig->PostProcessMaterial.LoadSynchronous()
        : nullptr;

    if (RenderConfig && PostProcessMaterialAsset)
    {
        bConfigurationFailureLogged = false;
        MaskWorldHalfExtent = FMath::Max(
            RenderConfig->MinimumWorldHalfExtent,
            1.0f
        );
        return true;
    }

    if (!bConfigurationFailureLogged)
    {
        UE_LOG(LogChimeraVisionManager, Error,
            TEXT("Vision rendering is disabled. Assign a Vision Render Config with a Post Process Material in Project Settings > Chimera > Vision."));
        bConfigurationFailureLogged = true;
    }
    return false;
}

TStatId UCMVisionManagerSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(
        UCMVisionManagerSubsystem,
        STATGROUP_Tickables
    );
}

void UCMVisionManagerSubsystem::RegisterVisionSource(
    UCMVisionComponent* VisionComponent
)
{
    if (VisionComponent)
    {
        VisionSources.AddUnique(VisionComponent);
    }
}

void UCMVisionManagerSubsystem::UnregisterVisionSource(
    UCMVisionComponent* VisionComponent
)
{
    VisionSources.Remove(VisionComponent);
}

bool UCMVisionManagerSubsystem::IsLocationVisible(
    const FVector& WorldLocation
) const
{
    if (!bVisionSystemEnabled)
    {
        return true;
    }

    for (const TWeakObjectPtr<UCMVisionComponent>& VisionSource
        : VisionSources)
    {
        if (const UCMVisionComponent* VisionComponent = VisionSource.Get())
        {
            if (VisionComponent->GetVisionContribution()
                    == ECMVisionContribution::RevealAndTint
                && VisionComponent->IsLocationVisible(WorldLocation)
                && HasLineOfSight(*VisionComponent, WorldLocation))
            {
                return true;
            }
        }
    }

    return false;
}

void UCMVisionManagerSubsystem::DisableVisionSystem()
{
    if (!bVisionSystemEnabled)
    {
        return;
    }

    bVisionSystemEnabled = false;
    RemovePostProcessBinding();
    RestoreOccluderRenderStates();
}

void UCMVisionManagerSubsystem::EnableVisionSystem()
{
    if (bVisionSystemEnabled)
    {
        return;
    }

    bVisionSystemEnabled = true;
    TimeUntilMaskUpdate = 0.0f;
}

bool UCMVisionManagerSubsystem::IsVisionSystemEnabled() const
{
    return bVisionSystemEnabled;
}

void UCMVisionManagerSubsystem::GetActiveVisionSources(
    TArray<UCMVisionComponent*>& OutVisionSources
) const
{
    OutVisionSources.Reset();
    for (const TWeakObjectPtr<UCMVisionComponent>& VisionSource
        : VisionSources)
    {
        UCMVisionComponent* VisionComponent = VisionSource.Get();
        if (VisionComponent && VisionComponent->IsVisionActive())
        {
            OutVisionSources.Add(VisionComponent);
        }
    }
}

UCanvasRenderTarget2D*
UCMVisionManagerSubsystem::GetVisibilityMask() const
{
    return BaseVisibilityMask;
}

FVector2D UCMVisionManagerSubsystem::GetVisibilityMaskWorldCenter() const
{
    return MaskWorldCenter;
}

float UCMVisionManagerSubsystem::GetVisibilityMaskWorldHalfExtent() const
{
    return MaskWorldHalfExtent;
}

void UCMVisionManagerSubsystem::EnsureVisibilityMask()
{
    if ((OccluderVisibilityMask && BaseVisibilityMask && VisionTintMask)
        || !RenderConfig)
    {
        return;
    }

    const int32 SafeResolution = FMath::Clamp(
        RenderConfig->MaskResolution,
        64,
        2048
    );

    OccluderVisibilityMask = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(
        this,
        UCanvasRenderTarget2D::StaticClass(),
        SafeResolution,
        SafeResolution
    );
    if (OccluderVisibilityMask)
    {
        OccluderVisibilityMask->Filter = TF_Bilinear;
        OccluderVisibilityMask->AddressX = TA_Clamp;
        OccluderVisibilityMask->AddressY = TA_Clamp;
        OccluderVisibilityMask->ClearColor = FLinearColor::Black;
        OccluderVisibilityMask->SetShouldClearRenderTargetOnReceiveUpdate(true);
        OccluderVisibilityMask->OnCanvasRenderTargetUpdate.AddDynamic(
            this,
            &UCMVisionManagerSubsystem::DrawOccluderVisibilityMask
        );
    }

    BaseVisibilityMask = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(
        this,
        UCanvasRenderTarget2D::StaticClass(),
        SafeResolution,
        SafeResolution
    );
    if (BaseVisibilityMask)
    {
        BaseVisibilityMask->Filter = TF_Bilinear;
        BaseVisibilityMask->AddressX = TA_Clamp;
        BaseVisibilityMask->AddressY = TA_Clamp;
        BaseVisibilityMask->ClearColor = FLinearColor::Black;
        BaseVisibilityMask->SetShouldClearRenderTargetOnReceiveUpdate(true);
        BaseVisibilityMask->OnCanvasRenderTargetUpdate.AddDynamic(
            this,
            &UCMVisionManagerSubsystem::DrawBaseVisibilityMask
        );
    }

    VisionTintMask = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(
        this,
        UCanvasRenderTarget2D::StaticClass(),
        SafeResolution,
        SafeResolution
    );
    if (VisionTintMask)
    {
        VisionTintMask->Filter = TF_Bilinear;
        VisionTintMask->AddressX = TA_Clamp;
        VisionTintMask->AddressY = TA_Clamp;
        VisionTintMask->ClearColor = FLinearColor::Transparent;
        VisionTintMask->SetShouldClearRenderTargetOnReceiveUpdate(true);
        VisionTintMask->OnCanvasRenderTargetUpdate.AddDynamic(
            this,
            &UCMVisionManagerSubsystem::DrawVisionTintMask
        );
    }

    MaskDrawTexture = LoadObject<UTexture2D>(
        nullptr,
        TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")
    );
}

void UCMVisionManagerSubsystem::UpdateVisibilityMaskBounds(
    const TArray<UCMVisionComponent*>& ActiveSources
)
{
    if (ActiveSources.IsEmpty())
    {
        return;
    }

    FVector2D CenterSum = FVector2D::ZeroVector;
    for (const UCMVisionComponent* VisionSource : ActiveSources)
    {
        const FVector Location = VisionSource->GetVisionOrigin();
        CenterSum += FVector2D(Location.X, Location.Y);
    }
    MaskWorldCenter = CenterSum / ActiveSources.Num();

    float MinimumRevealHeight = TNumericLimits<float>::Max();
    float MaximumRevealHeight = TNumericLimits<float>::Lowest();
    for (const UCMVisionComponent* VisionSource : ActiveSources)
    {
        if (VisionSource->GetVisionContribution()
            != ECMVisionContribution::RevealAndTint)
        {
            continue;
        }

        const float Height = VisionSource->GetVisionOrigin().Z;
        MinimumRevealHeight = FMath::Min(MinimumRevealHeight, Height);
        MaximumRevealHeight = FMath::Max(MaximumRevealHeight, Height);
    }
    if (MinimumRevealHeight <= MaximumRevealHeight)
    {
        constexpr float HeightEncodingMargin = 100.0f;
        MaskWorldMinHeight = MinimumRevealHeight - HeightEncodingMargin;
        MaskWorldHeightRange = FMath::Max(
            MaximumRevealHeight - MinimumRevealHeight
                + HeightEncodingMargin * 2.0f,
            1.0f
        );
    }

    float RequiredHalfExtent = FMath::Max(
        RenderConfig->MinimumWorldHalfExtent,
        1.0f
    );
    for (const UCMVisionComponent* VisionSource : ActiveSources)
    {
        const FVector Location = VisionSource->GetVisionOrigin();
        RequiredHalfExtent = FMath::Max(
            RequiredHalfExtent,
            FMath::Max(
                FMath::Abs(Location.X - MaskWorldCenter.X),
                FMath::Abs(Location.Y - MaskWorldCenter.Y)
            ) + FMath::Max(
                VisionSource->GetVisionDistance(),
                VisionSource->GetNearVisionRadius()
            )
        );
    }
    MaskWorldHalfExtent = RequiredHalfExtent * FMath::Max(
        RenderConfig->MaskBoundsPadding,
        1.0f
    );
}

void UCMVisionManagerSubsystem::BuildVisionRayCache(
    const TArray<UCMVisionComponent*>& ActiveSources
)
{
    CachedVisionMaskData.Reset(ActiveSources.Num());

    const int32 ArcSegmentCount = FMath::Max(
        RenderConfig->ArcSegmentCount,
        3
    );
    const int32 NearVisionCircleSegmentCount = FMath::Max(
        RenderConfig->NearVisionCircleSegmentCount,
        8
    );
    const int32 EdgeRefinementSteps = FMath::Clamp(
        RenderConfig->OcclusionEdgeRefinementSteps,
        0,
        8
    );
    const float EdgeRefinementDistance = FMath::Max(
        RenderConfig->OcclusionEdgeRefinementDistance,
        1.0f
    );
    const float RevealDistance = FMath::Max(
        RenderConfig->OccluderSurfaceRevealDistance,
        0.0f
    );

    for (const UCMVisionComponent* VisionSource : ActiveSources)
    {
        FCMVisionSourceMaskData& SourceData =
            CachedVisionMaskData.AddDefaulted_GetRef();
        SourceData.Origin = VisionSource->GetVisionOrigin();
        SourceData.VisionTint = VisionSource->GetVisionTint();
        SourceData.bRevealsWorld =
            VisionSource->GetVisionContribution()
                == ECMVisionContribution::RevealAndTint;

        const auto SampleRay = [
            this,
            VisionSource,
            &SourceData,
            RevealDistance
        ](float Angle, float Distance)
        {
            const FVector DesiredEnd = SourceData.Origin + FVector(
                FMath::Cos(Angle) * Distance,
                FMath::Sin(Angle) * Distance,
                0.0f
            );
            FHitResult Hit;
            FCMVisionRaySample Ray;
            Ray.BaseEnd = ClipVisionRayToOccluder(
                *VisionSource,
                SourceData.Origin,
                DesiredEnd,
                0.0f,
                &Hit
            );
            Ray.RevealedEnd = Ray.BaseEnd;
            Ray.HitComponent = Hit.GetComponent();
            Ray.bBlockingHit = Hit.bBlockingHit;

            if (Hit.bBlockingHit && RevealDistance > 0.0f)
            {
                const FVector RayDirection =
                    (DesiredEnd - SourceData.Origin).GetSafeNormal2D();
                const float RemainingDistance = FVector::Dist2D(
                    Ray.BaseEnd,
                    DesiredEnd
                );
                Ray.RevealedEnd += RayDirection * FMath::Min(
                    RevealDistance,
                    RemainingDistance
                );
            }

            return Ray;
        };

        const auto IsOcclusionEdge = [
            &SourceData,
            EdgeRefinementDistance
        ](const FCMVisionRaySample& A, const FCMVisionRaySample& B)
        {
            if (A.bBlockingHit != B.bBlockingHit)
            {
                return true;
            }
            if (!A.bBlockingHit)
            {
                return false;
            }

            const float DistanceA = FVector::Dist2D(
                SourceData.Origin,
                A.BaseEnd
            );
            const float DistanceB = FVector::Dist2D(
                SourceData.Origin,
                B.BaseEnd
            );
            return FMath::Abs(DistanceA - DistanceB)
                >= EdgeRefinementDistance;
        };

        TFunction<void(
            float,
            const FCMVisionRaySample&,
            float,
            const FCMVisionRaySample&,
            float,
            int32,
            TArray<FCMVisionRaySample>&)> AppendRefinedRange;
        AppendRefinedRange = [
            &SampleRay,
            &IsOcclusionEdge,
            &AppendRefinedRange
        ](
            float AngleA,
            const FCMVisionRaySample& RayA,
            float AngleB,
            const FCMVisionRaySample& RayB,
            float Distance,
            int32 RemainingSteps,
            TArray<FCMVisionRaySample>& OutRays)
        {
            if (RemainingSteps > 0 && IsOcclusionEdge(RayA, RayB))
            {
                const float MiddleAngle = (AngleA + AngleB) * 0.5f;
                const FCMVisionRaySample MiddleRay = SampleRay(
                    MiddleAngle,
                    Distance
                );
                AppendRefinedRange(
                    AngleA,
                    RayA,
                    MiddleAngle,
                    MiddleRay,
                    Distance,
                    RemainingSteps - 1,
                    OutRays
                );
                AppendRefinedRange(
                    MiddleAngle,
                    MiddleRay,
                    AngleB,
                    RayB,
                    Distance,
                    RemainingSteps - 1,
                    OutRays
                );
                return;
            }

            OutRays.Add(RayB);
        };

        const FVector Direction = VisionSource->GetRenderedAimDirection();
        const float Distance = VisionSource->GetVisionDistance();
        const float HalfAngleRadians = FMath::DegreesToRadians(
            VisionSource->GetVisionAngleDegrees() * 0.5f
        );
        const float CenterAngle = FMath::Atan2(
            Direction.Y,
            Direction.X
        );

        const float StartAngle = CenterAngle - HalfAngleRadians;
        const float EndAngle = CenterAngle + HalfAngleRadians;
        SourceData.Rays.Reserve(ArcSegmentCount + 1);
        float PreviousAngle = StartAngle;
        FCMVisionRaySample PreviousRay = SampleRay(
            PreviousAngle,
            Distance
        );
        SourceData.Rays.Add(PreviousRay);
        for (int32 RayIndex = 1;
            RayIndex <= ArcSegmentCount;
            ++RayIndex)
        {
            const float Alpha =
                static_cast<float>(RayIndex) / ArcSegmentCount;
            const float Angle = FMath::Lerp(StartAngle, EndAngle, Alpha);
            const FCMVisionRaySample Ray = SampleRay(Angle, Distance);
            AppendRefinedRange(
                PreviousAngle,
                PreviousRay,
                Angle,
                Ray,
                Distance,
                EdgeRefinementSteps,
                SourceData.Rays
            );
            PreviousAngle = Angle;
            PreviousRay = Ray;
        }

        const float NearVisionRadius =
            VisionSource->GetNearVisionRadius();
        if (NearVisionRadius > 0.0f)
        {
            SourceData.NearVisionRays.Reserve(
                NearVisionCircleSegmentCount + 1
            );
            PreviousAngle = 0.0f;
            PreviousRay = SampleRay(PreviousAngle, NearVisionRadius);
            SourceData.NearVisionRays.Add(PreviousRay);
            for (int32 RayIndex = 1;
                RayIndex <= NearVisionCircleSegmentCount;
                ++RayIndex)
            {
                const float Angle = UE_TWO_PI
                    * static_cast<float>(RayIndex)
                    / NearVisionCircleSegmentCount;
                const FCMVisionRaySample Ray = SampleRay(
                    Angle,
                    NearVisionRadius
                );
                AppendRefinedRange(
                    PreviousAngle,
                    PreviousRay,
                    Angle,
                    Ray,
                    NearVisionRadius,
                    EdgeRefinementSteps,
                    SourceData.NearVisionRays
                );
                PreviousAngle = Angle;
                PreviousRay = Ray;
            }
            SourceData.NearVisionRays.Pop(EAllowShrinking::No);
        }
    }
}

void UCMVisionManagerSubsystem::EnsurePostProcessBinding()
{
    UCameraComponent* Camera = FindViewCamera();
    if (BoundCamera.Get() != Camera)
    {
        RemovePostProcessBinding();
    }

    if (PostProcessMaterialInstance && Camera)
    {
        return;
    }

    if (!Camera || !PostProcessMaterialAsset || !OccluderVisibilityMask
        || !BaseVisibilityMask || !VisionTintMask)
    {
        return;
    }

    PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(
        PostProcessMaterialAsset,
        this
    );
    if (!PostProcessMaterialInstance)
    {
        return;
    }

    PostProcessMaterialInstance->SetTextureParameterValue(
        OccluderVisionMaskParameterName,
        OccluderVisibilityMask
    );
    PostProcessMaterialInstance->SetTextureParameterValue(
        BaseVisionMaskParameterName,
        BaseVisibilityMask
    );
    PostProcessMaterialInstance->SetTextureParameterValue(
        VisionTintMaskParameterName,
        VisionTintMask
    );
    PostProcessMaterialInstance->SetScalarParameterValue(
        OccluderStencilValueParameterName,
        RenderConfig->OccluderStencilValue
    );
    PostProcessMaterialInstance->SetVectorParameterValue(
        VisionTintColorParameterName,
        RenderConfig->VisionTintColor
    );
    PostProcessMaterialInstance->SetScalarParameterValue(
        VisionTintStrengthParameterName,
        RenderConfig->VisionTintStrength
    );
    PostProcessMaterialInstance->SetScalarParameterValue(
        CeilingSurfaceNormalZThresholdParameterName,
        RenderConfig->CeilingSurfaceNormalZThreshold
    );
    PostProcessMaterialInstance->SetScalarParameterValue(
        VisionHeightToleranceParameterName,
        RenderConfig->VisionHeightTolerance
    );

    Camera->PostProcessSettings.AddBlendable(
        PostProcessMaterialInstance,
        RenderConfig->PostProcessBlendWeight
    );
    BoundCamera = Camera;

    UE_LOG(LogChimeraVisionManager, Log,
        TEXT("Bound Vision Post Process to camera %s using config %s."),
        *GetNameSafe(Camera),
        *GetNameSafe(RenderConfig));
}

UCameraComponent* UCMVisionManagerSubsystem::FindViewCamera() const
{
    const UWorld* World = GetWorld();
    const APlayerController* PlayerController = World
        ? World->GetFirstPlayerController()
        : nullptr;
    AActor* ViewTarget = PlayerController
        ? PlayerController->GetViewTarget()
        : nullptr;
    return ViewTarget
        ? ViewTarget->FindComponentByClass<UCameraComponent>()
        : nullptr;
}

void UCMVisionManagerSubsystem::RemovePostProcessBinding()
{
    if (UCameraComponent* Camera = BoundCamera.Get())
    {
        if (PostProcessMaterialInstance)
        {
            Camera->PostProcessSettings.RemoveBlendable(
                PostProcessMaterialInstance
            );
        }

    }

    BoundCamera.Reset();
    PostProcessMaterialInstance = nullptr;
}

void UCMVisionManagerSubsystem::RestoreOccluderRenderStates()
{
    for (const FCMVisionOccluderRenderState& State : OccluderRenderStates)
    {
        if (UPrimitiveComponent* Component = State.Component.Get())
        {
            Component->SetCustomDepthStencilValue(
                State.CustomDepthStencilValue
            );
            Component->SetRenderCustomDepth(State.bRenderCustomDepth);
        }
    }
    OccluderRenderStates.Reset();
}

void UCMVisionManagerSubsystem::UpdateOccluderRenderStates(
    const TSet<UPrimitiveComponent*>& CurrentOccluders
)
{
    for (int32 Index = OccluderRenderStates.Num() - 1; Index >= 0; --Index)
    {
        FCMVisionOccluderRenderState& State = OccluderRenderStates[Index];
        UPrimitiveComponent* Component = State.Component.Get();
        if (Component && CurrentOccluders.Contains(Component))
        {
            continue;
        }

        if (Component)
        {
            Component->SetCustomDepthStencilValue(
                State.CustomDepthStencilValue
            );
            Component->SetRenderCustomDepth(State.bRenderCustomDepth);
        }
        OccluderRenderStates.RemoveAtSwap(Index);
    }

    for (UPrimitiveComponent* Component : CurrentOccluders)
    {
        if (!Component || OccluderRenderStates.ContainsByPredicate(
            [Component](const FCMVisionOccluderRenderState& State)
            {
                return State.Component.Get() == Component;
            }))
        {
            continue;
        }

        FCMVisionOccluderRenderState& State =
            OccluderRenderStates.AddDefaulted_GetRef();
        State.Component = Component;
        State.bRenderCustomDepth = Component->bRenderCustomDepth;
        State.CustomDepthStencilValue = Component->CustomDepthStencilValue;
        Component->SetCustomDepthStencilValue(
            RenderConfig->OccluderStencilValue
        );
        Component->SetRenderCustomDepth(true);
    }
}

FVector UCMVisionManagerSubsystem::ClipVisionRayToOccluder(
    const UCMVisionComponent& VisionSource,
    const FVector& RayOrigin,
    const FVector& DesiredEnd,
    float RevealDistance,
    FHitResult* OutHit
) const
{
    if (OutHit)
    {
        *OutHit = FHitResult();
    }

    if (!RenderConfig || !RenderConfig->bTraceOcclusion)
    {
        return DesiredEnd;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return DesiredEnd;
    }

    const FVector TraceStart = RayOrigin;
    const FVector TraceEnd(
        DesiredEnd.X,
        DesiredEnd.Y,
        TraceStart.Z
    );
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(CMVisionOcclusion),
        false
    );
    const AActor* SourceOwner = VisionSource.GetOwner();
    QueryParams.AddIgnoredActor(SourceOwner);
    if (SourceOwner)
    {
        QueryParams.AddIgnoredActor(SourceOwner->GetAttachParentActor());
    }

    FCollisionObjectQueryParams ObjectQueryParams;
    for (const TEnumAsByte<ECollisionChannel> ObjectType
        : RenderConfig->OccluderObjectTypes)
    {
        ObjectQueryParams.AddObjectTypesToQuery(ObjectType);
    }
    if (!ObjectQueryParams.IsValid())
    {
        return DesiredEnd;
    }

    FHitResult Hit;
    if (!World->LineTraceSingleByObjectType(
        Hit,
        TraceStart,
        TraceEnd,
        ObjectQueryParams,
        QueryParams
    ))
    {
        return DesiredEnd;
    }

    if (OutHit)
    {
        *OutHit = Hit;
    }

    const FVector TraceDirection = (TraceEnd - TraceStart).GetSafeNormal();
    const float RevealedDistance = FMath::Min(
        Hit.Distance + FMath::Max(RevealDistance, 0.0f),
        FVector::Distance(TraceStart, TraceEnd)
    );
    const FVector RevealedPoint = TraceStart
        + TraceDirection * RevealedDistance;
    return FVector(RevealedPoint.X, RevealedPoint.Y, DesiredEnd.Z);
}

bool UCMVisionManagerSubsystem::HasLineOfSight(
    const UCMVisionComponent& VisionSource,
    const FVector& WorldLocation
) const
{
    const FVector Origin = VisionSource.GetVisionOrigin();
    const FVector PlanarTarget(
        WorldLocation.X,
        WorldLocation.Y,
        Origin.Z
    );
    return ClipVisionRayToOccluder(VisionSource, Origin, PlanarTarget)
        .Equals(PlanarTarget, 1.0f);
}

FVector2D UCMVisionManagerSubsystem::WorldToMaskPixel(
    const FVector& WorldLocation,
    int32 Width,
    int32 Height
) const
{
    const float SafeWorldSize = FMath::Max(
        MaskWorldHalfExtent * 2.0f,
        1.0f
    );
    const float U = (WorldLocation.X - MaskWorldCenter.X)
        / SafeWorldSize + 0.5f;
    const float V = (WorldLocation.Y - MaskWorldCenter.Y)
        / SafeWorldSize + 0.5f;
    return FVector2D(U * Width, V * Height);
}

void UCMVisionManagerSubsystem::DrawOccluderVisibilityMask(
    UCanvas* Canvas,
    int32 Width,
    int32 Height
)
{
    TSet<UPrimitiveComponent*> CurrentOccluders;
    DrawCachedVisionMask(
        Canvas,
        Width,
        Height,
        &CurrentOccluders,
        true
    );
    UpdateOccluderRenderStates(CurrentOccluders);
}

void UCMVisionManagerSubsystem::DrawBaseVisibilityMask(
    UCanvas* Canvas,
    int32 Width,
    int32 Height
)
{
    DrawCachedVisionMask(Canvas, Width, Height, nullptr, false);
}

void UCMVisionManagerSubsystem::DrawVisionTintMask(
    UCanvas* Canvas,
    int32 Width,
    int32 Height
)
{
    DrawCachedVisionMask(Canvas, Width, Height, nullptr, true, true);
}

void UCMVisionManagerSubsystem::DrawCachedVisionMask(
    UCanvas* Canvas,
    int32 Width,
    int32 Height,
    TSet<UPrimitiveComponent*>* OutOccluders,
    bool bUseRevealedEnds,
    bool bDrawVisionTint
)
{
    if (!Canvas || !MaskDrawTexture
        || !MaskDrawTexture->GetResource())
    {
        return;
    }

    TArray<const FCMVisionSourceMaskData*> SourcesToDraw;
    SourcesToDraw.Reserve(CachedVisionMaskData.Num());
    for (const FCMVisionSourceMaskData& SourceData : CachedVisionMaskData)
    {
        if (!bDrawVisionTint && !SourceData.bRevealsWorld)
        {
            continue;
        }

        SourcesToDraw.Add(&SourceData);
    }

    if (!bDrawVisionTint)
    {
        SourcesToDraw.Sort([](
            const FCMVisionSourceMaskData& A,
            const FCMVisionSourceMaskData& B)
        {
            return A.Origin.Z < B.Origin.Z;
        });
    }

    for (const FCMVisionSourceMaskData* SourceDataPtr : SourcesToDraw)
    {
        const FCMVisionSourceMaskData& SourceData = *SourceDataPtr;

        if (bDrawVisionTint && SourceData.VisionTint.A <= 0.0f)
        {
            continue;
        }

        if (SourceData.Rays.Num() < 2
            && SourceData.NearVisionRays.Num() < 3)
        {
            continue;
        }

        const FVector2D OriginPixel = WorldToMaskPixel(
            SourceData.Origin,
            Width,
            Height
        );

        TArray<FCanvasUVTri> Triangles;
        const int32 ArcSegmentCount = SourceData.Rays.Num() - 1;
        Triangles.Reserve(
            FMath::Max(ArcSegmentCount, 0)
            + SourceData.NearVisionRays.Num()
        );
        const float EncodedHeight = FMath::Clamp(
            (SourceData.Origin.Z - MaskWorldMinHeight)
                / FMath::Max(MaskWorldHeightRange, 1.0f),
            0.0f,
            1.0f
        );
        const FLinearColor DrawColor = bDrawVisionTint
            ? SourceData.VisionTint
            : FLinearColor(1.0f, EncodedHeight, 0.0f, 1.0f);
        const auto AddMaskTriangle = [&Triangles, DrawColor](
            const FVector2D& A,
            const FVector2D& B,
            const FVector2D& C)
        {
            FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
            Triangle.V0_Pos = A;
            Triangle.V1_Pos = B;
            Triangle.V2_Pos = C;
            Triangle.V0_UV = FVector2D::ZeroVector;
            Triangle.V1_UV = FVector2D::ZeroVector;
            Triangle.V2_UV = FVector2D::ZeroVector;
            Triangle.V0_Color = DrawColor;
            Triangle.V1_Color = DrawColor;
            Triangle.V2_Color = DrawColor;
        };
        const auto AddRayTriangle = [
            this,
            &AddMaskTriangle,
            OriginPixel,
            Width,
            Height,
            OutOccluders,
            bUseRevealedEnds
        ](const FCMVisionRaySample& RayA,
            const FCMVisionRaySample& RayB)
        {
            if (OutOccluders)
            {
                if (UPrimitiveComponent* Component =
                    RayA.HitComponent.Get())
                {
                    OutOccluders->Add(Component);
                }
                if (UPrimitiveComponent* Component =
                    RayB.HitComponent.Get())
                {
                    OutOccluders->Add(Component);
                }
            }

            const FVector& PointA = bUseRevealedEnds
                ? RayA.RevealedEnd
                : RayA.BaseEnd;
            const FVector& PointB = bUseRevealedEnds
                ? RayB.RevealedEnd
                : RayB.BaseEnd;

            const FVector2D PointAPixel = WorldToMaskPixel(
                PointA,
                Width,
                Height
            );
            const FVector2D PointBPixel = WorldToMaskPixel(
                PointB,
                Width,
                Height
            );

            AddMaskTriangle(OriginPixel, PointAPixel, PointBPixel);
        };

        for (int32 ArcIndex = 0;
            ArcIndex < ArcSegmentCount;
            ++ArcIndex)
        {
            AddRayTriangle(
                SourceData.Rays[ArcIndex],
                SourceData.Rays[ArcIndex + 1]
            );
        }

        for (int32 RayIndex = 0;
            RayIndex < SourceData.NearVisionRays.Num();
            ++RayIndex)
        {
            const int32 NextRayIndex =
                (RayIndex + 1) % SourceData.NearVisionRays.Num();
            AddRayTriangle(
                SourceData.NearVisionRays[RayIndex],
                SourceData.NearVisionRays[NextRayIndex]
            );
        }

        FCanvasTriangleItem TriangleItem(
            Triangles,
            MaskDrawTexture->GetResource()
        );
        TriangleItem.BlendMode = SE_BLEND_Opaque;
        Canvas->DrawItem(TriangleItem);
    }
}
