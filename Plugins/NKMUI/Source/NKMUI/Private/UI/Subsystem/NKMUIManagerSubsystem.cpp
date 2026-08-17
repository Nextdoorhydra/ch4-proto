#include "UI/Subsystem/NKMUIManagerSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "UI/NKMUIActivatableWidget.h"
#include "UI/NKMUIExtensionData.h"
#include "UI/NKMUIPolicy.h"
#include "UI/NKMUIRootLayout.h"
#include "UI/NKMUIManagerSettings.h"
#include "UObject/UObjectHash.h"

bool UNKMUIManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> DerivedClasses;
	GetDerivedClasses(GetClass(), DerivedClasses, true);
	for (const UClass* DerivedClass : DerivedClasses)
	{
		if (DerivedClass && !DerivedClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return false;
		}
	}
	return true;
}

void UNKMUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TArray<TSoftObjectPtr<UNKMUIExtensionData>> ExtensionAssets;
	GetConfiguredExtensionDataAssets(ExtensionAssets);
	for (const TSoftObjectPtr<UNKMUIExtensionData>& ExtensionAsset : ExtensionAssets)
	{
		if (!ExtensionAsset.IsNull())
		{
			RegisterExtensionsFromData(ExtensionAsset);
		}
	}
}

void UNKMUIManagerSubsystem::Deinitialize()
{
	ResetPolicy();
	for (TPair<FSoftObjectPath, TSharedPtr<FStreamableHandle>>& Pair : RetainedWidgetClassHandles)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->ReleaseHandle();
		}
	}
	RetainedWidgetClassHandles.Reset();
	CachedExtensionData.Reset();
	PendingExtensionData.Reset();
	PendingExtensionLoads.Reset();
	PendingExtensionRequests.Reset();
	ActiveExtensionRequestIds.Reset();
	RegisteredExtensionDataPaths.Reset();
	Super::Deinitialize();
}

void UNKMUIManagerSubsystem::InitializePolicy(ULocalPlayer* LocalPlayer)
{
	InitializePolicyWithResult(LocalPlayer, FNKMUIPolicyInitializationCompleted());
}

void UNKMUIManagerSubsystem::InitializePolicyWithResult(
	ULocalPlayer* LocalPlayer,
	FNKMUIPolicyInitializationCompleted OnComplete)
{
	UE_LOG(LogTemp, Display, TEXT("[NKMStartup][UI] InitializePolicy. LocalPlayer=%s CurrentPolicy=%s InFlight=%s"),
		*GetNameSafe(LocalPlayer), *GetNameSafe(CurrentPolicy),
		bPolicyInitializationInFlight ? TEXT("true") : TEXT("false"));
	if (CurrentPolicy)
	{
		OnComplete.ExecuteIfBound(ENKMUIAsyncResult::Succeeded);
		return;
	}
	if (!IsValid(LocalPlayer))
	{
		OnComplete.ExecuteIfBound(ENKMUIAsyncResult::InvalidRequest);
		return;
	}

	PendingPolicyInitializationRequests.Add(MoveTemp(OnComplete));
	if (bPolicyInitializationInFlight)
	{
		return;
	}

	const TSoftClassPtr<UNKMUIPolicy> PolicyClass = GetConfiguredPolicyClass();
	if (PolicyClass.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[NKMStartup][UI] DefaultPolicyClass is not configured."));
		CompletePolicyInitializationRequests(ENKMUIAsyncResult::InvalidRequest);
		return;
	}

	bPolicyInitializationInFlight = true;
	const uint64 RequestGeneration = LifecycleGeneration;
	const TWeakObjectPtr<ULocalPlayer> WeakLocalPlayer(LocalPlayer);

	if (PolicyClass.Get())
	{
		HandlePolicyClassLoaded(RequestGeneration, WeakLocalPlayer, PolicyClass);
		return;
	}

	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		PolicyClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, RequestGeneration, WeakLocalPlayer, PolicyClass]()
		{
			HandlePolicyClassLoaded(RequestGeneration, WeakLocalPlayer, PolicyClass);
		}));
	TrackLifecycleHandle(Handle);

	if (!Handle.IsValid())
	{
		bPolicyInitializationInFlight = false;
		UE_LOG(LogTemp, Error, TEXT("[NKMStartup][UI] Policy class async load request failed. Path=%s"), *PolicyClass.ToString());
		CompletePolicyInitializationRequests(ENKMUIAsyncResult::LoadFailed);
	}
}

void UNKMUIManagerSubsystem::HandlePolicyClassLoaded(
	uint64 RequestGeneration,
	TWeakObjectPtr<ULocalPlayer> LocalPlayer,
	TSoftClassPtr<UNKMUIPolicy> PolicyClass)
{
	PruneCompletedLifecycleHandles();
	if (RequestGeneration != LifecycleGeneration || !LocalPlayer.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[NKMStartup][UI] Policy class callback became stale. Generation=%llu Current=%llu LocalPlayerValid=%s"),
			RequestGeneration, LifecycleGeneration, LocalPlayer.IsValid() ? TEXT("true") : TEXT("false"));
		if (RequestGeneration == LifecycleGeneration)
		{
			bPolicyInitializationInFlight = false;
			CompletePolicyInitializationRequests(ENKMUIAsyncResult::InvalidRequest);
		}
		return;
	}

	const TSubclassOf<UNKMUIPolicy> LoadedPolicyClass = PolicyClass.Get();
	if (!LoadedPolicyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[NKMStartup][UI] Policy class failed to load. Path=%s"), *PolicyClass.ToString());
		bPolicyInitializationInFlight = false;
		CompletePolicyInitializationRequests(ENKMUIAsyncResult::LoadFailed);
		return;
	}

	CurrentPolicy = NewObject<UNKMUIPolicy>(this, LoadedPolicyClass);
	if (!CurrentPolicy)
	{
		UE_LOG(LogTemp, Error, TEXT("[NKMStartup][UI] Failed to create policy object."));
		bPolicyInitializationInFlight = false;
		CompletePolicyInitializationRequests(ENKMUIAsyncResult::LoadFailed);
		return;
	}

	const TSoftClassPtr<UNKMUIRootLayout> LayoutClass = CurrentPolicy->GetLayoutClass();
	if (LayoutClass.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[NKMStartup][UI] Policy has no RootLayout class. Policy=%s"), *GetNameSafe(CurrentPolicy));
		CurrentPolicy = nullptr;
		bPolicyInitializationInFlight = false;
		CompletePolicyInitializationRequests(ENKMUIAsyncResult::InvalidRequest);
		return;
	}

	if (LayoutClass.Get())
	{
		CompletePolicyInitialization(RequestGeneration, LocalPlayer.Get());
		return;
	}

	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		LayoutClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, RequestGeneration, LocalPlayer]()
		{
			HandleLayoutClassLoaded(RequestGeneration, LocalPlayer);
		}));
	TrackLifecycleHandle(Handle);

	if (!Handle.IsValid())
	{
		CurrentPolicy = nullptr;
		bPolicyInitializationInFlight = false;
		CompletePolicyInitializationRequests(ENKMUIAsyncResult::LoadFailed);
	}
}

void UNKMUIManagerSubsystem::HandleLayoutClassLoaded(
	uint64 RequestGeneration,
	TWeakObjectPtr<ULocalPlayer> LocalPlayer)
{
	PruneCompletedLifecycleHandles();
	if (RequestGeneration != LifecycleGeneration || !LocalPlayer.IsValid())
	{
		if (RequestGeneration == LifecycleGeneration)
		{
			CurrentPolicy = nullptr;
			bPolicyInitializationInFlight = false;
			CompletePolicyInitializationRequests(ENKMUIAsyncResult::InvalidRequest);
		}
		return;
	}

	CompletePolicyInitialization(RequestGeneration, LocalPlayer.Get());
}

void UNKMUIManagerSubsystem::CompletePolicyInitialization(
	uint64 RequestGeneration,
	ULocalPlayer* LocalPlayer)
{
	if (RequestGeneration != LifecycleGeneration || !CurrentPolicy || !CurrentPolicy->CreateLayout(LocalPlayer))
	{
		UE_LOG(LogTemp, Error, TEXT("[NKMStartup][UI] RootLayout creation failed. Generation=%llu Current=%llu Policy=%s LocalPlayer=%s"),
			RequestGeneration, LifecycleGeneration, *GetNameSafe(CurrentPolicy), *GetNameSafe(LocalPlayer));
		CurrentPolicy = nullptr;
		bPolicyInitializationInFlight = false;
		CompletePolicyInitializationRequests(ENKMUIAsyncResult::LoadFailed);
		return;
	}

	bPolicyInitializationInFlight = false;
	UE_LOG(LogTemp, Display, TEXT("[NKMStartup][UI] Policy and RootLayout initialized. Policy=%s CachedExtensions=%d"),
		*GetNameSafe(CurrentPolicy), CachedExtensionData.Num());
	RegisterCachedExtensions();
	CompletePolicyInitializationRequests(ENKMUIAsyncResult::Succeeded);
}

void UNKMUIManagerSubsystem::CompletePolicyInitializationRequests(ENKMUIAsyncResult Result)
{
	TArray<FNKMUIPolicyInitializationCompleted> Waiters = MoveTemp(PendingPolicyInitializationRequests);
	PendingPolicyInitializationRequests.Reset();
	for (FNKMUIPolicyInitializationCompleted& Waiter : Waiters)
	{
		Waiter.ExecuteIfBound(Result);
	}
}

UNKMUIActivatableWidget* UNKMUIManagerSubsystem::PushWidget(
	FGameplayTag LayerTag,
	TSubclassOf<UNKMUIActivatableWidget> WidgetClass)
{
	return CurrentPolicy && WidgetClass
		? CurrentPolicy->PushWidgetToLayer(LayerTag, WidgetClass)
		: nullptr;
}

void UNKMUIManagerSubsystem::RemoveWidget(
	FGameplayTag LayerTag,
	UNKMUIActivatableWidget* Widget)
{
	if (CurrentPolicy)
	{
		CurrentPolicy->RemoveWidgetFromLayer(LayerTag, Widget);
	}
}

void UNKMUIManagerSubsystem::PushWidgetAsync(
	FGameplayTag LayerTag,
	TSoftClassPtr<UNKMUIActivatableWidget> WidgetClass)
{
	PushWidgetAsyncWithResult(LayerTag, WidgetClass, FNKMUIWidgetPushCompleted());
}

int32 UNKMUIManagerSubsystem::PushWidgetAsyncWithResult(
	FGameplayTag LayerTag,
	TSoftClassPtr<UNKMUIActivatableWidget> WidgetClass,
	FNKMUIWidgetPushCompleted OnComplete)
{
	if (WidgetClass.IsNull())
	{
		OnComplete.ExecuteIfBound(ENKMUIAsyncResult::InvalidRequest, nullptr);
		return INDEX_NONE;
	}

	if (!CurrentPolicy)
	{
		OnComplete.ExecuteIfBound(ENKMUIAsyncResult::NotReady, nullptr);
		return INDEX_NONE;
	}

	if (const TSubclassOf<UNKMUIActivatableWidget> LoadedClass = WidgetClass.Get())
	{
		UNKMUIActivatableWidget* Widget = PushWidget(LayerTag, LoadedClass);
		OnComplete.ExecuteIfBound(
			Widget ? ENKMUIAsyncResult::Succeeded : ENKMUIAsyncResult::NotReady,
			Widget);
		return INDEX_NONE;
	}

	const int32 RequestId = NextRequestId++;
	FPendingWidgetRequest& Request = PendingWidgetRequests.Add(RequestId);
	Request.Generation = LifecycleGeneration;
	Request.LayerTag = LayerTag;
	Request.WidgetClass = WidgetClass;
	Request.Completion = OnComplete;
	Request.Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		WidgetClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, RequestId]()
		{
			HandleWidgetClassLoaded(RequestId);
		}));

	if (!Request.Handle.IsValid())
	{
		Request.Completion.ExecuteIfBound(ENKMUIAsyncResult::LoadFailed, nullptr);
		PendingWidgetRequests.Remove(RequestId);
		return INDEX_NONE;
	}

	return RequestId;
}

void UNKMUIManagerSubsystem::HandleWidgetClassLoaded(int32 RequestId)
{
	FPendingWidgetRequest Request;
	if (!PendingWidgetRequests.RemoveAndCopyValue(RequestId, Request))
	{
		return;
	}

	if (Request.Generation != LifecycleGeneration)
	{
		Request.Completion.ExecuteIfBound(ENKMUIAsyncResult::Stale, nullptr);
		return;
	}

	const TSubclassOf<UNKMUIActivatableWidget> LoadedClass = Request.WidgetClass.Get();
	if (!LoadedClass)
	{
		Request.Completion.ExecuteIfBound(ENKMUIAsyncResult::LoadFailed, nullptr);
		return;
	}

	UNKMUIActivatableWidget* Widget = PushWidget(Request.LayerTag, LoadedClass);
	Request.Completion.ExecuteIfBound(
		Widget ? ENKMUIAsyncResult::Succeeded : ENKMUIAsyncResult::NotReady,
		Widget);
}

void UNKMUIManagerSubsystem::ClearLayer(FGameplayTag LayerTag)
{
	if (CurrentPolicy)
	{
		CurrentPolicy->ClearLayer(LayerTag);
	}
}

void UNKMUIManagerSubsystem::ResetPolicy()
{
	++LifecycleGeneration;
	CancelOutstandingRequests(ENKMUIAsyncResult::Cancelled);
	CompletePolicyInitializationRequests(ENKMUIAsyncResult::Cancelled);
	bPolicyInitializationInFlight = false;

	if (CurrentPolicy)
	{
		CurrentPolicy->DestroyLayout();
		CurrentPolicy = nullptr;
	}

	for (TPair<FSoftObjectPath, TArray<FUIExtensionHandle>>& Pair : RegisteredExtensionHandles)
	{
		for (FUIExtensionHandle& Handle : Pair.Value)
		{
			Handle.Unregister();
		}
	}
	RegisteredExtensionHandles.Reset();
	RegisteredExtensionDataPaths.Reset();
}

void UNKMUIManagerSubsystem::RegisterExtensionsFromData(
	TSoftObjectPtr<UNKMUIExtensionData> ExtensionData,
	FNKMUIExtensionRegistrationCompleted OnComplete)
{
	if (ExtensionData.IsNull())
	{
		OnComplete.ExecuteIfBound(ENKMUIAsyncResult::InvalidRequest);
		return;
	}

	const FSoftObjectPath DataPath = ExtensionData.ToSoftObjectPath();
	if (RegisteredExtensionDataPaths.Contains(DataPath))
	{
		OnComplete.ExecuteIfBound(ENKMUIAsyncResult::Succeeded);
		return;
	}

	if (TArray<FNKMUIExtensionRegistrationCompleted>* ExistingWaiters =
		PendingExtensionRequests.Find(DataPath))
	{
		ExistingWaiters->Add(MoveTemp(OnComplete));
		return;
	}

	TArray<FNKMUIExtensionRegistrationCompleted>& Waiters =
		PendingExtensionRequests.Add(DataPath);
	Waiters.Add(MoveTemp(OnComplete));
	const uint64 RequestGeneration = LifecycleGeneration;
	const uint64 ExtensionRequestId = NextExtensionRequestId++;
	ActiveExtensionRequestIds.Add(DataPath, ExtensionRequestId);
	FPendingExtensionLoad& PendingLoad = PendingExtensionLoads.Add(DataPath);
	PendingLoad.RequestId = ExtensionRequestId;

	auto ContinueWithLoadedData = [this, RequestGeneration, ExtensionRequestId, ExtensionData, DataPath]()
	{
		if (!IsExtensionRequestCurrent(DataPath, ExtensionRequestId))
		{
			return;
		}

		if (RequestGeneration != LifecycleGeneration)
		{
			CompleteExtensionRequest(DataPath, ExtensionRequestId, ENKMUIAsyncResult::Stale);
			return;
		}

		if (FPendingExtensionLoad* Load = PendingExtensionLoads.Find(DataPath))
		{
			Load->DataHandle.Reset();
		}

		UNKMUIExtensionData* LoadedData = ExtensionData.Get();
		if (!LoadedData)
		{
			CompleteExtensionRequest(DataPath, ExtensionRequestId, ENKMUIAsyncResult::LoadFailed);
			return;
		}
		PendingExtensionData.Add(DataPath, LoadedData);

		TArray<FNKMUIExtensionEntry> Entries;
		LoadedData->GetExtensionEntries(Entries);
		TArray<FSoftObjectPath> WidgetClassPaths;
		for (const FNKMUIExtensionEntry& Entry : Entries)
		{
			if (!Entry.WidgetClass.IsNull())
			{
				WidgetClassPaths.AddUnique(Entry.WidgetClass.ToSoftObjectPath());
			}
		}

		if (WidgetClassPaths.IsEmpty())
		{
			HandleExtensionWidgetClassesLoaded(DataPath, RequestGeneration, ExtensionRequestId);
			return;
		}

		TSharedPtr<FStreamableHandle> WidgetClassHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			WidgetClassPaths,
			FStreamableDelegate::CreateWeakLambda(this, [this, DataPath, RequestGeneration, ExtensionRequestId]()
			{
				HandleExtensionWidgetClassesLoaded(DataPath, RequestGeneration, ExtensionRequestId);
			}));
		if (FPendingExtensionLoad* Load = PendingExtensionLoads.Find(DataPath);
			Load && Load->RequestId == ExtensionRequestId)
		{
			Load->WidgetClassHandle = WidgetClassHandle;
		}
		TrackLifecycleHandle(WidgetClassHandle);
		if (!WidgetClassHandle.IsValid())
		{
			PendingExtensionData.Remove(DataPath);
			CompleteExtensionRequest(DataPath, ExtensionRequestId, ENKMUIAsyncResult::LoadFailed);
		}
	};

	if (ExtensionData.Get())
	{
		ContinueWithLoadedData();
		return;
	}

	TSharedPtr<FStreamableHandle> DataHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		DataPath,
		FStreamableDelegate::CreateWeakLambda(this, ContinueWithLoadedData));
	if (FPendingExtensionLoad* Load = PendingExtensionLoads.Find(DataPath);
		Load && Load->RequestId == ExtensionRequestId)
	{
		Load->DataHandle = DataHandle;
	}
	TrackLifecycleHandle(DataHandle);
	if (!DataHandle.IsValid())
	{
		CompleteExtensionRequest(DataPath, ExtensionRequestId, ENKMUIAsyncResult::LoadFailed);
	}
}

void UNKMUIManagerSubsystem::HandleExtensionWidgetClassesLoaded(
	const FSoftObjectPath& DataPath,
	uint64 RequestGeneration,
	uint64 ExtensionRequestId)
{
	if (!IsExtensionRequestCurrent(DataPath, ExtensionRequestId))
	{
		return;
	}

	if (RequestGeneration != LifecycleGeneration)
	{
		PendingExtensionData.Remove(DataPath);
		CompleteExtensionRequest(DataPath, ExtensionRequestId, ENKMUIAsyncResult::Stale);
		return;
	}

	UNKMUIExtensionData* LoadedData = PendingExtensionData.FindRef(DataPath);
	if (!LoadedData)
	{
		CompleteExtensionRequest(DataPath, ExtensionRequestId, ENKMUIAsyncResult::LoadFailed);
		return;
	}

	TArray<FNKMUIExtensionEntry> Entries;
	LoadedData->GetExtensionEntries(Entries);
	for (const FNKMUIExtensionEntry& Entry : Entries)
	{
		if (!Entry.WidgetClass.IsNull() && !Entry.WidgetClass.Get())
		{
			PendingExtensionData.Remove(DataPath);
			CompleteExtensionRequest(DataPath, ExtensionRequestId, ENKMUIAsyncResult::LoadFailed);
			return;
		}

	}

	FPendingExtensionLoad CompletedLoad;
	if (PendingExtensionLoads.RemoveAndCopyValue(DataPath, CompletedLoad)
		&& CompletedLoad.RequestId == ExtensionRequestId
		&& CompletedLoad.WidgetClassHandle.IsValid())
	{
		ReleaseRetainedWidgetClasses(DataPath);
		RetainedWidgetClassHandles.Add(DataPath, MoveTemp(CompletedLoad.WidgetClassHandle));
	}

	PendingExtensionData.Remove(DataPath);
	CachedExtensionData.AddUnique(LoadedData);
	const ENKMUIAsyncResult Result = RegisterExtensionData(LoadedData);
	if (Result != ENKMUIAsyncResult::NotReady)
	{
		CompleteExtensionRequest(DataPath, ExtensionRequestId, Result);
	}
}

ENKMUIAsyncResult UNKMUIManagerSubsystem::UnregisterExtensionsFromData(
	TSoftObjectPtr<UNKMUIExtensionData> ExtensionData,
	bool bReleaseLoadedAssets)
{
	if (ExtensionData.IsNull())
	{
		return ENKMUIAsyncResult::InvalidRequest;
	}

	const FSoftObjectPath DataPath = ExtensionData.ToSoftObjectPath();
	if (const uint64* ExtensionRequestId = ActiveExtensionRequestIds.Find(DataPath))
	{
		PendingExtensionData.Remove(DataPath);
		CompleteExtensionRequest(DataPath, *ExtensionRequestId, ENKMUIAsyncResult::Cancelled);
	}

	UnregisterExtensionHandles(DataPath);
	RegisteredExtensionDataPaths.Remove(DataPath);

	if (bReleaseLoadedAssets)
	{
		CachedExtensionData.RemoveAll([&DataPath](const TObjectPtr<UNKMUIExtensionData>& Data)
		{
			return Data && FSoftObjectPath(Data.Get()) == DataPath;
		});
		ReleaseRetainedWidgetClasses(DataPath);
	}

	return ENKMUIAsyncResult::Succeeded;
}

void UNKMUIManagerSubsystem::InjectUIExtensionsDynamic(
	TSoftObjectPtr<UNKMUIExtensionData> ExtensionData)
{
	RegisterExtensionsFromData(ExtensionData);
}

void UNKMUIManagerSubsystem::RegisterCachedExtensions()
{
	for (UNKMUIExtensionData* ExtensionData : CachedExtensionData)
	{
		const FSoftObjectPath DataPath(ExtensionData);
		const ENKMUIAsyncResult Result = RegisterExtensionData(ExtensionData);
		if (const uint64* ExtensionRequestId = ActiveExtensionRequestIds.Find(DataPath))
		{
			CompleteExtensionRequest(DataPath, *ExtensionRequestId, Result);
		}
	}
}

ENKMUIAsyncResult UNKMUIManagerSubsystem::RegisterExtensionData(
	UNKMUIExtensionData* ExtensionData)
{
	if (!CurrentPolicy || !ExtensionData || !GetGameInstance())
	{
		UE_LOG(LogTemp, Display, TEXT("[NKMStartup][UI] Extension registration deferred. Policy=%s Data=%s GameInstance=%s"),
			*GetNameSafe(CurrentPolicy), *GetNameSafe(ExtensionData), *GetNameSafe(GetGameInstance()));
		return ENKMUIAsyncResult::NotReady;
	}

	const FSoftObjectPath DataPath(ExtensionData);
	if (RegisteredExtensionDataPaths.Contains(DataPath))
	{
		return ENKMUIAsyncResult::Succeeded;
	}

	UWorld* World = GetGameInstance()->GetWorld();
	UUIExtensionSubsystem* ExtensionSubsystem = World
		? World->GetSubsystem<UUIExtensionSubsystem>()
		: nullptr;
	if (!ExtensionSubsystem)
	{
		return ENKMUIAsyncResult::NotReady;
	}

	TArray<FNKMUIExtensionEntry> Entries;
	ExtensionData->GetExtensionEntries(Entries);
	Entries.Sort([](const FNKMUIExtensionEntry& A, const FNKMUIExtensionEntry& B)
	{
		return A.Priority < B.Priority;
	});

	for (const FNKMUIExtensionEntry& Entry : Entries)
	{
		if (!Entry.WidgetClass.IsNull() && !Entry.WidgetClass.Get())
		{
			return ENKMUIAsyncResult::LoadFailed;
		}

	}

	TArray<FUIExtensionHandle>& ExtensionHandles = RegisteredExtensionHandles.FindOrAdd(DataPath);
	for (const FNKMUIExtensionEntry& Entry : Entries)
	{
		if (!Entry.ExtensionPointTag.IsValid())
		{
			continue;
		}

		if (const TSubclassOf<UUserWidget> LoadedWidgetClass = Entry.WidgetClass.Get())
		{
			FUIExtensionHandle Handle = ExtensionSubsystem->RegisterExtensionAsWidget(
				Entry.ExtensionPointTag,
				LoadedWidgetClass,
				Entry.Priority);
			if (Handle.IsValid())
			{
				ExtensionHandles.Add(MoveTemp(Handle));
			}
		}
	}

	RegisteredExtensionDataPaths.Add(DataPath);
	UE_LOG(LogTemp, Display, TEXT("[NKMStartup][UI] Extension registered. Data=%s Entries=%d Handles=%d"),
		*DataPath.ToString(), Entries.Num(), ExtensionHandles.Num());
	return ENKMUIAsyncResult::Succeeded;
}

void UNKMUIManagerSubsystem::CompleteExtensionRequest(
	const FSoftObjectPath& DataPath,
	uint64 ExtensionRequestId,
	ENKMUIAsyncResult Result)
{
	if (!IsExtensionRequestCurrent(DataPath, ExtensionRequestId))
	{
		return;
	}

	ActiveExtensionRequestIds.Remove(DataPath);
	FPendingExtensionLoad PendingLoad;
	if (PendingExtensionLoads.RemoveAndCopyValue(DataPath, PendingLoad)
		&& PendingLoad.RequestId == ExtensionRequestId)
	{
		const TSharedPtr<FStreamableHandle> Handles[] =
		{
			PendingLoad.DataHandle,
			PendingLoad.WidgetClassHandle
		};
		for (const TSharedPtr<FStreamableHandle>& Handle : Handles)
		{
			if (Handle.IsValid())
			{
				if (Handle->IsActive())
				{
					Handle->CancelHandle();
				}
				else
				{
					Handle->ReleaseHandle();
				}
			}
		}
	}

	TArray<FNKMUIExtensionRegistrationCompleted> Waiters;
	if (PendingExtensionRequests.RemoveAndCopyValue(DataPath, Waiters))
	{
		for (FNKMUIExtensionRegistrationCompleted& Waiter : Waiters)
		{
			Waiter.ExecuteIfBound(Result);
		}
	}
	PruneCompletedLifecycleHandles();
}

bool UNKMUIManagerSubsystem::IsExtensionRequestCurrent(
	const FSoftObjectPath& DataPath,
	uint64 ExtensionRequestId) const
{
	const uint64* ActiveRequestId = ActiveExtensionRequestIds.Find(DataPath);
	return ActiveRequestId && *ActiveRequestId == ExtensionRequestId;
}

void UNKMUIManagerSubsystem::UnregisterExtensionHandles(const FSoftObjectPath& DataPath)
{
	TArray<FUIExtensionHandle> Handles;
	if (RegisteredExtensionHandles.RemoveAndCopyValue(DataPath, Handles))
	{
		for (FUIExtensionHandle& Handle : Handles)
		{
			Handle.Unregister();
		}
	}
}

void UNKMUIManagerSubsystem::ReleaseRetainedWidgetClasses(const FSoftObjectPath& DataPath)
{
	TSharedPtr<FStreamableHandle> Handle;
	if (RetainedWidgetClassHandles.RemoveAndCopyValue(DataPath, Handle) && Handle.IsValid())
	{
		Handle->ReleaseHandle();
	}
}

TSoftClassPtr<UNKMUIPolicy> UNKMUIManagerSubsystem::GetConfiguredPolicyClass() const
{
	return GetDefault<UNKMUIManagerSettings>()->DefaultPolicyClass;
}

void UNKMUIManagerSubsystem::GetConfiguredExtensionDataAssets(
	TArray<TSoftObjectPtr<UNKMUIExtensionData>>& OutAssets) const
{
	OutAssets = GetDefault<UNKMUIManagerSettings>()->ExtensionDataAssets;
}

void UNKMUIManagerSubsystem::TrackLifecycleHandle(const TSharedPtr<FStreamableHandle>& Handle)
{
	if (Handle.IsValid())
	{
		LifecycleLoadHandles.Add(Handle);
	}
}

void UNKMUIManagerSubsystem::PruneCompletedLifecycleHandles()
{
	LifecycleLoadHandles.RemoveAll([](const TSharedPtr<FStreamableHandle>& Handle)
	{
		return !Handle.IsValid() || Handle->HasLoadCompleted();
	});
}

void UNKMUIManagerSubsystem::CancelOutstandingRequests(ENKMUIAsyncResult Result)
{
	TArray<FPendingWidgetRequest> CancelledWidgetRequests;
	CancelledWidgetRequests.Reserve(PendingWidgetRequests.Num());
	for (TPair<int32, FPendingWidgetRequest>& Pair : PendingWidgetRequests)
	{
		CancelledWidgetRequests.Add(MoveTemp(Pair.Value));
	}
	PendingWidgetRequests.Reset();

	for (FPendingWidgetRequest& Request : CancelledWidgetRequests)
	{
		if (Request.Handle.IsValid() && Request.Handle->IsActive())
		{
			Request.Handle->CancelHandle();
		}
		Request.Completion.ExecuteIfBound(Result, nullptr);
	}

	for (TSharedPtr<FStreamableHandle>& Handle : LifecycleLoadHandles)
	{
		if (Handle.IsValid() && Handle->IsActive())
		{
			Handle->CancelHandle();
		}
	}
	LifecycleLoadHandles.Reset();
	PendingExtensionData.Reset();
	PendingExtensionLoads.Reset();
	ActiveExtensionRequestIds.Reset();

	TMap<FSoftObjectPath, TArray<FNKMUIExtensionRegistrationCompleted>> CancelledExtensionRequests =
		MoveTemp(PendingExtensionRequests);
	PendingExtensionRequests.Reset();
	for (TPair<FSoftObjectPath, TArray<FNKMUIExtensionRegistrationCompleted>>& Pair :
		CancelledExtensionRequests)
	{
		for (FNKMUIExtensionRegistrationCompleted& Waiter : Pair.Value)
		{
			Waiter.ExecuteIfBound(Result);
		}
	}
}


void UNKMUIManagerSubsystem::SetGameplayHUDVisible(bool bVisible)
{
	bGameplayHUDVisible = bVisible;
	GameplayHUDVisibilityChanged.Broadcast(bGameplayHUDVisible);
}
