#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "CMVisionManagerSubsystem.generated.h"

class UCMVisionComponent;
class UCanvas;
class UCanvasRenderTarget2D;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

/** Local registry and union query for all replicated shared-vision sources. */
UCLASS()
class CHIMERA_API UCMVisionManagerSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    void RegisterVisionSource(UCMVisionComponent* VisionComponent);
    void UnregisterVisionSource(UCMVisionComponent* VisionComponent);

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    bool IsLocationVisible(const FVector& WorldLocation) const;

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
    void EnsureVisibilityMask();
    void UpdateVisibilityMaskBounds(
        const TArray<UCMVisionComponent*>& ActiveSources
    );
    void EnsurePostProcessBinding();
    FVector2D WorldToMaskPixel(
        const FVector& WorldLocation,
        int32 Width,
        int32 Height
    ) const;

    UFUNCTION()
    void DrawVisibilityMask(UCanvas* Canvas, int32 Width, int32 Height);

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<UCMVisionComponent>> VisionSources;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasRenderTarget2D> VisibilityMask;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PostProcessMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> MaskDrawTexture;

    TWeakObjectPtr<UCameraComponent> BoundCamera;
    FVector2D MaskWorldCenter = FVector2D::ZeroVector;
    float MaskWorldHalfExtent = 1000.0f;
    float TimeUntilMaskUpdate = 0.0f;
};
