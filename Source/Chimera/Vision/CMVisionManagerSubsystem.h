#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "CMVisionManagerSubsystem.generated.h"

class UCMVisionComponent;
class UCMVisionRenderConfig;
class UCanvas;
class UCanvasRenderTarget2D;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UTexture2D;
struct FHitResult;

struct FCMVisionOccluderRenderState
{
    TWeakObjectPtr<UPrimitiveComponent> Component;
    bool bRenderCustomDepth = false;
    int32 CustomDepthStencilValue = 0;
};

struct FCMVisionRaySample
{
    FVector BaseEnd = FVector::ZeroVector;
    FVector RevealedEnd = FVector::ZeroVector;
    TWeakObjectPtr<UPrimitiveComponent> HitComponent;
    bool bBlockingHit = false;
};

struct FCMVisionSourceMaskData
{
    FVector Origin = FVector::ZeroVector;
    TArray<FCMVisionRaySample> Rays;
    TArray<FCMVisionRaySample> NearVisionRays;
    FLinearColor VisionTint = FLinearColor::Transparent;
    bool bRevealsWorld = true;
};

/** Local registry and union query for all replicated shared-vision sources. */
UCLASS()
class CHIMERA_API UCMVisionManagerSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    void RegisterVisionSource(UCMVisionComponent* VisionComponent);
    void UnregisterVisionSource(UCMVisionComponent* VisionComponent);

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    bool IsLocationVisible(const FVector& WorldLocation) const;

    UFUNCTION(BlueprintCallable, Category = "Chimera|Vision")
    void DisableVisionSystem();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Vision")
    void EnableVisionSystem();

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    bool IsVisionSystemEnabled() const;

    void GetActiveVisionSources(
        TArray<UCMVisionComponent*>& OutVisionSources
    ) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    UCanvasRenderTarget2D* GetVisibilityMask() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    FVector2D GetVisibilityMaskWorldCenter() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    float GetVisibilityMaskWorldHalfExtent() const;

private:
    bool LoadRenderConfig();
    void EnsureVisibilityMask();
    void UpdateVisibilityMaskBounds(
        const TArray<UCMVisionComponent*>& ActiveSources
    );
    void BuildVisionRayCache(
        const TArray<UCMVisionComponent*>& ActiveSources
    );
    void EnsurePostProcessBinding();
    UCameraComponent* FindViewCamera() const;
    void RemovePostProcessBinding();
    void RestoreOccluderRenderStates();
    void UpdateOccluderRenderStates(
        const TSet<UPrimitiveComponent*>& CurrentOccluders
    );
    FVector ClipVisionRayToOccluder(
        const UCMVisionComponent& VisionSource,
        const FVector& RayOrigin,
        const FVector& DesiredEnd,
        float RevealDistance = 0.0f,
        FHitResult* OutHit = nullptr
    ) const;
    bool HasLineOfSight(
        const UCMVisionComponent& VisionSource,
        const FVector& WorldLocation
    ) const;
    FVector2D WorldToMaskPixel(
        const FVector& WorldLocation,
        int32 Width,
        int32 Height
    ) const;
    void DrawCachedVisionMask(
        UCanvas* Canvas,
        int32 Width,
        int32 Height,
        TSet<UPrimitiveComponent*>* OutOccluders,
        bool bUseRevealedEnds,
        bool bDrawVisionTint = false
    );

    UFUNCTION()
    void DrawOccluderVisibilityMask(
        UCanvas* Canvas,
        int32 Width,
        int32 Height
    );

    UFUNCTION()
    void DrawBaseVisibilityMask(UCanvas* Canvas, int32 Width, int32 Height);

    UFUNCTION()
    void DrawVisionTintMask(UCanvas* Canvas, int32 Width, int32 Height);

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<UCMVisionComponent>> VisionSources;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasRenderTarget2D> OccluderVisibilityMask;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasRenderTarget2D> BaseVisibilityMask;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasRenderTarget2D> VisionTintMask;

    UPROPERTY(Transient)
    TObjectPtr<UCMVisionRenderConfig> RenderConfig;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> PostProcessMaterialAsset;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PostProcessMaterialInstance;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> MaskDrawTexture;

    TWeakObjectPtr<UCameraComponent> BoundCamera;
    TArray<FCMVisionSourceMaskData> CachedVisionMaskData;
    TArray<FCMVisionOccluderRenderState> OccluderRenderStates;
    FVector2D MaskWorldCenter = FVector2D::ZeroVector;
    float MaskWorldHalfExtent = 1000.0f;
    float MaskWorldMinHeight = -100.0f;
    float MaskWorldHeightRange = 200.0f;
    float TimeUntilMaskUpdate = 0.0f;
    bool bVisionSystemEnabled = true;
    bool bConfigurationFailureLogged = false;
};
