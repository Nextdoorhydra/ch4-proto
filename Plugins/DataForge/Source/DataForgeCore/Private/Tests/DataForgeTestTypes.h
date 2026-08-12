#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "DataForgeTestTypes.generated.h"

UCLASS()
class UDataForgeTestAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString DisplayName;
};

UCLASS()
class UDataForgeTestPrimaryAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString DisplayName;
};

USTRUCT()
struct FDataForgeTestRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	TSoftObjectPtr<UDataAsset> DataAsset;
};
