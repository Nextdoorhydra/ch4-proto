#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataForgeFolderSource.generated.h"

class UDataForgeNamingPolicy;

UENUM(BlueprintType)
enum class EDataForgeLayoutSubjectSource : uint8
{
	AssetName,
	FolderSegment,
	Fixed
};

UCLASS(BlueprintType)
class DATAFORGECORE_API UDataForgeAssetLayoutRecipe : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FName RecipeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FName Domain = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	TSoftObjectPtr<UDataForgeNamingPolicy> NamingPolicy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subject")
	EDataForgeLayoutSubjectSource SubjectSource = EDataForgeLayoutSubjectSource::AssetName;

	/** Zero-based folder below the configured inventory root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subject", meta = (EditCondition = "SubjectSource == EDataForgeLayoutSubjectSource::FolderSegment", EditConditionHides, ClampMin = "0"))
	int32 SubjectFolderIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subject", meta = (EditCondition = "SubjectSource == EDataForgeLayoutSubjectSource::Fixed", EditConditionHides))
	FString FixedSubject;

	/** Optional zero-based folder below the root that must match the Asset Kind folder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Kind", meta = (ClampMin = "-1"))
	int32 KindFolderIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Kind", meta = (EditCondition = "KindFolderIndex >= 0"))
	bool bRequireKindFolderMatch = true;
};

UCLASS(BlueprintType)
class DATAFORGECORE_API UDataForgeFolderSourceConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Content Browser folder such as /Game/Chimera/Character. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Folder Inventory")
	FString RootFolder;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Folder Inventory")
	bool bRecursive = true;

	/** Empty means every kind declared by the Naming Policy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Folder Inventory")
	TArray<FName> AllowedAssetKinds;

	/** Content Browser folders excluded together with their descendants. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Folder Inventory")
	TArray<FString> ExcludedFolders;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Folder Inventory")
	bool bExcludeDataForgeManagedAssets = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Folder Inventory")
	TSoftObjectPtr<UDataForgeAssetLayoutRecipe> LayoutRecipe;
};

class DATAFORGECORE_API FDataForgeAssetLayoutRecipeLibrary
{
public:
	static void ConfigureChimeraCharacter(UDataForgeAssetLayoutRecipe& Recipe);
	static void ConfigureChimeraObstacle(UDataForgeAssetLayoutRecipe& Recipe);
	static void ConfigureChimeraAbility(UDataForgeAssetLayoutRecipe& Recipe);
	static void ConfigureChimeraUI(UDataForgeAssetLayoutRecipe& Recipe);
};
