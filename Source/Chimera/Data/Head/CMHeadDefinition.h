#pragma once

#include "CoreMinimal.h"
#include "PrimaryDataAssetBase.h"

#include "CMHeadDefinition.generated.h"

class ACMHeadPartActor;

/** Async-loadable gameplay definition for one Head Part. */
UCLASS(BlueprintType)
class CHIMERA_API UCMHeadDefinition : public UPrimaryDataAssetBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Head")
    FName ID = NAME_None;

    /** Spawnable actor class prepared with this definition's Gameplay bundle. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Head",
        meta = (AssetBundles = "Gameplay"))
    TSoftClassPtr<ACMHeadPartActor> PartClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Head",
        meta = (ClampMin = "0.0", ClampMax = "360.0"))
    float VisionAngle = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Head",
        meta = (ClampMin = "0.0"))
    float VisionRange = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Head",
        meta = (ClampMin = "0.0"))
    float NearVisionRadius = 150.0f;
};
