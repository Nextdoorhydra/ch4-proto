#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PrimaryDataAssetBase.generated.h"

// UAsyncPDALoader가 캐싱하는 모든 PDA의 공통 베이스(마커).
// Primary Data Asset으로 관리할 때 이 클래스를 상속하면 UAsyncPDALoader::GetCachedAsset()으로 조회 가능해진다.
UCLASS(Abstract)
class ASYNCPDALOADER_API UPrimaryDataAssetBase : public UPrimaryDataAsset
{
	GENERATED_BODY()
};

