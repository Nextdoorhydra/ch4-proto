#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UIExtensionSystem.h"
#include "UObject/SoftObjectPtr.h"
#include "NKMUIManagerSubsystem.generated.h"

class ULocalPlayer;
class UNKMUIActivatableWidget;
class UNKMUIExtensionData;
class UNKMUIPolicy;

UENUM(BlueprintType)
enum class ENKMUIAsyncResult : uint8
{
	Succeeded,
	InvalidRequest,
	NotReady,
	LoadFailed,
	Cancelled,
	Stale
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(
	FNKMUIWidgetPushCompleted,
	ENKMUIAsyncResult, Result,
	UNKMUIActivatableWidget*, Widget);

DECLARE_DELEGATE_OneParam(
	FNKMUIExtensionRegistrationCompleted,
	ENKMUIAsyncResult);

DECLARE_DELEGATE_OneParam(
	FNKMUIPolicyInitializationCompleted,
	ENKMUIAsyncResult);

DECLARE_MULTICAST_DELEGATE_OneParam(FNKMUIOnGameplayHUDVisibilityChanged, bool);

UCLASS()
class NKMUI_API UNKMUIManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "NKM|UI")
	void InitializePolicy(ULocalPlayer* LocalPlayer);
	void InitializePolicyWithResult(
		ULocalPlayer* LocalPlayer,
		FNKMUIPolicyInitializationCompleted OnComplete);

	UFUNCTION(BlueprintCallable, Category = "NKM|UI")
	UNKMUIActivatableWidget* PushWidget(
		FGameplayTag LayerTag,
		TSubclassOf<UNKMUIActivatableWidget> WidgetClass);

	UFUNCTION(BlueprintCallable, Category = "NKM|UI")
	void RemoveWidget(FGameplayTag LayerTag, UNKMUIActivatableWidget* Widget);

	UFUNCTION(BlueprintCallable, Category = "NKM|UI")
	void PushWidgetAsync(
		FGameplayTag LayerTag,
		TSoftClassPtr<UNKMUIActivatableWidget> WidgetClass);

	UFUNCTION(BlueprintCallable, Category = "NKM|UI")
	int32 PushWidgetAsyncWithResult(
		FGameplayTag LayerTag,
		TSoftClassPtr<UNKMUIActivatableWidget> WidgetClass,
		FNKMUIWidgetPushCompleted OnComplete);

	UFUNCTION(BlueprintCallable, Category = "NKM|UI")
	void ClearLayer(FGameplayTag LayerTag);

	UFUNCTION(BlueprintCallable, Category = "NKM|UI")
	void ResetPolicy();

	void RegisterExtensionsFromData(
		TSoftObjectPtr<UNKMUIExtensionData> ExtensionData,
		FNKMUIExtensionRegistrationCompleted OnComplete =
			FNKMUIExtensionRegistrationCompleted());

	UFUNCTION(BlueprintCallable, Category = "NKM|UI")
	ENKMUIAsyncResult UnregisterExtensionsFromData(
		TSoftObjectPtr<UNKMUIExtensionData> ExtensionData,
		bool bReleaseLoadedAssets = true);

	UFUNCTION(BlueprintCallable, Category = "NKM|UI")
	void InjectUIExtensionsDynamic(TSoftObjectPtr<UNKMUIExtensionData> ExtensionData);

	void SetGameplayHUDVisible(bool bVisible);
	bool IsGameplayHUDVisible() const { return bGameplayHUDVisible; }
	FNKMUIOnGameplayHUDVisibilityChanged& OnGameplayHUDVisibilityChanged()
	{
		return GameplayHUDVisibilityChanged;
	}

private:
	struct FPendingWidgetRequest
	{
		uint64 Generation = 0;
		FGameplayTag LayerTag;
		TSoftClassPtr<UNKMUIActivatableWidget> WidgetClass;
		FNKMUIWidgetPushCompleted Completion;
		TSharedPtr<FStreamableHandle> Handle;
	};

	struct FPendingExtensionLoad
	{
		uint64 RequestId = 0;
		TSharedPtr<FStreamableHandle> DataHandle;
		TSharedPtr<FStreamableHandle> WidgetClassHandle;
	};

	TSoftClassPtr<UNKMUIPolicy> GetConfiguredPolicyClass() const;
	void GetConfiguredExtensionDataAssets(
		TArray<TSoftObjectPtr<UNKMUIExtensionData>>& OutAssets) const;

	void HandlePolicyClassLoaded(
		uint64 RequestGeneration,
		TWeakObjectPtr<ULocalPlayer> LocalPlayer,
		TSoftClassPtr<UNKMUIPolicy> PolicyClass);
	void HandleLayoutClassLoaded(uint64 RequestGeneration, TWeakObjectPtr<ULocalPlayer> LocalPlayer);
	void CompletePolicyInitialization(uint64 RequestGeneration, ULocalPlayer* LocalPlayer);
	void CompletePolicyInitializationRequests(ENKMUIAsyncResult Result);
	void RegisterCachedExtensions();
	ENKMUIAsyncResult RegisterExtensionData(UNKMUIExtensionData* ExtensionData);
	void HandleExtensionWidgetClassesLoaded(
		const FSoftObjectPath& DataPath,
		uint64 RequestGeneration,
		uint64 ExtensionRequestId);
	void CompleteExtensionRequest(
		const FSoftObjectPath& DataPath,
		uint64 ExtensionRequestId,
		ENKMUIAsyncResult Result);
	bool IsExtensionRequestCurrent(
		const FSoftObjectPath& DataPath,
		uint64 ExtensionRequestId) const;
	void UnregisterExtensionHandles(const FSoftObjectPath& DataPath);
	void ReleaseRetainedWidgetClasses(const FSoftObjectPath& DataPath);
	void HandleWidgetClassLoaded(int32 RequestId);
	void TrackLifecycleHandle(const TSharedPtr<FStreamableHandle>& Handle);
	void PruneCompletedLifecycleHandles();
	void CancelOutstandingRequests(ENKMUIAsyncResult Result);

	UPROPERTY()
	TObjectPtr<UNKMUIPolicy> CurrentPolicy;

	UPROPERTY()
	TArray<TObjectPtr<UNKMUIExtensionData>> CachedExtensionData;

	UPROPERTY()
	TMap<FSoftObjectPath, TObjectPtr<UNKMUIExtensionData>> PendingExtensionData;

	TArray<TSharedPtr<FStreamableHandle>> LifecycleLoadHandles;
	TMap<FSoftObjectPath, FPendingExtensionLoad> PendingExtensionLoads;
	TMap<FSoftObjectPath, TSharedPtr<FStreamableHandle>> RetainedWidgetClassHandles;
	TMap<FSoftObjectPath, TArray<FUIExtensionHandle>> RegisteredExtensionHandles;
	TMap<int32, FPendingWidgetRequest> PendingWidgetRequests;
	TMap<FSoftObjectPath, TArray<FNKMUIExtensionRegistrationCompleted>> PendingExtensionRequests;
	TArray<FNKMUIPolicyInitializationCompleted> PendingPolicyInitializationRequests;
	TMap<FSoftObjectPath, uint64> ActiveExtensionRequestIds;
	TSet<FSoftObjectPath> RegisteredExtensionDataPaths;

	FNKMUIOnGameplayHUDVisibilityChanged GameplayHUDVisibilityChanged;
	uint64 LifecycleGeneration = 1;
	uint64 NextExtensionRequestId = 1;
	int32 NextRequestId = 1;
	bool bPolicyInitializationInFlight = false;
	bool bGameplayHUDVisible = true;
};
