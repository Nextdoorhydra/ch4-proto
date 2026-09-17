#include "AsyncPDALoader.h"
#include "AsyncPDALoaderLog.h"

#include "AsyncPDALoaderTags.h"
#include "PrimaryDataAssetBase.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "AsyncLoadCompleteMessage.h"
#include "AsyncLoadRequestMessage.h"
#include "Algo/Unique.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
	// Bundle 이름을 정렬·중복 제거해 요청 병합에 사용할 동일한 residency key 생성
	void NormalizeAssetBundles(TArray<FName>& AssetBundles)
	{
		AssetBundles.Sort(FNameLexicalLess());
		AssetBundles.SetNum(Algo::Unique(AssetBundles));
	}

	// 요청 수와 성공·실패·취소 상태를 외부 완료 계약 enum으로 변환
	EAsyncLoadResult DetermineLoadResult(
		int32 RequestedCount,
		int32 LoadedCount,
		int32 FailedCount,
		bool bRequestFailed,
		bool bCancelled)
	{
		if (bCancelled)
		{
			return EAsyncLoadResult::Cancelled;
		}
		if (!bRequestFailed && FailedCount == 0 && LoadedCount == RequestedCount)
		{
			return EAsyncLoadResult::Succeeded;
		}
		if (LoadedCount > 0)
		{
			return EAsyncLoadResult::PartiallySucceeded;
		}
		return EAsyncLoadResult::Failed;
	}
}

// 로드 요청 메시지 리스너 등록
void UAsyncPDALoader::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadRequestListenerHandle = UGameplayMessageSubsystem::Get(this).RegisterListener<FAsyncLoadRequestMessage>(
		AsyncPDALoaderTags::Message_Load_Request,
		this,
		&ThisClass::HandleLoadRequest);
}

// 메시지 리스너와 진행 중 요청 정리
void UAsyncPDALoader::Deinitialize()
{
	if (LoadRequestListenerHandle.IsValid())
	{
		LoadRequestListenerHandle.Unregister();
	}

	CancelActiveLoads(false);
	ActiveScheduleProvider = nullptr;
	Super::Deinitialize();
}

// 게임이 선택한 Schedule Provider로 교체하고 이전 generation 요청 취소
void UAsyncPDALoader::SetActiveScheduleProvider(TScriptInterface<IAsyncLoadScheduleProvider> InProvider)
{
	if (ActiveScheduleProvider == InProvider)
	{
		return;
	}

	CancelActiveLoads(true);
	++ActiveGeneration;
	ActiveScheduleProvider = InProvider;
}

// 모든 진행 중 요청을 취소하고 새 세션 generation 시작
void UAsyncPDALoader::BeginNewSession()
{
	CancelActiveLoads(true);
	++ActiveGeneration;
	ActiveScheduleProvider = nullptr;
}

// Scope·Timing을 PDA 목록으로 해석하고 동일 Asset·Bundle 요청을 한 batch로 병합
void UAsyncPDALoader::HandleLoadRequest(FGameplayTag Channel, const FAsyncLoadRequestMessage& Msg)
{
	FActiveLoadRequest Request;
	Request.RequestId = NextRequestId++;
	Request.Generation = ActiveGeneration;
	Request.CorrelationId = Msg.CorrelationId.IsValid() ? Msg.CorrelationId : FGuid::NewGuid();
	Request.TimingTag = Msg.TimingTag;

	if (!ActiveScheduleProvider)
	{
		UE_LOG(LogAsyncPDALoader, Error,
			TEXT("Load request rejected: no active schedule provider. Scope=%d Timing=%s Correlation=%s"),
			Msg.Scope, *Msg.TimingTag.ToString(), *Request.CorrelationId.ToString());
		Request.bRequestFailed = true;
		BroadcastCompletion(MoveTemp(Request));
		return;
	}

	for (const FPrimaryAssetId& AssetId : ResolveAssetIds(Msg.Scope, Msg.TimingTag))
	{
		Request.RequestedAssetIds.AddUnique(AssetId);
	}

	TArray<FName> NormalizedBundles = Msg.AssetBundles;
	NormalizeAssetBundles(NormalizedBundles);

	TArray<FAssetLoadKey> NewAssetKeys;
	TArray<FPrimaryAssetId> NewAssetIds;
	for (const FPrimaryAssetId& AssetId : Request.RequestedAssetIds)
	{
		FAssetLoadKey AssetKey;
		AssetKey.AssetId = AssetId;
		AssetKey.AssetBundles = NormalizedBundles;

		if (CachedAssets.Contains(AssetId) && LoadedAssetKeys.Contains(AssetKey))
		{
			Request.LoadedAssetIds.Add(AssetId);
			continue;
		}

		Request.PendingAssetKeys.Add(AssetKey);
		AssetWaiters.FindOrAdd(AssetKey).AddUnique(Request.RequestId);
		if (!InFlightAssetKeys.Contains(AssetKey))
		{
			InFlightAssetKeys.Add(AssetKey);
			NewAssetKeys.Add(AssetKey);
			NewAssetIds.Add(AssetId);
		}
	}

	const int32 RequestId = Request.RequestId;
	ActiveRequests.Add(RequestId, MoveTemp(Request));

	if (NewAssetIds.IsEmpty())
	{
		CompleteRequestIfReady(RequestId);
		return;
	}

	const int32 BatchId = NextBatchId++;
	{
		FActiveLoadBatch& NewBatch = ActiveBatches.Add(BatchId);
		NewBatch.Generation = ActiveGeneration;
		NewBatch.AssetKeys = NewAssetKeys;
	}

	// 완료 callback이 동기 실행될 수 있으므로 batch를 키로 다시 조회한다.
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::Get().LoadPrimaryAssets(
		NewAssetIds,
		NormalizedBundles,
		FStreamableDelegate::CreateWeakLambda(this, [this, BatchId]()
		{
			OnBatchLoaded(BatchId);
		}),
		Msg.LoadPriority);

	if (FActiveLoadBatch* StillActiveBatch = ActiveBatches.Find(BatchId))
	{
		StillActiveBatch->Handle = Handle;

		// AssetManager는 요청한 에셋과 번들이 이미 준비된 경우 null 핸들을
		// 반환하면서 완료 델리게이트를 다음 틱에 실행할 수 있다. null 핸들을
		// 실패로 판정하지 않고 OnBatchLoaded가 실제 객체 상태를 확인하게 한다.
	}
	// batch가 없으면 완료 callback이 이미 처리한 상태다.
}

// AssetManager batch 완료 후 캐시를 갱신하고 기다리는 모든 요청에 결과 전달
void UAsyncPDALoader::OnBatchLoaded(int32 BatchId)
{
	FActiveLoadBatch Batch;
	if (!ActiveBatches.RemoveAndCopyValue(BatchId, Batch))
	{
		return;
	}

	if (Batch.Generation != ActiveGeneration)
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FAssetLoadKey& AssetKey : Batch.AssetKeys)
	{
		const FPrimaryAssetId& AssetId = AssetKey.AssetId;
		UPrimaryDataAssetBase* Asset = Cast<UPrimaryDataAssetBase>(AssetManager.GetPrimaryAssetObject(AssetId));
		if (Asset)
		{
			CachedAssets.Add(AssetId, Asset);
			LoadedAssetKeys.Add(AssetKey);
		}
		else
		{
			UE_LOG(LogAsyncPDALoader, Error,
				TEXT("Primary Asset load failed. Asset=%s Batch=%d Generation=%d"),
				*AssetId.ToString(), BatchId, Batch.Generation);
		}

		ResolveAssetLoadResult(AssetKey, Asset != nullptr);
	}
}

// 한 residency key의 결과를 중복 요청 waiter 전체에 반영
void UAsyncPDALoader::ResolveAssetLoadResult(const FAssetLoadKey& AssetKey, bool bSucceeded)
{
	InFlightAssetKeys.Remove(AssetKey);

	TArray<int32> WaitingRequestIds;
	AssetWaiters.RemoveAndCopyValue(AssetKey, WaitingRequestIds);
	for (const int32 RequestId : WaitingRequestIds)
	{
		FActiveLoadRequest* Request = ActiveRequests.Find(RequestId);
		if (!Request || Request->Generation != ActiveGeneration)
		{
			continue;
		}

		Request->PendingAssetKeys.Remove(AssetKey);
		if (bSucceeded)
		{
			Request->LoadedAssetIds.AddUnique(AssetKey.AssetId);
		}
		else
		{
			Request->FailedAssetIds.AddUnique(AssetKey.AssetId);
		}

		CompleteRequestIfReady(RequestId);
	}
}

// 요청의 pending key가 모두 끝났을 때 한 번만 완료 메시지 방송
void UAsyncPDALoader::CompleteRequestIfReady(int32 RequestId)
{
	FActiveLoadRequest* Request = ActiveRequests.Find(RequestId);
	if (!Request || !Request->PendingAssetKeys.IsEmpty())
	{
		return;
	}

	FActiveLoadRequest CompletedRequest;
	ActiveRequests.RemoveAndCopyValue(RequestId, CompletedRequest);
	BroadcastCompletion(MoveTemp(CompletedRequest));
}

// 내부 요청 상태를 공개 완료 메시지 계약으로 변환해 방송
void UAsyncPDALoader::BroadcastCompletion(FActiveLoadRequest&& Request, bool bCancelled)
{
	if (!UGameplayMessageSubsystem::HasInstance(this))
	{
		return;
	}

	FAsyncLoadCompleteMessage Message;
	Message.CorrelationId = Request.CorrelationId;
	Message.RequestId = Request.RequestId;
	Message.Generation = Request.Generation;
	Message.RequestedAssetIds = MoveTemp(Request.RequestedAssetIds);
	Message.LoadedAssetIds = MoveTemp(Request.LoadedAssetIds);
	Message.FailedAssetIds = MoveTemp(Request.FailedAssetIds);
	Message.TimingTag = Request.TimingTag;

	Message.Result = DetermineLoadResult(
		Message.RequestedAssetIds.Num(),
		Message.LoadedAssetIds.Num(),
		Message.FailedAssetIds.Num(),
		Request.bRequestFailed,
		bCancelled);

	if (Message.Result == EAsyncLoadResult::Failed)
	{
		UE_LOG(LogAsyncPDALoader, Error,
			TEXT("Load request failed. Timing=%s Correlation=%s Request=%d Generation=%d Requested=%d Loaded=%d Failed=%d"),
			*Message.TimingTag.ToString(), *Message.CorrelationId.ToString(),
			Message.RequestId, Message.Generation, Message.RequestedAssetIds.Num(),
			Message.LoadedAssetIds.Num(), Message.FailedAssetIds.Num());
	}
	else if (Message.Result != EAsyncLoadResult::Succeeded)
	{
		UE_LOG(LogAsyncPDALoader, Warning,
			TEXT("Load request incomplete. Result=%d Timing=%s Correlation=%s Request=%d Requested=%d Loaded=%d Failed=%d"),
			static_cast<int32>(Message.Result), *Message.TimingTag.ToString(),
			*Message.CorrelationId.ToString(), Message.RequestId,
			Message.RequestedAssetIds.Num(), Message.LoadedAssetIds.Num(),
			Message.FailedAssetIds.Num());
	}
	else
	{
		UE_LOG(LogAsyncPDALoader, Verbose,
			TEXT("Load request completed. Timing=%s Correlation=%s Request=%d Loaded=%d"),
			*Message.TimingTag.ToString(), *Message.CorrelationId.ToString(),
			Message.RequestId, Message.LoadedAssetIds.Num());
	}

	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		AsyncPDALoaderTags::Message_Load_Complete,
		Message);
}

// active handle과 waiter를 정리하고 필요하면 각 요청에 Cancelled 결과 방송
void UAsyncPDALoader::CancelActiveLoads(bool bBroadcastCancellation)
{
	for (TPair<int32, FActiveLoadBatch>& Pair : ActiveBatches)
	{
		if (Pair.Value.Handle.IsValid() && Pair.Value.Handle->IsActive())
		{
			Pair.Value.Handle->CancelHandle();
		}
	}
	ActiveBatches.Reset();
	InFlightAssetKeys.Reset();
	AssetWaiters.Reset();

	if (bBroadcastCancellation)
	{
		TArray<FActiveLoadRequest> CancelledRequests;
		CancelledRequests.Reserve(ActiveRequests.Num());
		for (TPair<int32, FActiveLoadRequest>& Pair : ActiveRequests)
		{
			CancelledRequests.Add(MoveTemp(Pair.Value));
		}
		ActiveRequests.Reset();

		for (FActiveLoadRequest& Request : CancelledRequests)
		{
			BroadcastCompletion(MoveTemp(Request), true);
		}
	}
	else
	{
		ActiveRequests.Reset();
	}
}

// 필수 preload 계약을 검증하면서 캐시된 PDA 조회
UPrimaryDataAssetBase* UAsyncPDALoader::GetCachedAsset(const FPrimaryAssetId& AssetId)
{
	UPrimaryDataAssetBase* Asset = FindCachedAsset(AssetId);
	ensureAlwaysMsgf(
		Asset,
		TEXT("UAsyncPDALoader: Required asset was not preloaded. AssetId=%s"),
		*AssetId.ToString());
	return Asset;
}

// 선택 데이터용 로그 없는 캐시 조회
UPrimaryDataAssetBase* UAsyncPDALoader::FindCachedAsset(const FPrimaryAssetId& AssetId) const
{
	const TObjectPtr<UPrimaryDataAssetBase>* Found = CachedAssets.Find(AssetId);
	return Found ? Found->Get() : nullptr;
}

// 로드 중이 아닌 PDA를 캐시와 AssetManager residency에서 해제
void UAsyncPDALoader::UnloadAssets(const TArray<FPrimaryAssetId>& AssetIds)
{
	if (AssetIds.IsEmpty())
	{
		return;
	}

	TSet<FPrimaryAssetId> InFlightAssetIds;
	for (const FAssetLoadKey& InFlightKey : InFlightAssetKeys)
	{
		InFlightAssetIds.Add(InFlightKey.AssetId);
	}

	TArray<FPrimaryAssetId> UnloadableAssetIds;
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (InFlightAssetIds.Contains(AssetId))
		{
			UE_LOG(LogAsyncPDALoader, Warning,
				TEXT("Cannot unload an asset while it is loading. Asset=%s"),
				*AssetId.ToString());
			continue;
		}

		UnloadableAssetIds.AddUnique(AssetId);
		CachedAssets.Remove(AssetId);
	}

	if (UnloadableAssetIds.IsEmpty())
	{
		return;
	}

	TArray<FAssetLoadKey> KeysToRemove;
	for (const FAssetLoadKey& LoadedKey : LoadedAssetKeys)
	{
		if (UnloadableAssetIds.Contains(LoadedKey.AssetId))
		{
			KeysToRemove.Add(LoadedKey);
		}
	}
	for (const FAssetLoadKey& KeyToRemove : KeysToRemove)
	{
		LoadedAssetKeys.Remove(KeyToRemove);
	}

	UAssetManager::Get().UnloadPrimaryAssets(UnloadableAssetIds);
}

// 현재 Provider에 Scope·Timing 해석 위임
TArray<FPrimaryAssetId> UAsyncPDALoader::ResolveAssetIds(int32 Scope, FGameplayTag TimingTag) const
{
	if (!ActiveScheduleProvider)
	{
		return TArray<FPrimaryAssetId>();
	}

	return ActiveScheduleProvider->ResolveAssetIds(Scope, TimingTag);
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAsyncLoadResultContractTest,
	"AsyncPDALoader.ResultContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Bundle 정규화와 공개 결과 enum 계약 검증
bool FAsyncLoadResultContractTest::RunTest(const FString& Parameters)
{
	TArray<FName> BundlesA = { TEXT("UI"), TEXT("Gameplay"), TEXT("UI") };
	TArray<FName> BundlesB = { TEXT("Gameplay"), TEXT("UI") };
	NormalizeAssetBundles(BundlesA);
	NormalizeAssetBundles(BundlesB);
	TestTrue(TEXT("Bundle keys ignore order and duplicates"), BundlesA == BundlesB);

	TestEqual(
		TEXT("All requested assets loaded"),
		DetermineLoadResult(2, 2, 0, false, false),
		EAsyncLoadResult::Succeeded);
	TestEqual(
		TEXT("A valid empty request completes"),
		DetermineLoadResult(0, 0, 0, false, false),
		EAsyncLoadResult::Succeeded);
	TestEqual(
		TEXT("Mixed success and failure is partial"),
		DetermineLoadResult(2, 1, 1, false, false),
		EAsyncLoadResult::PartiallySucceeded);
	TestEqual(
		TEXT("No successful assets is failure"),
		DetermineLoadResult(2, 0, 2, false, false),
		EAsyncLoadResult::Failed);
	TestEqual(
		TEXT("Missing schedule provider is failure"),
		DetermineLoadResult(0, 0, 0, true, false),
		EAsyncLoadResult::Failed);
	TestEqual(
		TEXT("Cancellation wins over other state"),
		DetermineLoadResult(2, 1, 1, false, true),
		EAsyncLoadResult::Cancelled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAsyncPDALoaderStateContractTest,
	"AsyncPDALoader.StateContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 중복 waiter, stale generation, unload·cancel 내부 상태 계약 검증
bool FAsyncPDALoaderStateContractTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UAsyncPDALoader* Loader = NewObject<UAsyncPDALoader>(TestGameInstance);
	TestNotNull(TEXT("Loader test instance is created"), Loader);
	if (!Loader)
	{
		return false;
	}

	const FPrimaryAssetId AssetId(FPrimaryAssetType(TEXT("TestAsset")), TEXT("Shared"));
	UAsyncPDALoader::FAssetLoadKey GameplayKey;
	GameplayKey.AssetId = AssetId;
	GameplayKey.AssetBundles = { TEXT("Gameplay") };

	UAsyncPDALoader::FAssetLoadKey UIKey;
	UIKey.AssetId = AssetId;
	UIKey.AssetBundles = { TEXT("UI") };
	TestFalse(TEXT("The same asset with different bundles has a distinct residency key"), GameplayKey == UIKey);

	UAsyncPDALoader::FAssetLoadKey SentinelKey;
	SentinelKey.AssetId = FPrimaryAssetId(FPrimaryAssetType(TEXT("TestAsset")), TEXT("Sentinel"));
	SentinelKey.AssetBundles = { TEXT("Gameplay") };

	const TArray<int32> DuplicateRequestIds = { 1, 2 };
	for (const int32 RequestId : DuplicateRequestIds)
	{
		UAsyncPDALoader::FActiveLoadRequest& Request = Loader->ActiveRequests.Add(RequestId);
		Request.RequestId = RequestId;
		Request.Generation = Loader->ActiveGeneration;
		Request.RequestedAssetIds = { AssetId, SentinelKey.AssetId };
		Request.PendingAssetKeys.Add(GameplayKey);
		Request.PendingAssetKeys.Add(SentinelKey);
	}
	Loader->AssetWaiters.Add(GameplayKey, DuplicateRequestIds);
	Loader->InFlightAssetKeys.Add(GameplayKey);

	Loader->ResolveAssetLoadResult(GameplayKey, true);
	TestFalse(TEXT("Resolved key leaves the in-flight set"), Loader->InFlightAssetKeys.Contains(GameplayKey));
	TestFalse(TEXT("Resolved key removes its waiter list"), Loader->AssetWaiters.Contains(GameplayKey));
	for (const int32 RequestId : DuplicateRequestIds)
	{
		const UAsyncPDALoader::FActiveLoadRequest* Request = Loader->ActiveRequests.Find(RequestId);
		TestNotNull(TEXT("Every duplicate waiter remains active for its other pending key"), Request);
		if (Request)
		{
			TestTrue(TEXT("Every duplicate waiter records the loaded asset"), Request->LoadedAssetIds.Contains(AssetId));
			TestFalse(TEXT("Resolved key is removed from every waiter"), Request->PendingAssetKeys.Contains(GameplayKey));
			TestTrue(TEXT("Unresolved key remains pending"), Request->PendingAssetKeys.Contains(SentinelKey));
		}
	}

	UAsyncPDALoader::FAssetLoadKey StaleKey;
	StaleKey.AssetId = FPrimaryAssetId(FPrimaryAssetType(TEXT("TestAsset")), TEXT("Stale"));
	StaleKey.AssetBundles = { TEXT("Gameplay") };
	UAsyncPDALoader::FActiveLoadRequest& StaleRequest = Loader->ActiveRequests.Add(3);
	StaleRequest.RequestId = 3;
	StaleRequest.Generation = Loader->ActiveGeneration - 1;
	StaleRequest.PendingAssetKeys.Add(StaleKey);
	Loader->AssetWaiters.Add(StaleKey, TArray<int32>{ 3 });
	Loader->InFlightAssetKeys.Add(StaleKey);

	Loader->ResolveAssetLoadResult(StaleKey, true);
	const UAsyncPDALoader::FActiveLoadRequest* UnchangedStaleRequest = Loader->ActiveRequests.Find(3);
	TestNotNull(TEXT("Stale request remains untouched"), UnchangedStaleRequest);
	if (UnchangedStaleRequest)
	{
		TestTrue(TEXT("Stale callback does not resolve pending state"), UnchangedStaleRequest->PendingAssetKeys.Contains(StaleKey));
		TestTrue(TEXT("Stale callback does not add a loaded asset"), UnchangedStaleRequest->LoadedAssetIds.IsEmpty());
	}

	Loader->InFlightAssetKeys.Add(GameplayKey);
	Loader->LoadedAssetKeys.Add(GameplayKey);
	Loader->UnloadAssets({ AssetId });
	TestTrue(TEXT("Unload keeps residency while the asset is in flight"), Loader->LoadedAssetKeys.Contains(GameplayKey));

	Loader->CancelActiveLoads(false);
	TestTrue(TEXT("Cancellation clears active requests"), Loader->ActiveRequests.IsEmpty());
	TestTrue(TEXT("Cancellation clears active batches"), Loader->ActiveBatches.IsEmpty());
	TestTrue(TEXT("Cancellation clears waiter state"), Loader->AssetWaiters.IsEmpty());
	TestTrue(TEXT("Cancellation clears in-flight state"), Loader->InFlightAssetKeys.IsEmpty());
	return true;
}

#endif
