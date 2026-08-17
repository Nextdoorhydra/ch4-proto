#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/AssetManagerTypes.h"
#include "AsyncLoadCompleteMessage.generated.h"

UENUM(BlueprintType)
enum class EAsyncLoadResult : uint8
{
	Succeeded,
	PartiallySucceeded,
	Failed,
	Cancelled
};

// 비동기 PDA 요청의 완료 결과다.
USTRUCT(BlueprintType)
struct FAsyncLoadCompleteMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Load")
	FGuid CorrelationId;

	UPROPERTY(BlueprintReadOnly, Category = "Load")
	int32 RequestId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Load")
	int32 Generation = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Load")
	TArray<FPrimaryAssetId> RequestedAssetIds;

	UPROPERTY(BlueprintReadOnly, Category = "Load")
	TArray<FPrimaryAssetId> LoadedAssetIds;

	UPROPERTY(BlueprintReadOnly, Category = "Load")
	TArray<FPrimaryAssetId> FailedAssetIds;

	UPROPERTY(BlueprintReadOnly, Category = "Load")
	FGameplayTag TimingTag;

	UPROPERTY(BlueprintReadOnly, Category = "Load")
	EAsyncLoadResult Result = EAsyncLoadResult::Failed;
};

