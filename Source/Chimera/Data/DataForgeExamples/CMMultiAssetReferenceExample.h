#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "CMMultiAssetReferenceExample.generated.h"

class UMaterialInterface;
class UTexture2D;

UCLASS(BlueprintType)
class CHIMERA_API UCMMultiAssetReferencePrimaryAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TSoftObjectPtr<UTexture2D>> Textures;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMMultiAssetReferenceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UCMMultiAssetReferencePrimaryAsset> PrimaryAsset;
};
