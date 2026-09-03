#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"

#include "AsyncLoadRequestMessage.h"
#include "AsyncPDALoader.h"
#include "AsyncPDALoaderTags.h"
#include "IAsyncLoadScheduleProvider.h"
#include "AsyncLoad/CMStageLoadSchedule.h"
#include "AsyncLoad/CMStageLoadTags.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

// 완료 메시지 리스너 등록
void UCMStageLoadCoordinatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UAsyncPDALoader>();
	Super::Initialize(Collection);

	LoadCompleteListenerHandle = UGameplayMessageSubsystem::Get(this).RegisterListener<FAsyncLoadCompleteMessage>(
		AsyncPDALoaderTags::Message_Load_Complete,
		this,
		&ThisClass::HandleLoadComplete);
}

// 리스너와 캠페인 로드 캐시 정리
void UCMStageLoadCoordinatorSubsystem::Deinitialize()
{
	if (LoadCompleteListenerHandle.IsValid())
	{
		LoadCompleteListenerHandle.Unregister();
	}

	ResetStageRouteLoading();
	Super::Deinitialize();
}

// 이전 스테이지 전용 에셋을 해제하고 새 Schedule을 로더 Provider로 연결
bool UCMStageLoadCoordinatorSubsystem::ActivateStageSchedule(UCMStageLoadSchedule* NewSchedule)
{
	UAsyncPDALoader* Loader = GetAsyncLoader();
	if (!IsValid(NewSchedule) || !Loader)
	{
		return false;
	}

	bChangingSchedule = true;

	// Provider 교체가 이전 세대의 in-flight 요청을 먼저 취소한 뒤 캐시를 해제해야
	// 로드 중인 AssetId가 UnloadAssets에서 거부되어 남는 일을 피할 수 있다.
	TScriptInterface<IAsyncLoadScheduleProvider> Provider;
	Provider.SetObject(NewSchedule);
	Provider.SetInterface(Cast<IAsyncLoadScheduleProvider>(NewSchedule));
	Loader->SetActiveScheduleProvider(Provider);
	ReleaseCurrentStageAssets();

	PendingRequests.Reset();
	GroupStates.Reset();
	LoadedAssetsByGroup.Reset();
	PendingReleaseGroups.Reset();
	SuppressedLoadGroups.Reset();
	QueuedLoadGroups.Reset();
	ActiveQueuedGroup = NAME_None;
	bActiveQueueFailed = false;
	bStartRequiredReported = false;
	PendingStartRequiredGroups.Reset();

	ActiveSchedule = NewSchedule;
	for (const FCMStageLoadGroupDefinition& Group : ActiveSchedule->LoadGroups)
	{
		GroupStates.Add(Group.LoadGroupId, ECMStageLoadGroupState::NotRequested);
	}

	bChangingSchedule = false;
	return true;
}

// 캠페인 종료·재시작 시 Session을 포함한 모든 추적 에셋과 진행 중 요청 초기화
void UCMStageLoadCoordinatorSubsystem::ResetStageRouteLoading()
{
	UAsyncPDALoader* Loader = GetAsyncLoader();
	bChangingSchedule = true;

	TArray<FPrimaryAssetId> AssetsToUnload;
	for (const TPair<FName, TArray<FPrimaryAssetId>>& Pair : LoadedAssetsByGroup)
	{
		for (const FPrimaryAssetId& AssetId : Pair.Value)
		{
			AssetsToUnload.AddUnique(AssetId);
		}
	}

	// BeginNewSession은 진행 중 요청을 먼저 취소한다. 그 후 이미 완료된 캐시만 안전하게 해제한다.
	if (Loader)
	{
		Loader->BeginNewSession();
		Loader->UnloadAssets(AssetsToUnload);
	}

	ActiveSchedule = nullptr;
	PendingRequests.Reset();
	GroupStates.Reset();
	LoadedAssetsByGroup.Reset();
	PendingReleaseGroups.Reset();
	SuppressedLoadGroups.Reset();
	QueuedLoadGroups.Reset();
	ActiveQueuedGroup = NAME_None;
	bActiveQueueFailed = false;
	bStartRequiredReported = false;
	ActiveStageRequestId.Invalidate();
	PendingScheduleId = FPrimaryAssetId();
	PendingStartRequiredGroups.Reset();
	bChangingSchedule = false;
}

// TimingTag에 해당하는 그룹을 데이터에 정의한 LoadOrder 순서로 큐 구성
bool UCMStageLoadCoordinatorSubsystem::StartAutomaticLoadQueue()
{
	if (!ActiveSchedule || !ActiveQueuedGroup.IsNone() || !QueuedLoadGroups.IsEmpty())
	{
		return false;
	}

	const TArray<const FCMStageLoadGroupDefinition*> Groups =
		ActiveSchedule->GetOrderedAutomaticGroups();
	for (const FCMStageLoadGroupDefinition* Group : Groups)
	{
		if (Group && !SuppressedLoadGroups.Contains(Group->LoadGroupId))
		{
			QueuedLoadGroups.Add(Group->LoadGroupId);
			if (Group->LoadPolicy == ECMStageLoadPolicy::BeforeStageStart)
			{
				PendingStartRequiredGroups.Add(Group->LoadGroupId);
			}
		}
	}

	bActiveQueueFailed = false;
	bStartRequiredReported = false;
	if (PendingStartRequiredGroups.IsEmpty())
	{
		bStartRequiredReported = true;
		OnStageStartRequiredFinished.Broadcast(ActiveStageRequestId, true);
	}
	RequestNextQueuedGroup();
	return true;
}

// 식별 가능한 요청 하나를 로컬 Schedule 로드와 Timing 큐로 변환
bool UCMStageLoadCoordinatorSubsystem::StartStageScheduleRequest(
	FPrimaryAssetId ScheduleId,
	FGuid RequestId)
{
	if (!ScheduleId.IsValid() || !RequestId.IsValid())
	{
		return false;
	}

	// Seamless Travel 중 구/신 로컬 컨트롤러가 같은 복제 요청을 차례로 전달할 수 있다.
	// 이미 실행 중인 동일 요청은 취소·재시작하지 않고 기존 실행을 공유한다.
	if (ActiveStageRequestId == RequestId)
	{
		UE_LOG(LogChimeraStageLoad, Display,
			TEXT("Duplicate local stage schedule request ignored. NetMode=%d Request=%s Schedule=%s"),
			GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : INDEX_NONE,
			*RequestId.ToString(), *ScheduleId.ToString());
		return true;
	}

	// 이전 스테이지의 Sequential 큐가 남아 있어도 새 스테이지 요청이 안전하게 대체한다.
	if (ActiveStageRequestId.IsValid() || ScheduleLoadHandle.IsValid()
		|| !ActiveQueuedGroup.IsNone() || !QueuedLoadGroups.IsEmpty())
	{
		CancelActiveStageExecution(true);
	}

	ActiveStageRequestId = RequestId;
	PendingScheduleId = ScheduleId;

	UE_LOG(LogChimeraStageLoad, Display,
		TEXT("Local stage schedule started. NetMode=%d Request=%s Schedule=%s"),
		GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : INDEX_NONE,
		*RequestId.ToString(),
		*ScheduleId.ToString());

	// 같은 테스트 스테이지를 다시 여는 경우 Schedule PDA는 GameInstance의
	// AssetManager에 이미 남아 있을 수 있다. 이 상태를 로드 실패로 보지 않고
	// 기존 객체를 새 스테이지 실행 상태로 다시 활성화한다.
	if (UCMStageLoadSchedule* LoadedSchedule = Cast<UCMStageLoadSchedule>(
		UAssetManager::Get().GetPrimaryAssetObject(ScheduleId)))
	{
		if (ActivateStageSchedule(LoadedSchedule) && StartAutomaticLoadQueue())
		{
			return true;
		}

		ActiveStageRequestId.Invalidate();
		PendingScheduleId = FPrimaryAssetId();
		return false;
	}

	ScheduleLoadHandle = UAssetManager::Get().LoadPrimaryAsset(
		ScheduleId, TArray<FName>(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleScheduleLoaded));
	if (ScheduleLoadHandle.IsValid())
	{
		return true;
	}

	const FGuid FailedRequestId = ActiveStageRequestId;
	ActiveStageRequestId.Invalidate();
	PendingScheduleId = FPrimaryAssetId();
	OnStageStartRequiredFinished.Broadcast(FailedRequestId, false);
	OnStageScheduleFinished.Broadcast(FailedRequestId, false);
	return false;
}

void UCMStageLoadCoordinatorSubsystem::CancelActiveStageExecution(bool bNotifyFailure)
{
	const FGuid CancelledRequestId = ActiveStageRequestId;
	const bool bShouldReportStartFailure = CancelledRequestId.IsValid() && !bStartRequiredReported;

	if (ScheduleLoadHandle.IsValid())
	{
		ScheduleLoadHandle->CancelHandle();
	}
	if (UAsyncPDALoader* Loader = GetAsyncLoader())
	{
		Loader->BeginNewSession();
	}

	PendingRequests.Reset();
	QueuedLoadGroups.Reset();
	PendingStartRequiredGroups.Reset();
	ActiveQueuedGroup = NAME_None;
	bActiveQueueFailed = false;
	bStartRequiredReported = false;
	ActiveStageRequestId.Invalidate();
	PendingScheduleId = FPrimaryAssetId();
	ScheduleLoadHandle.Reset();

	if (bNotifyFailure && CancelledRequestId.IsValid())
	{
		if (bShouldReportStartFailure)
		{
			OnStageStartRequiredFinished.Broadcast(CancelledRequestId, false);
		}
		OnStageScheduleFinished.Broadcast(CancelledRequestId, false);
	}
}

// LoadGroup 정의를 AsyncPDALoader의 Scope·Timing 요청 메시지로 변환
FGuid UCMStageLoadCoordinatorSubsystem::RequestLoadGroup(FName LoadGroupId)
{
	if (!ActiveSchedule || SuppressedLoadGroups.Contains(LoadGroupId))
	{
		return FGuid();
	}

	const FCMStageLoadGroupDefinition* Group = ActiveSchedule->FindLoadGroup(LoadGroupId);
	const ECMStageLoadGroupState State = GetLoadGroupState(LoadGroupId);
	if (!Group || State == ECMStageLoadGroupState::Loading || State == ECMStageLoadGroupState::Ready
		|| State == ECMStageLoadGroupState::PendingRelease)
	{
		return FGuid();
	}

	FAsyncLoadRequestMessage Request;
	Request.CorrelationId = FGuid::NewGuid();
	Request.Scope = Group->Scope;
	Request.TimingTag = CMStageLoadTags::Stage;
	Request.LoadPriority = Group->LoadPriority;
	Request.AssetBundles = Group->AssetBundles;

	// 빈 Schedule 요청은 동기적으로 완료될 수 있으므로 방송 전에 반드시 추적 상태를 먼저 기록한다.
	PendingRequests.Add(Request.CorrelationId, LoadGroupId);
	GroupStates.FindOrAdd(LoadGroupId) = ECMStageLoadGroupState::Loading;
	UE_LOG(LogChimeraStageLoad, Display,
		TEXT("Stage load group started. NetMode=%d Schedule=%s Group=%s Request=%s Scope=%d"),
		GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : INDEX_NONE,
		*GetNameSafe(ActiveSchedule), *LoadGroupId.ToString(),
		*Request.CorrelationId.ToString(), Request.Scope);
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		AsyncPDALoaderTags::Message_Load_Request,
		Request);
	return Request.CorrelationId;
}

// Ready 그룹은 즉시 해제하고 Loading 그룹은 완료 직후 해제 예약
void UCMStageLoadCoordinatorSubsystem::ReleaseLoadGroup(FName LoadGroupId)
{
	const ECMStageLoadGroupState State = GetLoadGroupState(LoadGroupId);
	QueuedLoadGroups.Remove(LoadGroupId);

	if (State == ECMStageLoadGroupState::Loading || State == ECMStageLoadGroupState::PendingRelease)
	{
		PendingReleaseGroups.Add(LoadGroupId);
		GroupStates.FindOrAdd(LoadGroupId) = ECMStageLoadGroupState::PendingRelease;
		return;
	}

	UnloadGroupAssets(LoadGroupId);
	GroupStates.FindOrAdd(LoadGroupId) = ECMStageLoadGroupState::Released;
}

// 선택되지 않은 비가역 분기를 큐에서 영구 제외하고 정책이 허용한 그룹만 해제
void UCMStageLoadCoordinatorSubsystem::RejectBranchLoadGroups(const TArray<FName>& RejectedLoadGroupIds)
{
	if (!ActiveSchedule)
	{
		return;
	}

	for (const FName LoadGroupId : RejectedLoadGroupIds)
	{
		const FCMStageLoadGroupDefinition* Group = ActiveSchedule->FindLoadGroup(LoadGroupId);
		if (Group && Group->RetentionPolicy == ECMLoadRetentionPolicy::ReleaseWhenBranchRejected)
		{
			SuppressedLoadGroups.Add(LoadGroupId);
			ReleaseLoadGroup(LoadGroupId);
		}
	}
}

// 로컬 그룹 상태 조회
ECMStageLoadGroupState UCMStageLoadCoordinatorSubsystem::GetLoadGroupState(FName LoadGroupId) const
{
	const ECMStageLoadGroupState* Found = GroupStates.Find(LoadGroupId);
	return Found ? *Found : ECMStageLoadGroupState::NotRequested;
}

// ActivationGate가 통과 여부를 판단할 수 있도록 Ready 상태 제공
bool UCMStageLoadCoordinatorSubsystem::IsLoadGroupReady(FName LoadGroupId) const
{
	return GetLoadGroupState(LoadGroupId) == ECMStageLoadGroupState::Ready;
}

// CorrelationId가 일치하는 로컬 요청만 처리하고 순차 큐를 다음 그룹으로 진행
void UCMStageLoadCoordinatorSubsystem::HandleLoadComplete(
	FGameplayTag Channel,
	const FAsyncLoadCompleteMessage& Message)
{
	if (bChangingSchedule)
	{
		return;
	}

	FName LoadGroupId;
	if (!PendingRequests.RemoveAndCopyValue(Message.CorrelationId, LoadGroupId))
	{
		return;
	}

	LoadedAssetsByGroup.Add(LoadGroupId, Message.LoadedAssetIds);
	const bool bSucceeded = Message.Result == EAsyncLoadResult::Succeeded;
	UE_LOG(LogChimeraStageLoad, Display,
		TEXT("Stage load group completed. NetMode=%d Schedule=%s Group=%s Request=%s Result=%d Loaded=%d Failed=%d"),
		GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : INDEX_NONE,
		*GetNameSafe(ActiveSchedule), *LoadGroupId.ToString(),
		*Message.CorrelationId.ToString(), static_cast<int32>(Message.Result),
		Message.LoadedAssetIds.Num(), Message.FailedAssetIds.Num());
	if (!bSucceeded)
	{
		UE_LOG(LogChimeraStageLoad, Error,
			TEXT("Stage load group failed. Schedule=%s Group=%s Request=%s"),
			*GetNameSafe(ActiveSchedule), *LoadGroupId.ToString(),
			*Message.CorrelationId.ToString());
	}
	GroupStates.FindOrAdd(LoadGroupId) = bSucceeded
		? ECMStageLoadGroupState::Ready
		: ECMStageLoadGroupState::Failed;

	const bool bReleaseImmediately = PendingReleaseGroups.Remove(LoadGroupId) > 0;
	if (bReleaseImmediately)
	{
		UnloadGroupAssets(LoadGroupId);
		GroupStates.FindOrAdd(LoadGroupId) = ECMStageLoadGroupState::Released;
	}

	OnLoadGroupFinished.Broadcast(LoadGroupId, Message.Result, bReleaseImmediately);

	if (ActiveQueuedGroup == LoadGroupId)
	{
		bActiveQueueFailed |= !bSucceeded;
		const bool bWasStartRequired = PendingStartRequiredGroups.Remove(LoadGroupId) > 0;
		if (bWasStartRequired && !bSucceeded && !bStartRequiredReported)
		{
			bStartRequiredReported = true;
			OnStageStartRequiredFinished.Broadcast(ActiveStageRequestId, false);
		}
		else if (!bStartRequiredReported && PendingStartRequiredGroups.IsEmpty())
		{
			bStartRequiredReported = true;
			OnStageStartRequiredFinished.Broadcast(ActiveStageRequestId, true);
		}
		ActiveQueuedGroup = NAME_None;
		RequestNextQueuedGroup();
	}
}

// 순차 큐가 빌 때까지 Ready·제외 그룹을 건너뛰며 한 번에 하나의 요청만 실행
void UCMStageLoadCoordinatorSubsystem::RequestNextQueuedGroup()
{
	while (!QueuedLoadGroups.IsEmpty())
	{
		const FName NextGroup = QueuedLoadGroups[0];
		QueuedLoadGroups.RemoveAt(0);
		if (SuppressedLoadGroups.Contains(NextGroup))
		{
			continue;
		}

		const ECMStageLoadGroupState State = GetLoadGroupState(NextGroup);
		if (State == ECMStageLoadGroupState::Ready)
		{
			continue;
		}

		ActiveQueuedGroup = NextGroup;
		if (State == ECMStageLoadGroupState::Loading || State == ECMStageLoadGroupState::PendingRelease)
		{
			return;
		}

		if (RequestLoadGroup(NextGroup).IsValid())
		{
			return;
		}

		bActiveQueueFailed = true;
		ActiveQueuedGroup = NAME_None;
	}

	const bool bSucceeded = !bActiveQueueFailed;
	bActiveQueueFailed = false;
	OnLoadQueueFinished.Broadcast(CMStageLoadTags::Stage, bSucceeded);

	if (ActiveStageRequestId.IsValid())
	{
		const FGuid CompletedRequestId = ActiveStageRequestId;
		if (!bStartRequiredReported)
		{
			bStartRequiredReported = true;
			OnStageStartRequiredFinished.Broadcast(CompletedRequestId, bSucceeded);
		}
		ActiveStageRequestId.Invalidate();
		PendingScheduleId = FPrimaryAssetId();
		ScheduleLoadHandle.Reset();
		OnStageScheduleFinished.Broadcast(CompletedRequestId, bSucceeded);
	}
}

// 로드된 Primary Asset이 유효한 Schedule인지 확인한 뒤 기존 Coordinator 큐에 연결
void UCMStageLoadCoordinatorSubsystem::HandleScheduleLoaded()
{
	UCMStageLoadSchedule* Schedule = Cast<UCMStageLoadSchedule>(
		UAssetManager::Get().GetPrimaryAssetObject(PendingScheduleId));
	// 취소 과정에서 Loader Provider가 비워질 수 있으므로 같은 Schedule이어도 다시 활성화한다.
	const bool bScheduleReady = Schedule && ActivateStageSchedule(Schedule);
	if (bScheduleReady && StartAutomaticLoadQueue())
	{
		return;
	}

	const FGuid FailedRequestId = ActiveStageRequestId;
	ActiveStageRequestId.Invalidate();
	PendingScheduleId = FPrimaryAssetId();
	ScheduleLoadHandle.Reset();
	OnStageStartRequiredFinished.Broadcast(FailedRequestId, false);
	OnStageScheduleFinished.Broadcast(FailedRequestId, false);
}

// 스테이지 전환 시 Session 에셋을 제외한 현재 Stage 그룹 캐시 해제
void UCMStageLoadCoordinatorSubsystem::ReleaseCurrentStageAssets()
{
	UAsyncPDALoader* Loader = GetAsyncLoader();
	if (!Loader)
	{
		return;
	}

	TArray<FPrimaryAssetId> AssetsToUnload;
	for (const TPair<FName, TArray<FPrimaryAssetId>>& Pair : LoadedAssetsByGroup)
	{
		for (const FPrimaryAssetId& AssetId : Pair.Value)
		{
			AssetsToUnload.AddUnique(AssetId);
		}
	}
	Loader->UnloadAssets(AssetsToUnload);
}

// 그룹의 완료된 Primary Asset 캐시 해제
void UCMStageLoadCoordinatorSubsystem::UnloadGroupAssets(FName LoadGroupId)
{
	TArray<FPrimaryAssetId> Assets;
	if (!LoadedAssetsByGroup.RemoveAndCopyValue(LoadGroupId, Assets))
	{
		return;
	}

	if (UAsyncPDALoader* Loader = GetAsyncLoader())
	{
		Loader->UnloadAssets(Assets);
	}
}

// NKM에서 이식한 범용 로더 GameInstanceSubsystem 조회
UAsyncPDALoader* UCMStageLoadCoordinatorSubsystem::GetAsyncLoader() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UAsyncPDALoader>() : nullptr;
}
