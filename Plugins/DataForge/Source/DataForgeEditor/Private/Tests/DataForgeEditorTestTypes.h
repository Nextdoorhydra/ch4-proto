#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "DataForgeEditorTestTypes.generated.h"

UENUM()
enum class EDataForgeEditorRarity : uint8
{
	Common,
	Rare
};

UCLASS()
class UDataForgeEditorManagedAsset : public UDataAsset
{
	GENERATED_BODY()
};

USTRUCT()
struct FDataForgeEditorAutoMapRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString DisplayName;

	UPROPERTY(EditAnywhere)
	int32 Price = 0;

	UPROPERTY(EditAnywhere)
	float Ratio = 0.0f;

	UPROPERTY(EditAnywhere)
	bool bEnabled = false;

	UPROPERTY(EditAnywhere)
	EDataForgeEditorRarity Rarity = EDataForgeEditorRarity::Common;

	UPROPERTY(Transient)
	FString TransientValue;
};

USTRUCT()
struct FDataForgeEditorGeneratedOutputRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString DisplayName;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UDataForgeEditorManagedAsset> Data;
};
