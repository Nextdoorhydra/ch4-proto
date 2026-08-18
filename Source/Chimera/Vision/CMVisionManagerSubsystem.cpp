#include "Vision/CMVisionManagerSubsystem.h"

#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Parts/Head/CMVisionComponent.h"

namespace
{
    constexpr int32 VisibilityMaskResolution = 512;
    constexpr int32 VisionArcSegments = 24;
    constexpr float VisibilityMaskUpdateInterval = 0.05f;
    const TCHAR* VisionPostProcessMaterialPath =
        TEXT("/Game/Chimera/Vision/M_CMVisionMaskPostProcess.M_CMVisionMaskPostProcess");
}

void UCMVisionManagerSubsystem::Deinitialize()
{
    if (UCameraComponent* Camera = BoundCamera.Get())
    {
        if (PostProcessMaterial)
        {
            Camera->PostProcessSettings.RemoveBlendable(
                PostProcessMaterial
            );
        }
    }

    BoundCamera.Reset();
    PostProcessMaterial = nullptr;
    VisibilityMask = nullptr;
    MaskDrawTexture = nullptr;
    VisionSources.Reset();
    Super::Deinitialize();
}

void UCMVisionManagerSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    EnsureVisibilityMask();
    EnsurePostProcessBinding();
    if (!VisibilityMask)
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
    VisibilityMask->UpdateResource();
    TimeUntilMaskUpdate = VisibilityMaskUpdateInterval;

    if (PostProcessMaterial)
    {
        PostProcessMaterial->SetVectorParameterValue(
            TEXT("VisionMaskCenter"),
            FLinearColor(
                MaskWorldCenter.X,
                MaskWorldCenter.Y,
                0.0f,
                0.0f
            )
        );
        PostProcessMaterial->SetScalarParameterValue(
            TEXT("VisionMaskHalfExtent"),
            MaskWorldHalfExtent
        );
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
    for (const TWeakObjectPtr<UCMVisionComponent>& VisionSource
        : VisionSources)
    {
        if (const UCMVisionComponent* VisionComponent = VisionSource.Get())
        {
            if (VisionComponent->IsLocationVisible(WorldLocation))
            {
                return true;
            }
        }
    }

    return false;
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
    return VisibilityMask;
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
    if (VisibilityMask)
    {
        return;
    }

    VisibilityMask = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(
        this,
        UCanvasRenderTarget2D::StaticClass(),
        VisibilityMaskResolution,
        VisibilityMaskResolution
    );
    if (VisibilityMask)
    {
        VisibilityMask->ClearColor = FLinearColor::Black;
        VisibilityMask->SetShouldClearRenderTargetOnReceiveUpdate(true);
        VisibilityMask->OnCanvasRenderTargetUpdate.AddDynamic(
            this,
            &UCMVisionManagerSubsystem::DrawVisibilityMask
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
        const FVector Location = VisionSource->GetComponentLocation();
        CenterSum += FVector2D(Location.X, Location.Y);
    }
    MaskWorldCenter = CenterSum / ActiveSources.Num();

    float RequiredHalfExtent = 1000.0f;
    for (const UCMVisionComponent* VisionSource : ActiveSources)
    {
        const FVector Location = VisionSource->GetComponentLocation();
        RequiredHalfExtent = FMath::Max(
            RequiredHalfExtent,
            FMath::Max(
                FMath::Abs(Location.X - MaskWorldCenter.X),
                FMath::Abs(Location.Y - MaskWorldCenter.Y)
            ) + VisionSource->GetVisionDistance()
        );
    }
    MaskWorldHalfExtent = RequiredHalfExtent;
}

void UCMVisionManagerSubsystem::EnsurePostProcessBinding()
{
    if (PostProcessMaterial && BoundCamera.IsValid())
    {
        return;
    }

    UWorld* World = GetWorld();
    APlayerController* PlayerController = World
        ? World->GetFirstPlayerController()
        : nullptr;
    AActor* ViewTarget = PlayerController
        ? PlayerController->GetViewTarget()
        : nullptr;
    UCameraComponent* Camera = ViewTarget
        ? ViewTarget->FindComponentByClass<UCameraComponent>()
        : nullptr;
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(
        nullptr,
        VisionPostProcessMaterialPath
    );
    if (!Camera || !Material || !VisibilityMask)
    {
        return;
    }

    PostProcessMaterial = UMaterialInstanceDynamic::Create(Material, this);
    PostProcessMaterial->SetTextureParameterValue(
        TEXT("VisionMask"),
        VisibilityMask
    );
    Camera->PostProcessSettings.AddBlendable(PostProcessMaterial, 1.0f);
    BoundCamera = Camera;
}

FVector2D UCMVisionManagerSubsystem::WorldToMaskPixel(
    const FVector& WorldLocation,
    int32 Width,
    int32 Height
) const
{
    const float SafeExtent = FMath::Max(MaskWorldHalfExtent, 1.0f);
    const float U = (WorldLocation.X - MaskWorldCenter.X)
        / (SafeExtent * 2.0f) + 0.5f;
    const float V = 0.5f - (WorldLocation.Y - MaskWorldCenter.Y)
        / (SafeExtent * 2.0f);
    return FVector2D(U * Width, V * Height);
}

void UCMVisionManagerSubsystem::DrawVisibilityMask(
    UCanvas* Canvas,
    int32 Width,
    int32 Height
)
{
    if (!Canvas || !MaskDrawTexture || !MaskDrawTexture->GetResource())
    {
        return;
    }

    TArray<UCMVisionComponent*> ActiveSources;
    GetActiveVisionSources(ActiveSources);
    for (const UCMVisionComponent* VisionSource : ActiveSources)
    {
        const FVector Origin = VisionSource->GetComponentLocation();
        const FVector Direction = VisionSource->GetAimDirection();
        const float Distance = VisionSource->GetVisionDistance();
        const float HalfAngleRadians = FMath::DegreesToRadians(
            VisionSource->GetVisionAngleDegrees() * 0.5f
        );
        const float CenterAngle = FMath::Atan2(Direction.Y, Direction.X);
        const FVector2D OriginPixel = WorldToMaskPixel(
            Origin,
            Width,
            Height
        );

        TArray<FCanvasUVTri> Triangles;
        Triangles.Reserve(VisionArcSegments);
        for (int32 ArcIndex = 0;
            ArcIndex < VisionArcSegments;
            ++ArcIndex)
        {
            const float AlphaA =
                static_cast<float>(ArcIndex) / VisionArcSegments;
            const float AlphaB =
                static_cast<float>(ArcIndex + 1) / VisionArcSegments;
            const float AngleA = FMath::Lerp(
                CenterAngle - HalfAngleRadians,
                CenterAngle + HalfAngleRadians,
                AlphaA
            );
            const float AngleB = FMath::Lerp(
                CenterAngle - HalfAngleRadians,
                CenterAngle + HalfAngleRadians,
                AlphaB
            );

            const FVector PointA = Origin + FVector(
                FMath::Cos(AngleA) * Distance,
                FMath::Sin(AngleA) * Distance,
                0.0f
            );
            const FVector PointB = Origin + FVector(
                FMath::Cos(AngleB) * Distance,
                FMath::Sin(AngleB) * Distance,
                0.0f
            );

            FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
            Triangle.V0_Pos = OriginPixel;
            Triangle.V1_Pos = WorldToMaskPixel(PointA, Width, Height);
            Triangle.V2_Pos = WorldToMaskPixel(PointB, Width, Height);
            Triangle.V0_UV = FVector2D::ZeroVector;
            Triangle.V1_UV = FVector2D::ZeroVector;
            Triangle.V2_UV = FVector2D::ZeroVector;
            Triangle.V0_Color = FLinearColor::White;
            Triangle.V1_Color = FLinearColor::White;
            Triangle.V2_Color = FLinearColor::White;
        }

        FCanvasTriangleItem TriangleItem(
            Triangles,
            MaskDrawTexture->GetResource()
        );
        TriangleItem.BlendMode = SE_BLEND_Additive;
        Canvas->DrawItem(TriangleItem);
    }
}
