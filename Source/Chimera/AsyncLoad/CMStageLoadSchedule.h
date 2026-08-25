#pragma once

#include "CoreMinimal.h"
#include "AsyncLoadScheduleCatalogBase.h"
#include "AsyncLoad/CMStageLoadTypes.h"

#include "CMStageLoadSchedule.generated.h"

UCLASS(BlueprintType)
// 한 스테이지의 PDA 배치와 순차 로드 그룹을 AsyncPDALoader에 제공
class CHIMERA_API UCMStageLoadSchedule : public UAsyncLoadScheduleCatalogBase
{
	GENERATED_BODY()

public:
	// 스테이지 정의·CSV·로그를 연결하는 안정적인 식별자
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	FName StageId;

	// Entry와 Background를 포함한 스테이지 로드 그룹 목록
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Load", meta = (TitleProperty = "LoadGroupId"))
	TArray<FCMStageLoadGroupDefinition> LoadGroups;

#if WITH_EDITOR
	// PDA catalog를 다시 검색한 뒤 LoadGroupId 기준으로 Scope와 Timing을 자동 동기화
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Load")
	void RefreshAndRebuildCatalog();

	// 현재 LoadGroups와 catalog assignment만 다시 계산하고 잘못된 GroupId를 미할당 처리
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Load")
	void RebuildLoadGroupScopes();
#endif

	// LoadGroupId에 해당하는 그룹 정의 검색
	const FCMStageLoadGroupDefinition* FindLoadGroup(FName LoadGroupId) const;

	// 같은 TimingTag 그룹을 LoadOrder 순서로 반환
	TArray<const FCMStageLoadGroupDefinition*> GetOrderedAutomaticGroups() const;

	// 에디터 검증기가 실제 요청 가능한 Scope·Timing 조합을 확인할 수 있도록 제공
	virtual TArray<TPair<int32, FGameplayTag>> GetReachableScopeTimingPairs() const override;

protected:
	// Schedule PDA 자신이 자기 catalog에 들어가는 순환 참조 방지
	virtual TSet<FName> GetExcludedPrimaryAssetTypes() const override;
};
