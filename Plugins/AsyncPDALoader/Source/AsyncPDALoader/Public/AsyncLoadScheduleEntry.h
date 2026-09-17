#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/AssetManagerTypes.h"
#include "AsyncLoadScheduleEntry.generated.h"

// PDA의 로드 Scope와 Timing을 지정한다.
USTRUCT(BlueprintType)
struct ASYNCPDALOADER_API FAsyncLoadScheduleEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Schedule")
	FPrimaryAssetId AssetId;

	// 게임별 Schedule이 사람이 읽을 수 있는 그룹 식별자로 Scope와 Timing을 자동 구성할 때 사용한다.
	UPROPERTY(EditAnywhere, Category = "Schedule")
	FName GroupId;

	// Scope의 의미는 게임이 정의하며 INDEX_NONE은 공통 범위를 뜻한다.
	UPROPERTY(EditAnywhere, Category = "Schedule")
	int32 Scope = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category = "Schedule")
	FGameplayTag Timing;

	// Timing이 비어 있으면 미할당 항목이다.
	bool IsAssigned() const { return Timing.IsValid(); }

	bool operator==(const FAsyncLoadScheduleEntry& Other) const
	{
		return AssetId == Other.AssetId;
	}
};

// 같은 PrimaryAssetType의 schedule 항목을 묶는다.
USTRUCT(BlueprintType)
struct ASYNCPDALOADER_API FAsyncLoadScheduleCategory
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Schedule")
	FName PrimaryAssetType;

	UPROPERTY(EditAnywhere, Category = "Schedule")
	TArray<FAsyncLoadScheduleEntry> Entries;
};
