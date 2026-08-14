#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataForgeTypes.h"
#include "DataForgeNamingPolicy.generated.h"

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeAssetKindNamingRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Naming")
	FName AssetKind = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Naming")
	FString TypePrefix;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Naming")
	FString FolderName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Naming")
	TSoftClassPtr<UObject> ExpectedAssetClass;
};

UCLASS(BlueprintType)
class DATAFORGECORE_API UDataForgeNamingPolicy : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Naming")
	FString ProjectPrefix;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Naming")
	TArray<FDataForgeAssetKindNamingRule> AssetKinds;
};

struct DATAFORGECORE_API FDataForgeAssetIdentity
{
	FName AssetKind = NAME_None;
	FString TypePrefix;
	FString ProjectPrefix;
	FString Subject;
	FString Role;
	FString Variant;
	int32 Numbering = INDEX_NONE;
};

struct DATAFORGECORE_API FDataForgeNamingParseContext
{
	/** Optional kind constraint. When omitted, the type prefix selects the kind. */
	FName AssetKind = NAME_None;

	/** Optional boundary supplied by a folder recipe or entity source. */
	FString Subject;

	/** Optional boundary within the text following Subject. */
	FString Role;

	/** Optional loaded class used to validate the selected kind. */
	UClass* ActualAssetClass = nullptr;
};

struct DATAFORGECORE_API FDataForgeNamingResult
{
	bool bSuccess = false;
	FString AssetName;
	FDataForgeAssetIdentity Identity;
	TArray<FDataForgeDiagnostic> Diagnostics;
};

class DATAFORGECORE_API FDataForgeNamingPolicyResolver
{
public:
	/** Replaces the Policy definition with the built-in Chimera project convention. */
	static void ConfigureChimeraDefaults(UDataForgeNamingPolicy& Policy);
	static FDataForgeResult ValidatePolicy(const UDataForgeNamingPolicy& Policy);
	static FDataForgeNamingResult Parse(
		const UDataForgeNamingPolicy& Policy,
		const FString& AssetName,
		const FDataForgeNamingParseContext& Context = {});
	static FDataForgeNamingResult Build(
		const UDataForgeNamingPolicy& Policy,
		const FDataForgeAssetIdentity& Identity);
};
