#include "UI/Subsystem/NKMUIAssetResidencySubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

void UNKMUIAssetLease::Initialize(
	UNKMUIAssetResidencySubsystem* InOwner,
	const TArray<TSoftObjectPtr<UObject>>& InAssets,
	FNKMUIAssetLeaseCompletedNative InNativeCompletion,
	FNKMUIAssetLeaseCompleted InDynamicCompletion)
{
	Owner = InOwner;
	NativeCompletion = MoveTemp(InNativeCompletion);
	DynamicCompletion = MoveTemp(InDynamicCompletion);
	State = ENKMUIAssetLeaseState::Loading;

	TSet<FSoftObjectPath> UniquePaths;
	for (const TSoftObjectPtr<UObject>& Asset : InAssets)
	{
		const FSoftObjectPath AssetPath = Asset.ToSoftObjectPath();
		if (!Asset.IsNull() && !UniquePaths.Contains(AssetPath))
		{
			UniquePaths.Add(AssetPath);
			RequestedAssets.Add(Asset);
		}
	}
}

void UNKMUIAssetLease::StartLoading()
{
	if (RequestedAssets.IsEmpty())
	{
		State = ENKMUIAssetLeaseState::Failed;
		Complete(ENKMUIAssetLoadResult::InvalidRequest);
		return;
	}

	TArray<FSoftObjectPath> Paths;
	Paths.Reserve(RequestedAssets.Num());
	for (const TSoftObjectPtr<UObject>& Asset : RequestedAssets)
	{
		Paths.Add(Asset.ToSoftObjectPath());
	}

	Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths,
		FStreamableDelegate::CreateWeakLambda(this, [this]()
		{
			HandleLoadCompleted();
		}),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		true,
		TEXT("NKMUIAssetLease"));

	if (!Handle.IsValid())
	{
		State = ENKMUIAssetLeaseState::Failed;
		Complete(ENKMUIAssetLoadResult::LoadFailed);
		return;
	}

	Handle->StartStalledHandle();
}

void UNKMUIAssetLease::HandleLoadCompleted()
{
	if (State != ENKMUIAssetLeaseState::Loading)
	{
		return;
	}

	LoadedAssets.Reserve(RequestedAssets.Num());
	for (const TSoftObjectPtr<UObject>& Asset : RequestedAssets)
	{
		UObject* LoadedAsset = Asset.Get();
		if (!LoadedAsset)
		{
			LoadedAssets.Reset();
			State = ENKMUIAssetLeaseState::Failed;
			if (Handle.IsValid())
			{
				Handle->ReleaseHandle();
				Handle.Reset();
			}
			Complete(ENKMUIAssetLoadResult::LoadFailed);
			return;
		}

		LoadedAssets.Add(LoadedAsset);
	}

	State = ENKMUIAssetLeaseState::Ready;
	Complete(ENKMUIAssetLoadResult::Succeeded);
}

void UNKMUIAssetLease::Complete(ENKMUIAssetLoadResult Result)
{
	if (UNKMUIAssetResidencySubsystem* OwnerSubsystem = Owner.Get())
	{
		OwnerSubsystem->NotifyLeaseLoadFinished(this);
	}

	FNKMUIAssetLeaseCompletedNative LocalNativeCompletion = MoveTemp(NativeCompletion);
	FNKMUIAssetLeaseCompleted LocalDynamicCompletion = MoveTemp(DynamicCompletion);
	NativeCompletion.Unbind();
	DynamicCompletion.Unbind();

	LocalNativeCompletion.ExecuteIfBound(Result, this);
	LocalDynamicCompletion.ExecuteIfBound(Result, this);
}

void UNKMUIAssetLease::Release()
{
	ReleaseInternal(true);
}

void UNKMUIAssetLease::ReleaseInternal(bool bNotifyCancellation)
{
	if (State == ENKMUIAssetLeaseState::Released)
	{
		return;
	}

	const bool bWasLoading = State == ENKMUIAssetLeaseState::Loading;
	State = ENKMUIAssetLeaseState::Released;
	LoadedAssets.Reset();
	RequestedAssets.Reset();

	if (Handle.IsValid())
	{
		if (bWasLoading)
		{
			Handle->CancelHandle();
		}
		else
		{
			Handle->ReleaseHandle();
		}
		Handle.Reset();
	}

	if (UNKMUIAssetResidencySubsystem* OwnerSubsystem = Owner.Get())
	{
		OwnerSubsystem->NotifyLeaseReleased(this);
	}
	Owner.Reset();

	if (bWasLoading && bNotifyCancellation)
	{
		FNKMUIAssetLeaseCompletedNative LocalNativeCompletion = MoveTemp(NativeCompletion);
		FNKMUIAssetLeaseCompleted LocalDynamicCompletion = MoveTemp(DynamicCompletion);
		NativeCompletion.Unbind();
		DynamicCompletion.Unbind();
		LocalNativeCompletion.ExecuteIfBound(ENKMUIAssetLoadResult::Cancelled, this);
		LocalDynamicCompletion.ExecuteIfBound(ENKMUIAssetLoadResult::Cancelled, this);
	}
	else
	{
		NativeCompletion.Unbind();
		DynamicCompletion.Unbind();
	}
}

void UNKMUIAssetLease::GetLoadedAssets(TArray<UObject*>& OutAssets) const
{
	OutAssets.Reset(LoadedAssets.Num());
	for (UObject* Asset : LoadedAssets)
	{
		OutAssets.Add(Asset);
	}
}

void UNKMUIAssetLease::BeginDestroy()
{
	ReleaseInternal(false);
	Super::BeginDestroy();
}

void UNKMUIAssetResidencySubsystem::Deinitialize()
{
	const TArray<TWeakObjectPtr<UNKMUIAssetLease>> LeasesToRelease = ActiveLeases;
	for (const TWeakObjectPtr<UNKMUIAssetLease>& WeakLease : LeasesToRelease)
	{
		if (UNKMUIAssetLease* Lease = WeakLease.Get())
		{
			Lease->ReleaseInternal(false);
		}
	}

	PendingLeases.Reset();
	ActiveLeases.Reset();
	Super::Deinitialize();
}

UNKMUIAssetLease* UNKMUIAssetResidencySubsystem::AcquireAssetsAsync(
	const TArray<TSoftObjectPtr<UObject>>& Assets,
	FNKMUIAssetLeaseCompletedNative OnComplete)
{
	return CreateLease(
		Assets,
		MoveTemp(OnComplete),
		FNKMUIAssetLeaseCompleted());
}

UNKMUIAssetLease* UNKMUIAssetResidencySubsystem::K2_AcquireAssetsAsync(
	const TArray<TSoftObjectPtr<UObject>>& Assets,
	FNKMUIAssetLeaseCompleted OnComplete)
{
	return CreateLease(
		Assets,
		FNKMUIAssetLeaseCompletedNative(),
		MoveTemp(OnComplete));
}

UNKMUIAssetLease* UNKMUIAssetResidencySubsystem::CreateLease(
	const TArray<TSoftObjectPtr<UObject>>& Assets,
	FNKMUIAssetLeaseCompletedNative NativeCompletion,
	FNKMUIAssetLeaseCompleted DynamicCompletion)
{
	UNKMUIAssetLease* Lease = NewObject<UNKMUIAssetLease>(this);
	PendingLeases.Add(Lease);
	ActiveLeases.Add(Lease);
	Lease->Initialize(this, Assets, MoveTemp(NativeCompletion), MoveTemp(DynamicCompletion));
	Lease->StartLoading();
	return Lease;
}

void UNKMUIAssetResidencySubsystem::NotifyLeaseLoadFinished(UNKMUIAssetLease* Lease)
{
	PendingLeases.RemoveSingleSwap(Lease);
}

void UNKMUIAssetResidencySubsystem::NotifyLeaseReleased(UNKMUIAssetLease* Lease)
{
	PendingLeases.RemoveSingleSwap(Lease);
	ActiveLeases.RemoveAllSwap([Lease](const TWeakObjectPtr<UNKMUIAssetLease>& WeakLease)
	{
		return !WeakLease.IsValid() || WeakLease.Get() == Lease;
	});
}
