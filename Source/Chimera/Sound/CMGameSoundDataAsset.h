#pragma once

#include "CoreMinimal.h"
#include "PrimaryDataAssetBase.h"
#include "Sound/NKMSoundCatalogProvider.h"

#include "CMGameSoundDataAsset.generated.h"

UCLASS(BlueprintType)
class CHIMERA_API UCMGameSoundDataAsset
    : public UPrimaryDataAssetBase
    , public INKMSoundCatalogProvider
{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(FPrimaryAssetType(TEXT("NKMSoundDataAsset")), GetFName());
    }

    virtual const TArray<FNKMSoundEntry>& GetSoundEntries() const override
    {
        return SoundEntries;
    }

    virtual FPrimaryAssetId GetSoundCatalogId() const override
    {
        return GetPrimaryAssetId();
    }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Sound")
    TArray<FNKMSoundEntry> SoundEntries;
};
