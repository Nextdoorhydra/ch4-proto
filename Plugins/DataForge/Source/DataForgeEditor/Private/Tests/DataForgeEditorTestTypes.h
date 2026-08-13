#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/Texture.h"
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

public:
	UPROPERTY(EditAnywhere)
	FString DisplayName;
};

UCLASS()
class UDataForgeEditorPresetAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FString DisplayName;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UTexture> Icon;
};

UCLASS()
class UDataForgeEditorAmbiguousPresetAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UTexture> PrimaryTexture;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UTexture> SecondaryTexture;
};

UCLASS()
class UDataForgeEditorManyPresetAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<TSoftObjectPtr<UTexture>> Textures;
};

UCLASS()
class UDataForgeEditorRenameTargetAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UDataForgeEditorManagedAsset> Asset;
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

USTRUCT()
struct FDataForgeEditorPresetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UDataForgeEditorPresetAsset> Data;
};
