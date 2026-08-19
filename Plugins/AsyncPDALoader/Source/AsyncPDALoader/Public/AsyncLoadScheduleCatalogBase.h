#pragma once

#include "CoreMinimal.h"
#include "PrimaryDataAssetBase.h"
#include "IAsyncLoadScheduleProvider.h"
#include "AsyncLoadScheduleEntry.h"
#include "AsyncLoadScheduleCatalogBase.generated.h"

// PDA별 Scope와 Timing을 관리하는 schedule catalog 기본 클래스다.
UCLASS(Abstract)
class ASYNCPDALOADER_API UAsyncLoadScheduleCatalogBase : public UPrimaryDataAssetBase, public IAsyncLoadScheduleProvider
{
	GENERATED_BODY()

public:
	// PrimaryAssetType별 schedule 항목.
	UPROPERTY(EditAnywhere, Category = "Schedule", meta = (TitleProperty = "PrimaryAssetType"))
	TArray<FAsyncLoadScheduleCategory> Categories;

#if WITH_EDITOR
	// 등록된 PDA를 다시 스캔하며 기존 schedule 값은 유지한다.
	UFUNCTION(CallInEditor, Category = "Schedule")
	void RefreshPDACatalog();
#endif

	//~ IAsyncLoadScheduleProvider interface
	virtual TArray<FPrimaryAssetId> ResolveAssetIds(int32 Scope, FGameplayTag Timing) const override;
	virtual TArray<TPair<int32, FGameplayTag>> GetRegisteredScopeTimingPairs() const override;
	// 게임별 요청 가능 조합은 하위 클래스가 제공한다.
	virtual TArray<TPair<int32, FGameplayTag>> GetReachableScopeTimingPairs() const override { return {}; }
	//~ End IAsyncLoadScheduleProvider interface

protected:
	// catalog 스캔에서 제외할 PrimaryAssetType을 반환한다.
	virtual TSet<FName> GetExcludedPrimaryAssetTypes() const { return {}; }
};

