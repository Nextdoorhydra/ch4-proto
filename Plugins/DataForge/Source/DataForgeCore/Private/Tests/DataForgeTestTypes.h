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

UCLASS()
class UDataForgeExpandedTestPrimaryAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FString Description;
};

UCLASS()
class UDataForgeAssociationTestAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UDataForgeTestAsset> PrimaryAsset;

	UPROPERTY(EditAnywhere)
	TArray<TSoftObjectPtr<UDataForgeTestAsset>> Assets;
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

USTRUCT()
struct FDataForgeExpandedTestRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FString Description;

	UPROPERTY()
	TSoftObjectPtr<UDataAsset> DataAsset;
};
