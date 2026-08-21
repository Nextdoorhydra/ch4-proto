#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CMCameraOcclusionConfig.generated.h"

/** Editor-authored policy for fading objects between the camera and Chimera. */
UCLASS(BlueprintType)
class CHIMERA_API UCMCameraOcclusionConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Camera Occlusion")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Camera Occlusion")
    TArray<TEnumAsByte<ECollisionChannel>> OccluderObjectTypes = {
        ECC_WorldStatic
    };

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.0"))
    float TraceRadius = 50.0f;

    /** Used only when the local player has no controlled Head. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Camera Occlusion")
    float TargetHeightOffset = 80.0f;

    /** Offset from the first controlled Head in normalized viewport UV. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Camera Occlusion",
        meta = (UIMin = "-0.5", UIMax = "0.5"))
    FVector2D ScreenCenterOffset = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.0"))
    float FadeOutSpeed = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.0"))
    float FadeInSpeed = 3.0f;

    /** Normalized viewport radius used by the obstacle material's screen mask. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ScreenFadeRadius = 0.15f;

    /** Approximate visible coverage at the center of the dithered fade. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumOpacity = 0.1f;

    /** Width of the soft transition around the screen-space fade circle. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float EdgeSoftness = 0.03f;
};
