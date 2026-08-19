#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/AssetManagerTypes.h"
#include "UObject/Interface.h"
#include "IAsyncLoadScheduleProvider.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UAsyncLoadScheduleProvider : public UInterface
{
	GENERATED_BODY()
};

// 게임별 로드 schedule을 제공한다.
class ASYNCPDALOADER_API IAsyncLoadScheduleProvider
{
	GENERATED_BODY()

public:
	// Scope와 Timing에 해당하는 자산을 반환한다.
	virtual TArray<FPrimaryAssetId> ResolveAssetIds(int32 Scope, FGameplayTag Timing) const = 0;

	// 실제 요청 가능한 Scope와 Timing 조합을 반환한다.
	virtual TArray<TPair<int32, FGameplayTag>> GetReachableScopeTimingPairs() const = 0;

	// 데이터에 등록된 모든 Scope와 Timing 조합을 반환한다.
	virtual TArray<TPair<int32, FGameplayTag>> GetRegisteredScopeTimingPairs() const = 0;
};

