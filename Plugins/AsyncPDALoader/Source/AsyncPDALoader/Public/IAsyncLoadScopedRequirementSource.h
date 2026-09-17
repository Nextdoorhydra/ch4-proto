#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "UObject/Interface.h"
#include "IAsyncLoadScopedRequirementSource.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UAsyncLoadScopedRequirementSource : public UInterface
{
	GENERATED_BODY()
};

// Scope마다 달라지는 필수 자산을 제공한다.
class ASYNCPDALOADER_API IAsyncLoadScopedRequirementSource
{
	GENERATED_BODY()

public:
	// 지정한 Scope에 필요한 자산을 반환한다.
	virtual TArray<FPrimaryAssetId> GetRequiredAssetIdsForScope(int32 Scope) = 0;

	// 검증할 Scope 목록을 반환한다.
	virtual TArray<int32> GetKnownScopes() const = 0;
};

