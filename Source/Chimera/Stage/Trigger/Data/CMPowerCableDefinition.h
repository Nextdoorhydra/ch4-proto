#pragma once

#include "CoreMinimal.h"
#include "PrimaryDataAssetBase.h"

#include "CMPowerCableDefinition.generated.h"

class UStaticMesh;

/** Async-loadable visual definition for a power cable. */
UCLASS(BlueprintType)
class CHIMERA_API UCMPowerCableDefinition : public UPrimaryDataAssetBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power|Cable",
        meta = (AssetBundles = "Gameplay"))
    TSoftObjectPtr<UStaticMesh> CableMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Visual", meta = (ClampMin = "1"))
    int32 VisualSegmentCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Visual", meta = (ClampMin = "0.0"))
    float CableSag = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Visual", meta = (ClampMin = "0.01"))
    float CableThicknessScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Simulation", meta = (ClampMin = "0.0"))
    float InitialCableLength = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Simulation", meta = (ClampMin = "5.0"))
    float RopeNodeSpacing = 35.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Simulation", meta = (ClampMin = "0.0"))
    float RopeGravityScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Simulation", meta = (ClampMin = "0.0",
        ClampMax = "1.0"))
    float RopeDamping = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Simulation", meta = (ClampMin = "1"))
    int32 RopeConstraintIterations = 16;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Simulation", meta = (ClampMin = "0.0"))
    float RopeCollisionRadius = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Simulation", meta = (ClampMin = "0.0"))
    float RopeSleepMovementThreshold = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Simulation", meta = (ClampMin = "1"))
    int32 RopeSleepFrameCount = 20;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Simulation", meta = (ClampMin = "0.0"))
    float InitialCoilRadius = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Cable|Simulation")
    bool bStartCoiled = true;
};
