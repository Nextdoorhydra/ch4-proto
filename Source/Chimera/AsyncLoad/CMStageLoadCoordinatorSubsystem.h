#pragma once

#include "CoreMinimal.h"
#include "AsyncLoadCompleteMessage.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "AsyncLoad/CMStageLoadTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "CMStageLoadCoordinatorSubsystem.generated.h"

class UAsyncPDALoader;
class UCMStageLoadSchedule;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FCMStageLoadGroupFinished,
	FName, LoadGroupId,
	EAsyncLoadResult, Result,
	bool, bReleasedImmediately);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FCMStageLoadQueueFinished,
	FGameplayTag, TimingTag,
	bool, bSucceeded);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FCMStageStartRequiredFinished,
	FGuid, RequestId,
	bool, bSucceeded);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FCMStageScheduleFinished,
	FGuid, RequestId,
	bool, bSucceeded);

UCLASS()
// 각 머신에서 Stage Schedule을 실제 AsyncPDALoader 요청과 순차 큐로 변환
class CHIMERA_API UCMStageLoadCoordinatorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 현재 스테이지 Schedule을 활성화하고 이전 스테이지 전용 에셋 정리
	UFUNCTION(BlueprintCallable, Category = "Chimera|Loading")
	bool ActivateStageSchedule(UCMStageLoadSchedule* NewSchedule);

	// Session을 포함한 모든 로드 상태와 캐시를 새 캠페인 기준으로 초기화
	UFUNCTION(BlueprintCallable, Category = "Chimera|Loading")
	void ResetStageRouteLoading();

	// TimingTag에 속한 그룹을 LoadOrder 순서로 하나씩 로드
	UFUNCTION(BlueprintCallable, Category = "Chimera|Loading")
	bool StartAutomaticLoadQueue();

	// Schedule PDA를 비동기로 준비한 뒤 지정 Timing 큐를 실행
	UFUNCTION(BlueprintCallable, Category = "Chimera|Loading")
	bool StartStageScheduleRequest(FPrimaryAssetId ScheduleId, FGuid RequestId);

	// 분기 선택이나 ActivationGate가 특정 그룹을 직접 요청할 때 사용
	UFUNCTION(BlueprintCallable, Category = "Chimera|Loading")
	FGuid RequestLoadGroup(FName LoadGroupId);

	// 이미 준비된 그룹을 해제. 로드 중이면 완료 직후 해제하도록 예약
	UFUNCTION(BlueprintCallable, Category = "Chimera|Loading")
	void ReleaseLoadGroup(FName LoadGroupId);

	// 되돌릴 수 없는 분기에서 선택되지 않은 그룹을 이후 큐에서도 제외하고 해제
	UFUNCTION(BlueprintCallable, Category = "Chimera|Loading")
	void RejectBranchLoadGroups(const TArray<FName>& RejectedLoadGroupIds);

	// 로컬 머신의 현재 그룹 준비 상태 조회
	UFUNCTION(BlueprintPure, Category = "Chimera|Loading")
	ECMStageLoadGroupState GetLoadGroupState(FName LoadGroupId) const;

	UFUNCTION(BlueprintPure, Category = "Chimera|Loading")
	bool IsLoadGroupReady(FName LoadGroupId) const;

	// 개별 그룹의 로컬 로드 또는 실패 완료 알림
	UPROPERTY(BlueprintAssignable, Category = "Chimera|Loading")
	FCMStageLoadGroupFinished OnLoadGroupFinished;

	// 한 TimingTag의 순차 큐가 모두 소비된 시점 알림
	UPROPERTY(BlueprintAssignable, Category = "Chimera|Loading")
	FCMStageLoadQueueFinished OnLoadQueueFinished;

	UPROPERTY(BlueprintAssignable, Category = "Chimera|Loading")
	FCMStageStartRequiredFinished OnStageStartRequiredFinished;

	UPROPERTY(BlueprintAssignable, Category = "Chimera|Loading")
	FCMStageScheduleFinished OnStageScheduleFinished;

private:
	// AsyncPDALoader 완료 메시지를 현재 로컬 요청과 CorrelationId로 연결
	void HandleLoadComplete(FGameplayTag Channel, const FAsyncLoadCompleteMessage& Message);

	// 현재 큐에서 이미 준비·제외된 그룹을 건너뛰고 다음 요청 실행
	void RequestNextQueuedGroup();

	// 현재 스테이지 그룹만 해제하고 캠페인 Session 에셋 유지
	void ReleaseCurrentStageAssets();

	// 그룹이 보유한 PrimaryAssetId를 AsyncPDALoader에서 실제 해제
	void UnloadGroupAssets(FName LoadGroupId);

	// Primary Asset 로드 완료 후 Schedule을 활성화하고 Timing 큐 시작
	void HandleScheduleLoaded();
	void CancelActiveStageExecution(bool bNotifyFailure);

	UAsyncPDALoader* GetAsyncLoader() const;

	UPROPERTY(Transient)
	TObjectPtr<UCMStageLoadSchedule> ActiveSchedule;

	TMap<FGuid, FName> PendingRequests;
	TMap<FName, ECMStageLoadGroupState> GroupStates;
	TMap<FName, TArray<FPrimaryAssetId>> LoadedAssetsByGroup;
	TSet<FName> PendingReleaseGroups;
	TSet<FName> SuppressedLoadGroups;

	TArray<FName> QueuedLoadGroups;
	FName ActiveQueuedGroup;
	bool bActiveQueueFailed = false;
	bool bStartRequiredReported = false;
	bool bChangingSchedule = false;
	FGuid ActiveStageRequestId;
	FPrimaryAssetId PendingScheduleId;
	TSet<FName> PendingStartRequiredGroups;
	TSharedPtr<FStreamableHandle> ScheduleLoadHandle;

	FGameplayMessageListenerHandle LoadCompleteListenerHandle;
};
