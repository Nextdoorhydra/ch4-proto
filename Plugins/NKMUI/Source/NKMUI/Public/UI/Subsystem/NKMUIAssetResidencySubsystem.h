#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "NKMUIAssetResidencySubsystem.generated.h"

struct FStreamableHandle;
class UNKMUIAssetLease;
class UNKMUIAssetResidencySubsystem;

UENUM(BlueprintType)
enum class ENKMUIAssetLoadResult : uint8
{
	Succeeded,
	InvalidRequest,
	LoadFailed,
	Cancelled
};

UENUM(BlueprintType)
enum class ENKMUIAssetLeaseState : uint8
{
	Loading,
	Ready,
	Failed,
	Released
};

DECLARE_DELEGATE_TwoParams(
	FNKMUIAssetLeaseCompletedNative,
	ENKMUIAssetLoadResult,
	UNKMUIAssetLease*);

DECLARE_DYNAMIC_DELEGATE_TwoParams(
	FNKMUIAssetLeaseCompleted,
	ENKMUIAssetLoadResult, Result,
	UNKMUIAssetLease*, Lease);

/**
 * Keeps one asynchronous asset request resident until Release is called.
 * Multiple leases and project-specific loaders can safely own the same asset;
 * Unreal's StreamableManager keeps it resident until every owner releases it.
 * The caller must keep the lease in a UPROPERTY (or another strong UObject reference)
 * for as long as residency is required.
 */
UCLASS(BlueprintType)
class NKMUI_API UNKMUIAssetLease : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NKM|UI|Assets")
	void Release();

	UFUNCTION(BlueprintPure, Category = "NKM|UI|Assets")
	ENKMUIAssetLeaseState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "NKM|UI|Assets")
	bool IsReady() const { return State == ENKMUIAssetLeaseState::Ready; }

	UFUNCTION(BlueprintPure, Category = "NKM|UI|Assets")
	void GetLoadedAssets(TArray<UObject*>& OutAssets) const;

	virtual void BeginDestroy() override;

private:
	friend class UNKMUIAssetResidencySubsystem;

	void Initialize(
		UNKMUIAssetResidencySubsystem* InOwner,
		const TArray<TSoftObjectPtr<UObject>>& InAssets,
		FNKMUIAssetLeaseCompletedNative InNativeCompletion,
		FNKMUIAssetLeaseCompleted InDynamicCompletion);
	void StartLoading();
	void HandleLoadCompleted();
	void Complete(ENKMUIAssetLoadResult Result);
	void ReleaseInternal(bool bNotifyCancellation);

	TWeakObjectPtr<UNKMUIAssetResidencySubsystem> Owner;
	TArray<TSoftObjectPtr<UObject>> RequestedAssets;
	TSharedPtr<FStreamableHandle> Handle;
	FNKMUIAssetLeaseCompletedNative NativeCompletion;

	UPROPERTY(Transient)
	FNKMUIAssetLeaseCompleted DynamicCompletion;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> LoadedAssets;

	UPROPERTY(Transient)
	ENKMUIAssetLeaseState State = ENKMUIAssetLeaseState::Released;
};

/**
 * Creates explicit residency leases for feature prewarm and custom on-demand flows.
 * It deliberately delegates loading and cross-owner reference tracking to AssetManager.
 */
UCLASS()
class NKMUI_API UNKMUIAssetResidencySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UNKMUIAssetLease* AcquireAssetsAsync(
		const TArray<TSoftObjectPtr<UObject>>& Assets,
		FNKMUIAssetLeaseCompletedNative OnComplete = FNKMUIAssetLeaseCompletedNative());

	UFUNCTION(BlueprintCallable, Category = "NKM|UI|Assets", meta = (DisplayName = "Acquire UI Assets Async"))
	UNKMUIAssetLease* K2_AcquireAssetsAsync(
		const TArray<TSoftObjectPtr<UObject>>& Assets,
		FNKMUIAssetLeaseCompleted OnComplete);

private:
	friend class UNKMUIAssetLease;

	UNKMUIAssetLease* CreateLease(
		const TArray<TSoftObjectPtr<UObject>>& Assets,
		FNKMUIAssetLeaseCompletedNative NativeCompletion,
		FNKMUIAssetLeaseCompleted DynamicCompletion);
	void NotifyLeaseLoadFinished(UNKMUIAssetLease* Lease);
	void NotifyLeaseReleased(UNKMUIAssetLease* Lease);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNKMUIAssetLease>> PendingLeases;

	TArray<TWeakObjectPtr<UNKMUIAssetLease>> ActiveLeases;
};
