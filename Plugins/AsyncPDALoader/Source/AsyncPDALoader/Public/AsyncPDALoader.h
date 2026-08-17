#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/StreamableManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "UObject/ScriptInterface.h"
#include "GameplayMessageRuntime/Public/GameFramework/GameplayMessageSubsystem.h"
#include "IAsyncLoadScheduleProvider.h"
#include "AsyncPDALoader.generated.h"

class UPrimaryDataAssetBase;
struct FAsyncLoadRequestMessage;

// Schedule provider가 선택한 PDA를 비동기로 로드하고 완료 결과를 방송한다.
UCLASS()
class ASYNCPDALOADER_API UAsyncPDALoader : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 이후 요청에 사용할 schedule provider를 설정한다.
	void SetActiveScheduleProvider(TScriptInterface<IAsyncLoadScheduleProvider> InProvider);
	void BeginNewSession();

	// 현재 schedule provider를 반환한다.
	TScriptInterface<IAsyncLoadScheduleProvider> GetActiveScheduleProvider() const { return ActiveScheduleProvider; }

	// 캐시 조회 전용. 누락은 preload invariant 위반으로 보고 동기 fallback 없이 nullptr을 반환한다.
	UPrimaryDataAssetBase* GetCachedAsset(const FPrimaryAssetId& AssetId);

	UPrimaryDataAssetBase* FindCachedAsset(const FPrimaryAssetId& AssetId) const;

	// 캐시와 AssetManager에서 자산을 해제한다.
	void UnloadAssets(const TArray<FPrimaryAssetId>& AssetIds);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FAsyncPDALoaderStateContractTest;
#endif

	struct FAssetLoadKey
	{
		FPrimaryAssetId AssetId;
		TArray<FName> AssetBundles;

		bool operator==(const FAssetLoadKey& Other) const
		{
			return AssetId == Other.AssetId && AssetBundles == Other.AssetBundles;
		}

		friend uint32 GetTypeHash(const FAssetLoadKey& Key)
		{
			uint32 Hash = GetTypeHash(Key.AssetId);
			for (const FName Bundle : Key.AssetBundles)
			{
				Hash = HashCombine(Hash, GetTypeHash(Bundle));
			}
			return Hash;
		}
	};

	struct FActiveLoadRequest
	{
		int32 RequestId = INDEX_NONE;
		int32 Generation = 0;
		FGuid CorrelationId;
		FGameplayTag TimingTag;
		TArray<FPrimaryAssetId> RequestedAssetIds;
		TSet<FAssetLoadKey> PendingAssetKeys;
		TArray<FPrimaryAssetId> LoadedAssetIds;
		TArray<FPrimaryAssetId> FailedAssetIds;
		bool bRequestFailed = false;
	};

	struct FActiveLoadBatch
	{
		int32 Generation = 0;
		TArray<FAssetLoadKey> AssetKeys;
		TSharedPtr<FStreamableHandle> Handle;
	};

	void HandleLoadRequest(FGameplayTag Channel, const FAsyncLoadRequestMessage& Msg);
	void OnBatchLoaded(int32 BatchId);
	void ResolveAssetLoadResult(const FAssetLoadKey& AssetKey, bool bSucceeded);
	void CompleteRequestIfReady(int32 RequestId);
	void BroadcastCompletion(FActiveLoadRequest&& Request, bool bCancelled = false);
	void CancelActiveLoads(bool bBroadcastCancellation);

	// Scope와 Timing에 해당하는 자산을 조회한다.
	TArray<FPrimaryAssetId> ResolveAssetIds(int32 Scope, FGameplayTag TimingTag) const;

	UPROPERTY()
	TScriptInterface<IAsyncLoadScheduleProvider> ActiveScheduleProvider;

	UPROPERTY()
	TMap<FPrimaryAssetId, TObjectPtr<UPrimaryDataAssetBase>> CachedAssets;

	TMap<int32, FActiveLoadRequest> ActiveRequests;
	TMap<int32, FActiveLoadBatch> ActiveBatches;
	TMap<FAssetLoadKey, TArray<int32>> AssetWaiters;
	TSet<FAssetLoadKey> InFlightAssetKeys;
	TSet<FAssetLoadKey> LoadedAssetKeys;
	int32 ActiveGeneration = 1;
	int32 NextRequestId = 1;
	int32 NextBatchId = 1;

	FGameplayMessageListenerHandle LoadRequestListenerHandle;
};

