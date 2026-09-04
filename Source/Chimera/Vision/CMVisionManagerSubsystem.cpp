#include "Vision/CMVisionManagerSubsystem.h"

#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
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
    const FName VisionAboveHeightAllowanceParameterName(
        TEXT("VisionAboveHeightAllowance")
    );

    TAutoConsoleVariable<int32> CVarVisionDebugDraw(
        TEXT("CM.Vision.DebugDraw"),
        0,
        TEXT("Draws slot vision origins (green), component locations (red), and aim rays (cyan)."),
        ECVF_Cheat
    );
}

DEFINE_LOG_CATEGORY_STATIC(LogChimeraVisionManager, Log, All);

void UCMVisionManagerSubsystem::Initialize(
    FSubsystemCollectionBase& Collection
)
{
    Super::Initialize(Collection);

    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        return;
    }

    if (EnsureLoadCoordinatorSubscription())
    {
        RefreshRenderConfigState();
    }
}

void UCMVisionManagerSubsystem::Deinitialize()
{
    if (UCMStageLoadCoordinatorSubsystem* Coordinator =
        BoundLoadCoordinator.Get())
    {
        Coordinator->OnLoadGroupFinished.RemoveAll(this);
    }
    BoundLoadCoordinator.Reset();

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
    if (!World || !World->IsGameWorld()
        || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (!bVisionSystemEnabled)
    {
        RemovePostProcessBinding();
        RestoreOccluderRenderStates();
        return;
    }

    if (!bRenderConfigReady)
    {
        if (!bRenderConfigFailed
            && !BoundLoadCoordinator.IsValid()
            && EnsureLoadCoordinatorSubscription())
        {
            RefreshRenderConfigState();
        }
        if (!bRenderConfigReady)
        {
            return;
        }
    }

    if (!RenderConfig)
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
    BuildVisionRayCache(ActiveSources);

    if (PostProcessMaterialInstance)
    {
        PostProcessMaterialInstance->SetScalarParameterValue(
            VisionMaskWorldMinHeightParameterName,
            MaskWorldMinHeight
        );
        PostProcessMaterialInstance->SetScalarParameterValue(
            VisionMaskWorldHeightRangeParameterName,
            MaskWorldHeightRange
        );
    }

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

    BaseVisibilityMask->UpdateResource();
    OccluderVisibilityMask->UpdateResource();
    VisionTintMask->UpdateResource();
    TimeUntilMaskUpdate = FMath::Max(
        RenderConfig->MaskUpdateInterval,
        0.01f
    );

}

bool UCMVisionManagerSubsystem::EnsureLoadCoordinatorSubscription()
{
    if (BoundLoadCoordinator.IsValid())
    {
        return true;
    }

    UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance() : nullptr;
    UCMStageLoadCoordinatorSubsystem* Coordinator = GameInstance
        ? GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>()
        : nullptr;
    if (!Coordinator)
    {
        return false;
    }

    Coordinator->OnLoadGroupFinished.AddUniqueDynamic(
        this, &ThisClass::HandleLoadGroupFinished);
    BoundLoadCoordinator = Coordinator;
    return true;
}

void UCMVisionManagerSubsystem::RefreshRenderConfigState()
{
    const UCMVisionSettings* Settings = GetDefault<UCMVisionSettings>();
    if (!Settings || Settings->DefaultRenderConfig.IsNull())
    {
        MarkRenderConfigFailed(TEXT("DefaultRenderConfig is empty"));
        return;
    }
    if (Settings->DefaultRenderConfigLoadGroupId.IsNone())
    {
        MarkRenderConfigFailed(TEXT("DefaultRenderConfigLoadGroupId is empty"));
        return;
    }

    const UCMStageLoadCoordinatorSubsystem* Coordinator =
        BoundLoadCoordinator.Get();
    if (!Coordinator)
    {
        return;
    }

    const ECMStageLoadGroupState State = Coordinator->GetLoadGroupState(
        Settings->DefaultRenderConfigLoadGroupId);
    if (State == ECMStageLoadGroupState::Ready)
    {
        TryResolveLoadedRenderConfig();
    }
    else if (State == ECMStageLoadGroupState::Failed
        || State == ECMStageLoadGroupState::Released)
    {
        MarkRenderConfigFailed(TEXT("LoadGroup is not available"));
    }
}

void UCMVisionManagerSubsystem::HandleLoadGroupFinished(
    FName FinishedLoadGroupId,
    EAsyncLoadResult Result,
    bool bReleasedImmediately
)
{
    const UCMVisionSettings* Settings = GetDefault<UCMVisionSettings>();
    if (!Settings
        || FinishedLoadGroupId != Settings->DefaultRenderConfigLoadGroupId
        || bRenderConfigReady
        || bRenderConfigFailed)
    {
        return;
    }

    if (Result != EAsyncLoadResult::Succeeded || bReleasedImmediately)
    {
        MarkRenderConfigFailed(
            TEXT("LoadGroup failed or was released immediately"));
        return;
    }
    TryResolveLoadedRenderConfig();
}

bool UCMVisionManagerSubsystem::TryResolveLoadedRenderConfig()
{
    const UCMVisionSettings* Settings = GetDefault<UCMVisionSettings>();
    RenderConfig = Settings ? Settings->DefaultRenderConfig.Get() : nullptr;
    PostProcessMaterialAsset = RenderConfig
        ? RenderConfig->PostProcessMaterial.Get() : nullptr;
    MaskDrawTexture = RenderConfig
        ? RenderConfig->MaskDrawTexture.Get() : nullptr;

    if (RenderConfig && PostProcessMaterialAsset && MaskDrawTexture)
    {
        bRenderConfigReady = true;
        bRenderConfigFailed = false;
        bConfigurationFailureLogged = false;
        MaskWorldHalfExtent = FMath::Max(
            RenderConfig->MinimumWorldHalfExtent,
            1.0f
        );
        return true;
    }
    
    MarkRenderConfigFailed(
        TEXT("Definition assets were not loaded by the assigned LoadGroup"));
    return false;
}

void UCMVisionManagerSubsystem::MarkRenderConfigFailed(const TCHAR* Reason)
{
    if (bRenderConfigFailed)
    {
        return;
    }

    bRenderConfigReady = false;
    bRenderConfigFailed = true;
    if (!bConfigurationFailureLogged)
    {
        const UCMVisionSettings* Settings = GetDefault<UCMVisionSettings>();
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Vision Render Config failed. Definition=%s LoadGroup=%s Reason=%s"),
            Settings
                ? *Settings->DefaultRenderConfig.ToSoftObjectPath().ToString()
                : TEXT("None"),
            Settings
                ? *Settings->DefaultRenderConfigLoadGroupId.ToString()
                : TEXT("None"),
            Reason);
        bConfigurationFailureLogged = true;
    }
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
    if (!bRenderConfigReady)
    {
        return false;
    }

    TArray<UCMVisionComponent*> SeeingSources;
    GetVisionSourcesSeeingLocation(WorldLocation, SeeingSources);
    return !SeeingSources.IsEmpty();
}

void UCMVisionManagerSubsystem::GetVisionSourcesSeeingLocation(
    const FVector& WorldLocation,
    TArray<UCMVisionComponent*>& OutSources
) const
{
    OutSources.Reset();

    if (!bVisionSystemEnabled || !bRenderConfigReady)
    {
        return;
    }

    for (const TWeakObjectPtr<UCMVisionComponent>& VisionSource : VisionSources)
    {
        if (UCMVisionComponent* VisionComponent = VisionSource.Get())
        {
            if (VisionComponent->GetVisionContribution()
                    == ECMVisionContribution::RevealAndTint
                && VisionComponent->IsLocationVisible(WorldLocation)
                && HasLineOfSight(*VisionComponent, WorldLocation))
            {
                OutSources.Add(VisionComponent);
            }
        }
    }
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

    FVector2D ViewportSize;
    if (!GetViewportSize(ViewportSize))
    {
        return;
    }
    const int32 SafeResolution = FMath::Clamp(
        FMath::Max(ViewportSize.X, ViewportSize.Y), 64, 4096);

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

}

bool UCMVisionManagerSubsystem::GetViewportSize(FVector2D& OutSize) const
{
    if (!GEngine || !GEngine->GameViewport)
    {
        return false;
    }
    GEngine->GameViewport->GetViewportSize(OutSize);
    return OutSize.X > 0 && OutSize.Y > 0;
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
        SourceData.NearVisionRadius = FMath::Max(
            VisionSource->GetNearVisionRadius(),
            0.0f
        );
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
    PostProcessMaterialInstance->SetScalarParameterValue(
        VisionAboveHeightAllowanceParameterName,
        RenderConfig->VisionAboveHeightAllowance
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
    
    UCameraComponent* Camera = ViewTarget
        ? ViewTarget->FindComponentByClass<UCameraComponent>()
        : nullptr;
    
    return Camera;
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
        true
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

    TArray<FHitResult> Hits;
    if (!World->LineTraceMultiByObjectType(
        Hits,
        TraceStart,
        TraceEnd,
        ObjectQueryParams,
        QueryParams
    ))
    {
        return DesiredEnd;
    }

    FHitResult Hit;
    for (const FHitResult& Candidate : Hits)
    {
        if (!Candidate.bBlockingHit)
        {
            continue;
        }

        const AActor* HitActor = Candidate.GetActor();
        const UPrimitiveComponent* HitComponent = Candidate.GetComponent();
        const bool bIgnored = RenderConfig->VisionOccluderIgnoreTag != NAME_None
            && ((HitActor && HitActor->ActorHasTag(
                RenderConfig->VisionOccluderIgnoreTag
            )) || (HitComponent && HitComponent->ComponentHasTag(
                RenderConfig->VisionOccluderIgnoreTag
            )));
        if (bIgnored)
        {
            continue;
        }

        Hit = Candidate;
        break;
    }

    if (!Hit.IsValidBlockingHit())
    {
        return DesiredEnd;
    }

    if (OutHit)
    {
        *OutHit = Hit;
    }

    const FVector TraceDirection = (TraceEnd - TraceStart).GetSafeNormal();
    float OccluderDistance = Hit.Distance;
    if (const UPrimitiveComponent* HitComponent = Hit.GetComponent())
    {
        const FBoxSphereBounds Bounds = HitComponent->Bounds;
        const float GeometricThickness = 2.0f * FMath::Min3(
            Bounds.BoxExtent.X,
            Bounds.BoxExtent.Y,
            Bounds.BoxExtent.Z
        );
        const float RequiredThickness = FMath::Max(
            RenderConfig->MinimumOccluderThickness
                - GeometricThickness,
            0.0f
        );
        OccluderDistance = FMath::Max(
            OccluderDistance,
            Hit.Distance + RequiredThickness
        );
        const float TopHeight = Bounds.Origin.Z + Bounds.BoxExtent.Z;
        const float LowObstacleTopRevealHeight = FMath::Max(
            FMath::Max(
                RenderConfig->VisionHeightTolerance,
                RenderConfig->OccluderSurfaceRevealDistance
            ),
            RenderConfig->VisionBelowHeightAllowance
        );
        const float VisionTopHeight = VisionSource.GetVisionOrigin().Z
            + LowObstacleTopRevealHeight;
        if (TopHeight <= VisionTopHeight)
        {
            const FVector PlanarDirection = TraceDirection.GetSafeNormal2D();
            const float ProjectedCenter = FVector::DotProduct(
                Bounds.Origin - TraceStart,
                PlanarDirection
            );
            const float ProjectedExtent = FMath::Abs(PlanarDirection.X)
                * Bounds.BoxExtent.X
                + FMath::Abs(PlanarDirection.Y) * Bounds.BoxExtent.Y;
            OccluderDistance = FMath::Max(
                OccluderDistance,
                FMath::Min(ProjectedCenter + ProjectedExtent,
                    FVector::Distance(TraceStart, TraceEnd))
            );
        }
    }
    const float RevealedDistance = FMath::Min(
        OccluderDistance + FMath::Max(RevealDistance, 0.0f),
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

FVector2D UCMVisionManagerSubsystem::WorldToScreenMaskPixel(
    const FVector& WorldLocation,
    int32 Width,
    int32 Height
) const
{
    const APlayerController* PlayerController = GetWorld()
        ? GetWorld()->GetFirstPlayerController() : nullptr;
    FVector2D ScreenPosition;
    FVector2D ViewportSize;
    if (!PlayerController
        || !PlayerController->ProjectWorldLocationToScreen(
            WorldLocation, ScreenPosition, false)
        || !GetViewportSize(ViewportSize))
    {
        return FVector2D(-Width, -Height);
    }
    return FVector2D(
        ScreenPosition.X * Width / static_cast<float>(ViewportSize.X),
        ScreenPosition.Y * Height / static_cast<float>(ViewportSize.Y));
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

    // Clear explicitly for every render-target update.  Relying only on
    // UCanvasRenderTarget2D's receive-update clear can leave pixels from a
    // previous frame when the active vision polygon changes shape or height.
    // Those stale pixels appear as an unrelated vision cone, especially on
    // upper floors after moving around a low obstacle.
    FCanvasTileItem ClearItem(
        FVector2D::ZeroVector,
        FVector2D(static_cast<float>(Width), static_cast<float>(Height)),
        bDrawVisionTint ? FLinearColor::Transparent : FLinearColor::Black
    );
    ClearItem.BlendMode = SE_BLEND_Opaque;
    Canvas->DrawItem(ClearItem);

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

        const FVector2D OriginPixel = WorldToScreenMaskPixel(
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
        const float NearRadiusPixels = 0.0f;
        const auto AddMaskTriangle = [&Triangles](
            const FVector2D& A,
            const FVector2D& B,
            const FVector2D& C,
            const FLinearColor& ColorA,
            const FLinearColor& ColorB,
            const FLinearColor& ColorC)
        {
            FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
            Triangle.V0_Pos = A;
            Triangle.V1_Pos = B;
            Triangle.V2_Pos = C;
            Triangle.V0_UV = FVector2D::ZeroVector;
            Triangle.V1_UV = FVector2D::ZeroVector;
            Triangle.V2_UV = FVector2D::ZeroVector;
            Triangle.V0_Color = ColorA;
            Triangle.V1_Color = ColorB;
            Triangle.V2_Color = ColorC;
        };
        const auto AddRayTriangle = [
            this,
            &AddMaskTriangle,
            DrawColor,
            OriginPixel,
            Width,
            Height,
            OutOccluders,
            bUseRevealedEnds,
            bDrawVisionTint,
            NearRadiusPixels
        ](const FCMVisionRaySample& RayA,
            const FCMVisionRaySample& RayB,
            bool bSoftenEdgeA = false,
            bool bSoftenEdgeB = false)
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

            const FVector2D PointAPixel = WorldToScreenMaskPixel(
                PointA,
                Width,
                Height
            );
            const FVector2D PointBPixel = WorldToScreenMaskPixel(
                PointB,
                Width,
                Height
            );

            const bool bUseEdgeSoftness =
                !bDrawVisionTint && RenderConfig->VisionEdgeSoftness > 0.0f;
            if (!bUseEdgeSoftness)
            {
                AddMaskTriangle(
                    OriginPixel,
                    PointAPixel,
                    PointBPixel,
                    DrawColor,
                    DrawColor,
                    DrawColor
                );
            }
            else
            {
                const float SoftnessPixels = RenderConfig->VisionEdgeSoftness
                    * Width / FMath::Max(MaskWorldHalfExtent * 2.0f, 1.0f);
                const float DistanceA = FVector2D::Distance(
                    OriginPixel,
                    PointAPixel
                );
                const float DistanceB = FVector2D::Distance(
                    OriginPixel,
                    PointBPixel
                );
                const float InnerDistanceA = FMath::Max(
                    DistanceA - SoftnessPixels,
                    0.0f
                );
                const float InnerDistanceB = FMath::Max(
                    DistanceB - SoftnessPixels,
                    0.0f
                );
                const FVector2D InnerPointA = FMath::IsNearlyZero(DistanceA)
                    ? OriginPixel
                    : FMath::Lerp(
                        OriginPixel,
                        PointAPixel,
                        InnerDistanceA / DistanceA
                    );
                const FVector2D InnerPointB = FMath::IsNearlyZero(DistanceB)
                    ? OriginPixel
                    : FMath::Lerp(
                        OriginPixel,
                        PointBPixel,
                        InnerDistanceB / DistanceB
                    );
                // The edge contributes no visibility at its outer boundary.
                // Its alpha must also be zero; with alpha blending this lets
                // another source's visibility remain visible underneath.
                // Keep the visibility channel at one while fading alpha.
                // This makes the source a true union: a feather cannot lower
                // visibility that another source has already provided.
                const FLinearColor EdgeColor(1.0f, DrawColor.G, 0.0f, 0.0f);
                const auto AddAngularEdge = [
                    &AddMaskTriangle,
                    OriginPixel,
                    DrawColor,
                    EdgeColor,
                    SoftnessPixels,
                    NearRadiusPixels
                ](
                    const FVector2D& EdgePoint,
                    const FVector2D& InteriorPoint,
                    const FVector2D& EdgeInnerPoint,
                    const FVector2D& InteriorInnerPoint
                )
                {
                    const float EdgeLength = FVector2D::Distance(
                        EdgeInnerPoint,
                        InteriorInnerPoint
                    );
                    if (FMath::IsNearlyZero(EdgeLength))
                    {
                        return;
                    }
                    const float Inset = FMath::Min(
                        SoftnessPixels / EdgeLength,
                        0.5f
                    );
                    const FVector2D InsetPoint = FMath::Lerp(
                        EdgeInnerPoint,
                        InteriorInnerPoint,
                        Inset
                    );
                    const FVector2D EdgeDirection = (
                        EdgePoint - OriginPixel
                    ).GetSafeNormal();
                    const float EdgeDistance = FVector2D::Distance(
                        OriginPixel,
                        EdgePoint
                    );
                    const FVector2D FeatherStart = OriginPixel
                        + EdgeDirection * FMath::Min(
                            NearRadiusPixels,
                            EdgeDistance
                        );
                    const FVector2D InteriorDirection = (
                        InteriorPoint - OriginPixel
                    ).GetSafeNormal();
                    const FVector2D InteriorFeatherStart = OriginPixel
                        + InteriorDirection * FMath::Min(
                            NearRadiusPixels,
                            FVector2D::Distance(
                                OriginPixel,
                                InteriorPoint
                            )
                        );
                    const FVector2D InsetFeatherStart = FMath::Lerp(
                        FeatherStart,
                        InteriorFeatherStart,
                        Inset
                    );
                    AddMaskTriangle(
                        FeatherStart,
                        EdgeInnerPoint,
                        InsetPoint,
                        EdgeColor,
                        EdgeColor,
                        DrawColor
                    );
                    AddMaskTriangle(
                        FeatherStart,
                        InsetPoint,
                        InsetFeatherStart,
                        EdgeColor,
                        DrawColor,
                        DrawColor
                    );
                };

                if (bSoftenEdgeA)
                {
                    AddAngularEdge(
                        PointAPixel,
                        PointBPixel,
                        InnerPointA,
                        InnerPointB
                    );
                }
                if (bSoftenEdgeB)
                {
                    AddAngularEdge(
                        PointBPixel,
                        PointAPixel,
                        InnerPointB,
                        InnerPointA
                    );
                }

                // Fill the part inside the softened boundary.  The original
                // full fan triangle is intentionally not drawn in this path,
                // so this center triangle is required to keep the interior
                // fully visible.
                AddMaskTriangle(
                    OriginPixel,
                    InnerPointA,
                    InnerPointB,
                    DrawColor,
                    DrawColor,
                    DrawColor
                );
                AddMaskTriangle(
                    InnerPointA,
                    InnerPointB,
                    PointAPixel,
                    DrawColor,
                    DrawColor,
                    EdgeColor
                );
                AddMaskTriangle(
                    PointAPixel,
                    InnerPointB,
                    PointBPixel,
                    EdgeColor,
                    DrawColor,
                    EdgeColor
                );
            }
        };

        // Draw the near circle first so the cone pass can cover their
        // overlapping area without leaving a seam at the junction.
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

        for (int32 ArcIndex = 0;
            ArcIndex < ArcSegmentCount;
            ++ArcIndex)
        {
            AddRayTriangle(
                SourceData.Rays[ArcIndex],
                SourceData.Rays[ArcIndex + 1],
                ArcIndex == 0,
                ArcIndex == ArcSegmentCount - 1
            );
        }

        FCanvasTriangleItem TriangleItem(
            Triangles,
            MaskDrawTexture->GetResource()
        );
        // Visibility sources form a union.  Alpha blending preserves an
        // already-visible pixel while allowing each source's feather to
        // transition in over it, instead of replacing it with black.
        TriangleItem.BlendMode = bDrawVisionTint
            ? SE_BLEND_Opaque
            : SE_BLEND_Translucent;
        Canvas->DrawItem(TriangleItem);
    }
}
