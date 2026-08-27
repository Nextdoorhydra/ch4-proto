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
};
