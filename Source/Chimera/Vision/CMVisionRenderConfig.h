#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CMVisionRenderConfig.generated.h"

class UMaterialInterface;

/** Editor-authored rendering policy shared by one local Vision Manager. */
UCLASS(BlueprintType)
class CHIMERA_API UCMVisionRenderConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
    TSoftObjectPtr<UMaterialInterface> PostProcessMaterial;

    /** Multiplicative color applied only inside the visible mask. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|Tint")
    FLinearColor VisionTintColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|Tint",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VisionTintStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
        meta = (ClampMin = "64", ClampMax = "2048"))
    int32 MaskResolution = 2048;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
        meta = (ClampMin = "3", ClampMax = "128"))
    int32 ArcSegmentCount = 24;

    /** Ray count used to keep the near-vision circle blocked by walls. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
        meta = (ClampMin = "8", ClampMax = "64"))
    int32 NearVisionCircleSegmentCount = 24;

    /** Extra binary-search rays added only around occlusion silhouettes. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
        meta = (ClampMin = "0", ClampMax = "8"))
    int32 OcclusionEdgeRefinementSteps = 5;

    /** Hit-distance jump that is treated as an occlusion silhouette. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
        meta = (ClampMin = "1.0"))
    float OcclusionEdgeRefinementDistance = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
        meta = (ClampMin = "0.01"))
    float MaskUpdateInterval = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
        meta = (ClampMin = "1.0"))
    float MinimumWorldHalfExtent = 1000.0f;

    /** Keeps a black border around the world mask so clamped UVs stay hidden. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
        meta = (ClampMin = "1.0"))
    float MaskBoundsPadding = 1.05f;

    /** High surfaces flatter than this are treated as ceilings. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask|Height",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CeilingSurfaceNormalZThreshold = 0.5f;

    /** Small allowance around the logical eye height to avoid edge flicker. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask|Height",
        meta = (ClampMin = "0.0"))
    float VisionHeightTolerance = 2.0f;

    /** Object types treated as walls. WorldStatic excludes Pawns and Parts. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion")
    TArray<TEnumAsByte<ECollisionChannel>> OccluderObjectTypes = {
        ECC_WorldStatic
    };

    /** Wall-only depth allowance; the base world mask remains fully occluded. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion",
        meta = (ClampMin = "0.0"))
    float OccluderSurfaceRevealDistance = 100.0f;

    /** Reserved stencil value used to apply the world mask only to walls. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion",
        meta = (ClampMin = "1", ClampMax = "255"))
    int32 OccluderStencilValue = 251;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion")
    bool bTraceOcclusion = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PostProcessBlendWeight = 1.0f;
};
