#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GoogleSheetParserBase.h"
#include "GoogleDataForgeIntegrationTestTypes.generated.h"

UCLASS()
class UGoogleDataForgeTestParser : public UGoogleSheetParserBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TObjectPtr<UDataTable> TargetTable;

	bool bCompleted = false;
	int32 CapturedRowCount = 0;
	TArray<FString> CapturedHeaders;

protected:
	virtual bool OnParseComplete(FString& OutError) override
	{
		bCompleted = true;
		CapturedRowCount = GetRowCount();
		CapturedHeaders = GetHeaders();
		return true;
	}
};

UCLASS()
class UGoogleDataForgeTestDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FString DisplayName;

	UPROPERTY(EditAnywhere)
	int32 Price = 0;

	UPROPERTY(EditAnywhere)
	FString Category;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> Icon = nullptr;
};

UCLASS()
class UGoogleDataForgeTestPrimaryAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FString DisplayName;

	UPROPERTY(EditAnywhere)
	int32 Price = 0;

	UPROPERTY(EditAnywhere)
	FString Category;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> Icon = nullptr;
};

USTRUCT()
struct FGoogleDataForgeTestRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString DisplayName;

	UPROPERTY(EditAnywhere)
	int32 Price = 0;

	UPROPERTY(EditAnywhere)
	FString Category;

	UPROPERTY(EditAnywhere)
	FString Rarity;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UDataAsset> DataAsset;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UPrimaryDataAsset> PrimaryAsset;
};
