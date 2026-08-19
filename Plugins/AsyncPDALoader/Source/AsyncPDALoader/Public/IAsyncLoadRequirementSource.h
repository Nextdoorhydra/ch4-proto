#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "UObject/Interface.h"
#include "IAsyncLoadRequirementSource.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UAsyncLoadRequirementSource : public UInterface
{
	GENERATED_BODY()
};

// schedule에 포함돼야 하는 필수 자산을 제공한다.
class ASYNCPDALOADER_API IAsyncLoadRequirementSource
{
	GENERATED_BODY()

public:
	virtual TArray<FPrimaryAssetId> GetRequiredAssetIds() const = 0;
};

