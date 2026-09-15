#include "ListenServerSessionSubsystem.h"

#include "ListenServerNetworkDiagnostics.h"
#include "ListenServerNetworkLog.h"
#include "ListenServerNetworkPolicy.h"
#include "ListenServerNetworkSettings.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	constexpr int32 MaxHostListenRetryCount = 3;
	constexpr float HostListenRetryDelaySeconds = 1.0f;

	enum class EListenServerAsyncPurpose : uint8
	{
		None,
		HostReplace,
		HostCreate,
		Find,
		QuickFind,
		Join,
		QuickJoin,
		InviteJoin,
		SessionStateUpdate,
		HostGameUpdate,
		HostGameStart,
		LeaveEnd,
		LeaveDestroy,
		RecoveryDestroy,
		InviteDestroy
	};

	const TCHAR* GetAdvertisedSessionStateValue(EListenServerAdvertisedSessionState State)
	{
		switch (State)
		{
		case EListenServerAdvertisedSessionState::Lobby:
			return TEXT("Lobby");
		case EListenServerAdvertisedSessionState::InGame:
			return TEXT("InGame");
		case EListenServerAdvertisedSessionState::Closed:
			return TEXT("Closed");
		default:
			return TEXT("Closed");
		}
	}

	void ApplySessionJoinability(FOnlineSessionSettings& Settings, bool bJoinable)
	{
		Settings.bShouldAdvertise = bJoinable;
		Settings.bAllowJoinInProgress = bJoinable;
		Settings.bAllowInvites = bJoinable;
		Settings.bAllowJoinViaPresence = bJoinable;
		Settings.bAllowJoinViaPresenceFriendsOnly = false;
		Settings.Set(
			ListenServerNetworkKeys::Joinable,
			bJoinable ? FString(TEXT("1")) : FString(TEXT("0")),
			EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
		);
	}

	struct FPendingTravel
	{
		uint64 OperationId = 0;
		FString ExpectedPackage;
		FString PreviousPackage;
		EListenServerRole Role = EListenServerRole::None;
		EListenServerConnectionState ConnectionState = EListenServerConnectionState::Offline;
		bool bAcceptAnyRemotePackage = false;
		bool bReturnToMenu = false;
	};

	FListenServerOperationResult MakeResult(bool bSucceeded, EListenServerError Error, const TCHAR* UserMessage, const FString& InternalError, bool bRecoverable = true)
	{
		FListenServerOperationResult Result;
		Result.bSucceeded = bSucceeded;
		Result.bRecoverable = bRecoverable;
		Result.Error = Error;
		Result.UserMessage = FText::FromString(UserMessage);
		Result.InternalError = InternalError;
		return Result;
	}

	bool IsMapConfiguredAndPresent(const FSoftObjectPath& Map, FString& OutLongPackageName)
	{
		OutLongPackageName = Map.GetLongPackageName();
		return !OutLongPackageName.IsEmpty() && FPackageName::IsValidLongPackageName(OutLongPackageName) && FPackageName::DoesPackageExist(OutLongPackageName);
	}

	bool PackageNamesMatch(const FString& LoadedPackage, const FString& ExpectedPackage)
	{
		if (ExpectedPackage.IsEmpty())
		{
			return true;
		}
		return LoadedPackage == ExpectedPackage || FPackageName::GetShortName(LoadedPackage).EndsWith(FPackageName::GetShortName(ExpectedPackage));
	}

	bool GetStringSetting(const FOnlineSessionSettings& Settings, FName Key, FString& OutValue)
	{
		OutValue.Reset();
		return Settings.Get(Key, OutValue);
	}

	ListenServerNetworkPolicy::EJoinFailureCode TranslateJoinFailure(EOnJoinSessionCompleteResult::Type Result)
	{
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::SessionIsFull:
			return ListenServerNetworkPolicy::EJoinFailureCode::SessionFull;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			return ListenServerNetworkPolicy::EJoinFailureCode::SessionNotFound;
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
			return ListenServerNetworkPolicy::EJoinFailureCode::ConnectString;
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			return ListenServerNetworkPolicy::EJoinFailureCode::AlreadyJoined;
		default:
			return ListenServerNetworkPolicy::EJoinFailureCode::Unknown;
		}
	}
}

struct FListenServerSessionSubsystemImpl
{
	explicit FListenServerSessionSubsystemImpl(UListenServerSessionSubsystem* InOwner)
		: Owner(InOwner)
	{
	}

	TWeakObjectPtr<UListenServerSessionSubsystem> Owner;
	EListenServerRole Role = EListenServerRole::None;
	EListenServerConnectionState ConnectionState = EListenServerConnectionState::Offline;
	EListenServerOperation Operation = EListenServerOperation::None;
	uint64 OperationCounter = 0;
	uint64 ActiveOperationId = 0;
	bool bOperationCancelled = false;
	bool bFailureRecoveryActive = false;
	bool bInitialized = false;
	FListenServerOperationResult LastResult;

	int32 SearchGeneration = INDEX_NONE;
	TArray<FListenServerSearchResult> SearchResults;
	TArray<FOnlineSessionSearchResult> RawSearchResults;
	TArray<FListenServerParticipant> Participants;
	FListenServerConnectionDiagnostics ConnectionDiagnostics;
	TSharedPtr<FOnlineSessionSearch> ActiveSearch;
	FListenServerSearchRequest ActiveSearchRequest;
	FListenServerHostRequest ActiveHostRequest;
	FListenServerQuickMatchRequest ActiveQuickMatchRequest;
	bool bQuickMatchActive = false;
	int32 QuickSearchAttempt = 0;
	int32 QuickJoinIndex = INDEX_NONE;
	bool bHostListenRetryPending = false;
	int32 HostListenRetryCount = 0;
	float HostListenRetryDelayRemaining = 0.0f;
	EListenServerConnectionState PendingJoinConnectionState = EListenServerConnectionState::Lobby;

	TUniquePtr<FOnlineSessionSearchResult> PendingInvite;
	FString PendingInviteSessionId;
	TUniquePtr<FPendingTravel> PendingTravel;
	FSoftObjectPath PendingHostGameMap;
	EListenServerAdvertisedSessionState PendingAdvertisedSessionState = EListenServerAdvertisedSessionState::Closed;
	bool bPendingAllowNewParticipants = false;
	EListenServerAsyncPurpose Purpose = EListenServerAsyncPurpose::None;
	bool bLeaveEndFailed = false;
	EListenServerError RecoveryError = EListenServerError::Unknown;
	FString RecoveryUserMessage;
	FString RecoveryInternalMessage;

	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle CancelFindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle UpdateHandle;
	FDelegateHandle StartHandle;
	FDelegateHandle EndHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle SessionFailureHandle;
	FDelegateHandle InviteReceivedHandle;
	FDelegateHandle InviteAcceptedHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
	FTSTicker::FDelegateHandle TimeoutHandle;
	FTSTicker::FDelegateHandle ParticipantObservationHandle;
	FTSTicker::FDelegateHandle ConnectionDiagnosticsHandle;

	IOnlineSubsystem* GetOnlineSubsystem() const
	{
		return IOnlineSubsystem::Get();
	}

	IOnlineSessionPtr GetSessionInterface() const
	{
		IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystem();
		return OnlineSubsystem != nullptr ? OnlineSubsystem->GetSessionInterface() : nullptr;
	}

	IOnlineIdentityPtr GetIdentityInterface() const
	{
		IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystem();
		return OnlineSubsystem != nullptr ? OnlineSubsystem->GetIdentityInterface() : nullptr;
	}

	IOnlineExternalUIPtr GetExternalUIInterface() const
	{
		IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystem();
		return OnlineSubsystem != nullptr ? OnlineSubsystem->GetExternalUIInterface() : nullptr;
	}

	UWorld* GetWorld() const
	{
		const UListenServerSessionSubsystem* Subsystem = Owner.Get();
		return Subsystem != nullptr ? Subsystem->GetWorld() : nullptr;
	}

	UGameInstance* GetGameInstance() const
	{
		const UListenServerSessionSubsystem* Subsystem = Owner.Get();
		return Subsystem != nullptr ? Subsystem->GetGameInstance() : nullptr;
	}

	const UListenServerNetworkSettings* GetSettings() const
	{
		return GetDefault<UListenServerNetworkSettings>();
	}

	void Initialize();
	void Deinitialize();
	void EnsurePersistentDelegates();
	void ClearPersistentDelegates();
	void ClearOperationDelegates();
	int32 GetRegisteredDelegateCount() const;
	bool TickParticipantObservation(float DeltaTime);
	void RefreshParticipants();
	void ClearParticipants();
	bool TickConnectionDiagnostics(float DeltaTime);
	void RefreshConnectionDiagnostics();
	void RetryPendingHostListen(float DeltaTime);

	bool ValidateOnlineAccess(EListenServerOperation RequestedOperation);
	bool Reject(EListenServerOperation RequestedOperation, EListenServerError Error, const TCHAR* UserMessage, const FString& InternalError);
	bool BeginOperation(EListenServerOperation NewOperation);
	void SetOperation(EListenServerOperation NewOperation, const TCHAR* Reason);
	void SetRoleAndConnection(EListenServerRole NewRole, EListenServerConnectionState NewConnection, const TCHAR* Reason);
	void BroadcastState() const;
	bool IsExpectedCallback(uint64 CallbackOperationId, EListenServerOperation ExpectedOperation, const TCHAR* CallbackName) const;
	void FinishSuccess(const TCHAR* UserMessage, const FString& InternalMessage = FString(), bool bRecoverable = true);
	void FinishFailure(EListenServerError Error, const TCHAR* UserMessage, const FString& InternalMessage, bool bRecoverable = true);
	void ProcessPendingInvite();

	void StartTimeout(float Seconds, EListenServerOperation TimedOperation);
	void ClearTimeout();
	void HandleTimeout(uint64 TimedOperationId, EListenServerOperation TimedOperation);

	bool Host(const FListenServerHostRequest& Request);
	void StartCreate(uint64 OperationId);
	void HandleCreateComplete(uint64 CallbackOperationId, FName SessionName, bool bWasSuccessful);
	void StartDestroy(uint64 OperationId, EListenServerAsyncPurpose DestroyPurpose);
	void HandleDestroyComplete(uint64 CallbackOperationId, FName SessionName, bool bWasSuccessful);
	void CleanupStaleCreatedOrJoinedSession(const TCHAR* Reason);

	bool Find(const FListenServerSearchRequest& Request, bool bForQuickMatch = false);
	void StartFind(uint64 OperationId, bool bForQuickMatch);
	void HandleFindComplete(uint64 CallbackOperationId, bool bWasSuccessful);
	void FilterSearchResults();
	EListenServerError CheckRawCompatibility(const FOnlineSessionSearchResult& RawResult, const FListenServerSearchRequest& Request, bool bRequireOpenSlot) const;
	void FillPublicSearchResult(const FOnlineSessionSearchResult& RawResult, int32 PublicIndex, FListenServerSearchResult& OutResult) const;

	bool Join(const FListenServerSearchResultHandle& ResultHandle);
	void StartJoinRaw(uint64 OperationId, const FOnlineSessionSearchResult& RawResult, EListenServerAsyncPurpose JoinPurpose);
	void HandleJoinComplete(uint64 CallbackOperationId, FName SessionName, EOnJoinSessionCompleteResult::Type JoinResult);
	void ContinueQuickMatchAfterJoinFailure(EListenServerError Error, const FString& InternalMessage);
	void ContinueQuickMatchAfterSearch();

	bool QuickMatch(const FListenServerQuickMatchRequest& Request);
	bool Leave();
	void StartEndForLeave(uint64 OperationId);
	void HandleEndComplete(uint64 CallbackOperationId, FName SessionName, bool bWasSuccessful);
	void ReturnToMainMenu(uint64 OperationId, bool bFromRecovery);
	void CompleteLocalCleanup();

	bool HostTravel(const FSoftObjectPath& Map);
	bool UpdateHostedSessionState(EListenServerAdvertisedSessionState NewState, bool bAllowNewParticipants);
	void StartSessionStateUpdate(uint64 OperationId);
	void StartHostGameUpdate(uint64 OperationId);
	void HandleUpdateComplete(uint64 CallbackOperationId, FName SessionName, bool bWasSuccessful);
	void StartHostGameSession(uint64 OperationId);
	void HandleStartComplete(uint64 CallbackOperationId, FName SessionName, bool bWasSuccessful);
	bool BeginTravel(uint64 OperationId, const FSoftObjectPath& Map, EListenServerRole TravelRole, EListenServerConnectionState TargetState, bool bListenOpenLevel, bool bReturnToMenu);
	bool StartHostListenTravel();
	bool BeginClientTravel(uint64 OperationId, const FString& ConnectString, EListenServerConnectionState TargetState);
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	bool Cancel();
	void HandleCancelFindComplete(uint64 CallbackOperationId, bool bWasSuccessful);
	bool ShowInviteUI();
	void HandleInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& AppId, const FOnlineSessionSearchResult& InviteResult);
	void HandleInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	void QueueOrJoinInvite(const FOnlineSessionSearchResult& InviteResult);

	void HandleNetworkFailure(UWorld* FailedWorld, UNetDriver* FailedNetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandleSessionFailure(const FUniqueNetId& PlayerId, ESessionFailure::Type FailureType);
	void BeginRecovery(EListenServerError Error, const TCHAR* UserMessage, const FString& InternalMessage);

	FListenServerDebugSnapshot GetDebugSnapshot() const;
	FListenServerConfigurationReport ValidateConfiguration() const;
};

UListenServerSessionSubsystem::UListenServerSessionSubsystem()
	: Impl(new FListenServerSessionSubsystemImpl(this))
{
}

UListenServerSessionSubsystem::~UListenServerSessionSubsystem()
{
	delete Impl;
	Impl = nullptr;
}

void UListenServerSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Impl->Initialize();
}

void UListenServerSessionSubsystem::Deinitialize()
{
	Impl->Deinitialize();
	Super::Deinitialize();
}

void FListenServerSessionSubsystemImpl::Initialize()
{
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;

	TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
	if (GEngine != nullptr)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddLambda([WeakOwner](UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
		{
			if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
			{
				Subsystem->Impl->HandleNetworkFailure(World, NetDriver, FailureType, ErrorString);
			}
		});
		TravelFailureHandle = GEngine->OnTravelFailure().AddLambda([WeakOwner](UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
		{
			if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
			{
				Subsystem->Impl->HandleTravelFailure(World, FailureType, ErrorString);
			}
		});
	}

	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddLambda([WeakOwner](const FString& MapName)
	{
		if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
		{
			Subsystem->Impl->HandlePreLoadMap(MapName);
		}
	});
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([WeakOwner](UWorld* LoadedWorld)
	{
		if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
		{
			Subsystem->Impl->HandlePostLoadMap(LoadedWorld);
		}
	});

	EnsurePersistentDelegates();
	ParticipantObservationHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FListenServerSessionSubsystemImpl::TickParticipantObservation),
		0.5f
	);
	ConnectionDiagnosticsHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FListenServerSessionSubsystemImpl::TickConnectionDiagnostics),
		1.0f
	);
	const IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystem();
	UE_LOG(LogListenServerNetwork, Log, TEXT("Initialized. OSS=%s Role=None Connection=Offline Operation=None"), OnlineSubsystem != nullptr ? *OnlineSubsystem->GetSubsystemName().ToString() : TEXT("Unavailable"));
}

void FListenServerSessionSubsystemImpl::Deinitialize()
{
	if (!bInitialized)
	{
		return;
	}

	ClearTimeout();
	if (ParticipantObservationHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ParticipantObservationHandle);
		ParticipantObservationHandle.Reset();
	}
	if (ConnectionDiagnosticsHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ConnectionDiagnosticsHandle);
		ConnectionDiagnosticsHandle.Reset();
	}
	ClearOperationDelegates();
	ClearPersistentDelegates();
	if (GEngine != nullptr)
	{
		if (NetworkFailureHandle.IsValid())
		{
			GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		}
		if (TravelFailureHandle.IsValid())
		{
			GEngine->OnTravelFailure().Remove(TravelFailureHandle);
		}
	}
	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	}
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	}
	PendingInvite.Reset();
	PendingTravel.Reset();
	ActiveSearch.Reset();
	RawSearchResults.Reset();
	SearchResults.Reset();
	Participants.Reset();
	bInitialized = false;
	UE_LOG(LogListenServerNetwork, Log, TEXT("Deinitialized and removed all delegates"));
}

bool FListenServerSessionSubsystemImpl::TickParticipantObservation(float)
{
	RefreshParticipants();
	return true;
}

void FListenServerSessionSubsystemImpl::RefreshParticipants()
{
	TArray<FListenServerParticipant> NewParticipants;
	UWorld* World = GetWorld();
	AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	if (ConnectionState != EListenServerConnectionState::Offline && GameState != nullptr)
	{
		FString HostPlatformUserId;
		IOnlineSessionPtr Sessions = GetSessionInterface();
		if (Sessions.IsValid())
		{
			if (const FNamedOnlineSession* NamedSession = Sessions->GetNamedSession(NAME_GameSession))
			{
				if (NamedSession->OwningUserId.IsValid())
				{
					HostPlatformUserId = NamedSession->OwningUserId->ToString();
				}
			}
		}

		TSet<const APlayerState*> LocalPlayerStates;
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
			{
				APlayerController* PlayerController = LocalPlayer != nullptr
					? LocalPlayer->GetPlayerController(World)
					: nullptr;
				if (PlayerController != nullptr && PlayerController->PlayerState != nullptr)
				{
					LocalPlayerStates.Add(PlayerController->PlayerState);
				}
			}
		}

		NewParticipants.Reserve(GameState->PlayerArray.Num());
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (!IsValid(PlayerState) || PlayerState->IsOnlyASpectator())
			{
				continue;
			}

			FListenServerParticipant& Participant = NewParticipants.AddDefaulted_GetRef();
			Participant.PlayerId = PlayerState->GetPlayerId();
			Participant.DisplayName = PlayerState->GetPlayerName();
			const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId();
			Participant.PlatformUserId = UniqueId.IsValid() ? UniqueId.ToString() : FString();
			Participant.PingMilliseconds = FMath::Max(0, FMath::RoundToInt(PlayerState->GetPingInMilliseconds()));
			Participant.bIsLocalPlayer = LocalPlayerStates.Contains(PlayerState);
			Participant.bIsHost = !HostPlatformUserId.IsEmpty()
				? Participant.PlatformUserId == HostPlatformUserId
				: Role == EListenServerRole::Host && Participant.bIsLocalPlayer;
		}
	}

	NewParticipants.Sort([](const FListenServerParticipant& Left, const FListenServerParticipant& Right)
	{
		if (Left.PlayerId != Right.PlayerId)
		{
			return Left.PlayerId < Right.PlayerId;
		}
		if (Left.PlatformUserId != Right.PlatformUserId)
		{
			return Left.PlatformUserId < Right.PlatformUserId;
		}
		return Left.DisplayName < Right.DisplayName;
	});

	if (Participants == NewParticipants)
	{
		return;
	}

	Participants = MoveTemp(NewParticipants);
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnParticipantsChanged.Broadcast();
	}
}

void FListenServerSessionSubsystemImpl::ClearParticipants()
{
	if (Participants.IsEmpty())
	{
		return;
	}

	Participants.Reset();
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnParticipantsChanged.Broadcast();
	}
}

bool FListenServerSessionSubsystemImpl::TickConnectionDiagnostics(float DeltaTime)
{
	if (PendingTravel.IsValid())
	{
		HandlePostLoadMap(GetWorld());
	}
	RetryPendingHostListen(DeltaTime);
	RefreshConnectionDiagnostics();
	return true;
}

void FListenServerSessionSubsystemImpl::RetryPendingHostListen(float DeltaTime)
{
	if (!bHostListenRetryPending || !PendingTravel.IsValid())
	{
		return;
	}
	HostListenRetryDelayRemaining -= DeltaTime;
	if (HostListenRetryDelayRemaining > 0.0f)
	{
		return;
	}

	bHostListenRetryPending = false;
	UWorld* World = GetWorld();
	const FString CurrentPackage = World != nullptr && World->GetPackage() != nullptr ? World->GetPackage()->GetName() : FString();
	const bool bExpectedSourceWorld = World != nullptr && PackageNamesMatch(CurrentPackage, PendingTravel->PreviousPackage);
	if (!bExpectedSourceWorld || World->GetNetMode() != NM_Standalone)
	{
		BeginRecovery(EListenServerError::TravelFailed, TEXT("The lobby network listener could not start."), TEXT("Host listen retry found an unexpected world state"));
		return;
	}

	++HostListenRetryCount;
	UE_LOG(LogListenServerNetwork, Warning, TEXT("Retrying host listen startup. Attempt=%d/%d"), HostListenRetryCount, MaxHostListenRetryCount);
	if (!StartHostListenTravel() && !bHostListenRetryPending)
	{
		BeginRecovery(EListenServerError::TravelFailed, TEXT("The lobby network listener could not start."), TEXT("Host listen retry failed without a retryable network error"));
	}
}

void FListenServerSessionSubsystemImpl::RefreshConnectionDiagnostics()
{
	FListenServerConnectionDiagnostics NewDiagnostics;
	NewDiagnostics.Role = Role;
	NewDiagnostics.ConnectionState = ConnectionState;

	if (Role == EListenServerRole::Host && ConnectionState != EListenServerConnectionState::Offline)
	{
		NewDiagnostics.LinkState = EListenServerLinkState::LocalHost;
	}
	else
	{
		UWorld* World = GetWorld();
		UNetDriver* NetDriver = World != nullptr ? World->GetNetDriver() : nullptr;
		UNetConnection* ServerConnection = NetDriver != nullptr ? NetDriver->ServerConnection.Get() : nullptr;
		if (ServerConnection != nullptr && ServerConnection->GetConnectionState() == USOCK_Open)
		{
			NewDiagnostics.LinkState = EListenServerLinkState::Connected;
			if (ServerConnection->InTotalPackets > 0 && ServerConnection->OutTotalPackets > 0)
			{
				NewDiagnostics.PingMilliseconds = FMath::Max(0, FMath::RoundToInt(ServerConnection->AvgLag * 1000.0f));
				NewDiagnostics.IncomingPacketLossPercent = FMath::Clamp(ServerConnection->GetInLossPercentage().GetAvgLossPercentage() * 100.0f, 0.0f, 100.0f);
				NewDiagnostics.OutgoingPacketLossPercent = FMath::Clamp(ServerConnection->GetOutLossPercentage().GetAvgLossPercentage() * 100.0f, 0.0f, 100.0f);
				NewDiagnostics.Quality = ListenServerNetworkDiagnostics::EvaluateQuality(
					NewDiagnostics.PingMilliseconds,
					NewDiagnostics.IncomingPacketLossPercent,
					NewDiagnostics.OutgoingPacketLossPercent
				);
			}
		}
		else if ((ServerConnection != nullptr && ServerConnection->GetConnectionState() == USOCK_Pending)
			|| Operation == EListenServerOperation::Joining
			|| (PendingTravel.IsValid() && PendingTravel->Role == EListenServerRole::Client))
		{
			NewDiagnostics.LinkState = EListenServerLinkState::Connecting;
		}
	}

	if (ConnectionDiagnostics == NewDiagnostics)
	{
		return;
	}

	ConnectionDiagnostics = NewDiagnostics;
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnConnectionDiagnosticsChanged.Broadcast(ConnectionDiagnostics);
	}
}

void FListenServerSessionSubsystemImpl::EnsurePersistentDelegates()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
	if (!SessionFailureHandle.IsValid())
	{
		SessionFailureHandle = Sessions->AddOnSessionFailureDelegate_Handle(FOnSessionFailureDelegate::CreateLambda([WeakOwner](const FUniqueNetId& PlayerId, ESessionFailure::Type FailureType)
		{
			if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
			{
				Subsystem->Impl->HandleSessionFailure(PlayerId, FailureType);
			}
		}));
	}
	if (!InviteReceivedHandle.IsValid())
	{
		InviteReceivedHandle = Sessions->AddOnSessionInviteReceivedDelegate_Handle(FOnSessionInviteReceivedDelegate::CreateLambda([WeakOwner](const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& AppId, const FOnlineSessionSearchResult& InviteResult)
		{
			if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
			{
				Subsystem->Impl->HandleInviteReceived(UserId, FromId, AppId, InviteResult);
			}
		}));
	}
	if (!InviteAcceptedHandle.IsValid())
	{
		InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateLambda([WeakOwner](const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
		{
			if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
			{
				Subsystem->Impl->HandleInviteAccepted(bWasSuccessful, ControllerId, MoveTemp(UserId), InviteResult);
			}
		}));
	}
	UE_LOG(LogListenServerNetwork, Verbose, TEXT("Persistent OSS delegates registered"));
}

void FListenServerSessionSubsystemImpl::ClearPersistentDelegates()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		if (SessionFailureHandle.IsValid())
		{
			Sessions->ClearOnSessionFailureDelegate_Handle(SessionFailureHandle);
		}
		if (InviteReceivedHandle.IsValid())
		{
			Sessions->ClearOnSessionInviteReceivedDelegate_Handle(InviteReceivedHandle);
		}
		if (InviteAcceptedHandle.IsValid())
		{
			Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
		}
	}
	SessionFailureHandle.Reset();
	InviteReceivedHandle.Reset();
	InviteAcceptedHandle.Reset();
	UE_LOG(LogListenServerNetwork, Verbose, TEXT("Persistent OSS delegates removed"));
}

void FListenServerSessionSubsystemImpl::ClearOperationDelegates()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		if (CreateHandle.IsValid()) Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		if (FindHandle.IsValid()) Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		if (CancelFindHandle.IsValid()) Sessions->ClearOnCancelFindSessionsCompleteDelegate_Handle(CancelFindHandle);
		if (JoinHandle.IsValid()) Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		if (UpdateHandle.IsValid()) Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateHandle);
		if (StartHandle.IsValid()) Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartHandle);
		if (EndHandle.IsValid()) Sessions->ClearOnEndSessionCompleteDelegate_Handle(EndHandle);
		if (DestroyHandle.IsValid()) Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	CreateHandle.Reset();
	FindHandle.Reset();
	CancelFindHandle.Reset();
	JoinHandle.Reset();
	UpdateHandle.Reset();
	StartHandle.Reset();
	EndHandle.Reset();
	DestroyHandle.Reset();
	UE_LOG(LogListenServerNetwork, Verbose, TEXT("Operation delegates removed"));
}

int32 FListenServerSessionSubsystemImpl::GetRegisteredDelegateCount() const
{
	const FDelegateHandle Handles[] = { CreateHandle, FindHandle, CancelFindHandle, JoinHandle, UpdateHandle, StartHandle, EndHandle, DestroyHandle, SessionFailureHandle, InviteReceivedHandle, InviteAcceptedHandle, NetworkFailureHandle, TravelFailureHandle, PreLoadMapHandle, PostLoadMapHandle };
	int32 Count = 0;
	for (const FDelegateHandle& Handle : Handles)
	{
		Count += Handle.IsValid() ? 1 : 0;
	}
	return Count;
}

bool FListenServerSessionSubsystemImpl::ValidateOnlineAccess(EListenServerOperation RequestedOperation)
{
	EnsurePersistentDelegates();
	IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystem();
	if (OnlineSubsystem == nullptr)
	{
		return Reject(RequestedOperation, EListenServerError::OnlineSubsystemUnavailable, TEXT("The online subsystem is unavailable."), TEXT("IOnlineSubsystem::Get returned null"));
	}
	if (OnlineSubsystem->GetSubsystemName() != STEAM_SUBSYSTEM)
	{
		return Reject(RequestedOperation, EListenServerError::SteamUnavailable, TEXT("Steam is not the active online subsystem."), FString::Printf(TEXT("Active OSS is %s"), *OnlineSubsystem->GetSubsystemName().ToString()));
	}
	if (!GetSessionInterface().IsValid())
	{
		return Reject(RequestedOperation, EListenServerError::SessionInterfaceUnavailable, TEXT("Steam sessions are unavailable."), TEXT("GetSessionInterface returned null"));
	}
	IOnlineIdentityPtr Identity = GetIdentityInterface();
	if (!Identity.IsValid())
	{
		return Reject(RequestedOperation, EListenServerError::IdentityInterfaceUnavailable, TEXT("Steam identity is unavailable."), TEXT("GetIdentityInterface returned null"));
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance == nullptr || GameInstance->GetFirstGamePlayer() == nullptr)
	{
		return Reject(RequestedOperation, EListenServerError::InvalidLocalUser, TEXT("The primary local player is unavailable."), TEXT("No primary LocalPlayer"));
	}
	if (GameInstance->GetLocalPlayers().Num() != 1)
	{
		return Reject(RequestedOperation, EListenServerError::MultipleLocalUsersUnsupported, TEXT("Exactly one local player is supported."), FString::Printf(TEXT("LocalPlayer count is %d"), GameInstance->GetLocalPlayers().Num()));
	}
	if (Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn || !Identity->GetUniquePlayerId(0).IsValid())
	{
		return Reject(RequestedOperation, EListenServerError::NotLoggedIn, TEXT("Sign in to Steam before using online sessions."), TEXT("LocalUserNum 0 is not logged in"));
	}
	return true;
}

bool FListenServerSessionSubsystemImpl::Reject(EListenServerOperation RequestedOperation, EListenServerError Error, const TCHAR* UserMessage, const FString& InternalError)
{
	LastResult = MakeResult(false, Error, UserMessage, InternalError);
	UE_LOG(LogListenServerNetwork, Warning, TEXT("Rejected operation %d: %s"), static_cast<int32>(RequestedOperation), *InternalError);
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnOperationCompleted.Broadcast(RequestedOperation, LastResult);
	}
	return false;
}

bool FListenServerSessionSubsystemImpl::BeginOperation(EListenServerOperation NewOperation)
{
	if (Operation != EListenServerOperation::None)
	{
		return Reject(NewOperation, EListenServerError::AlreadyBusy, TEXT("Another network operation is already running."), FString::Printf(TEXT("Current operation is %d"), static_cast<int32>(Operation)));
	}
	ActiveOperationId = ListenServerNetworkPolicy::AdvanceOperationId(OperationCounter);
	bOperationCancelled = false;
	Purpose = EListenServerAsyncPurpose::None;
	SetOperation(NewOperation, TEXT("operation started"));
	return true;
}

void FListenServerSessionSubsystemImpl::SetOperation(EListenServerOperation NewOperation, const TCHAR* Reason)
{
	const EListenServerOperation Previous = Operation;
	const bool bValid = ListenServerNetworkPolicy::IsValidOperationTransition(Previous, NewOperation);
	ensureMsgf(bValid, TEXT("Invalid ListenServerNetwork operation transition %d -> %d"), static_cast<int32>(Previous), static_cast<int32>(NewOperation));
	if (!bValid)
	{
		UE_LOG(LogListenServerNetwork, Error, TEXT("Rejected internal operation transition %d -> %d"), static_cast<int32>(Previous), static_cast<int32>(NewOperation));
		return;
	}
	Operation = NewOperation;
	UE_LOG(LogListenServerNetwork, Log, TEXT("Operation %d -> %d, Id=%llu, Reason=%s"), static_cast<int32>(Previous), static_cast<int32>(NewOperation), ActiveOperationId, Reason);
	BroadcastState();
}

void FListenServerSessionSubsystemImpl::SetRoleAndConnection(EListenServerRole NewRole, EListenServerConnectionState NewConnection, const TCHAR* Reason)
{
	if (Role == NewRole && ConnectionState == NewConnection)
	{
		return;
	}
	UE_LOG(LogListenServerNetwork, Log, TEXT("Role/connection %d/%d -> %d/%d, Reason=%s"), static_cast<int32>(Role), static_cast<int32>(ConnectionState), static_cast<int32>(NewRole), static_cast<int32>(NewConnection), Reason);
	Role = NewRole;
	ConnectionState = NewConnection;
	if (ConnectionState == EListenServerConnectionState::Offline)
	{
		ClearParticipants();
	}
	else
	{
		RefreshParticipants();
	}
	RefreshConnectionDiagnostics();
	BroadcastState();
}

void FListenServerSessionSubsystemImpl::BroadcastState() const
{
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnStateChanged.Broadcast(Role, ConnectionState, Operation);
	}
}

bool FListenServerSessionSubsystemImpl::IsExpectedCallback(uint64 CallbackOperationId, EListenServerOperation ExpectedOperation, const TCHAR* CallbackName) const
{
	const bool bExpected = !ListenServerNetworkPolicy::IsStaleOperation(CallbackOperationId, ActiveOperationId) && (Operation == ExpectedOperation || Operation == EListenServerOperation::Recovering);
	if (!bExpected)
	{
		UE_LOG(LogListenServerNetwork, Warning, TEXT("Ignored stale %s callback. CallbackId=%llu ActiveId=%llu Operation=%d Expected=%d"), CallbackName, CallbackOperationId, ActiveOperationId, static_cast<int32>(Operation), static_cast<int32>(ExpectedOperation));
	}
	return bExpected;
}

void FListenServerSessionSubsystemImpl::FinishSuccess(const TCHAR* UserMessage, const FString& InternalMessage, bool bRecoverable)
{
	const EListenServerOperation CompletedOperation = Operation;
	ClearTimeout();
	ClearOperationDelegates();
	LastResult = MakeResult(true, EListenServerError::None, UserMessage, InternalMessage, bRecoverable);
	bOperationCancelled = false;
	bQuickMatchActive = false;
	bFailureRecoveryActive = false;
	Purpose = EListenServerAsyncPurpose::None;
	SetOperation(EListenServerOperation::None, TEXT("operation succeeded"));
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnOperationCompleted.Broadcast(CompletedOperation, LastResult);
	}
	ProcessPendingInvite();
}

void FListenServerSessionSubsystemImpl::FinishFailure(EListenServerError Error, const TCHAR* UserMessage, const FString& InternalMessage, bool bRecoverable)
{
	const EListenServerOperation CompletedOperation = Operation;
	ClearTimeout();
	ClearOperationDelegates();
	LastResult = MakeResult(false, Error, UserMessage, InternalMessage, bRecoverable);
	bOperationCancelled = false;
	bQuickMatchActive = false;
	bFailureRecoveryActive = false;
	Purpose = EListenServerAsyncPurpose::None;
	SetOperation(EListenServerOperation::None, TEXT("operation failed"));
	UE_LOG(LogListenServerNetwork, Warning, TEXT("Operation %d failed: Error=%d Internal=%s"), static_cast<int32>(CompletedOperation), static_cast<int32>(Error), *InternalMessage);
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnOperationCompleted.Broadcast(CompletedOperation, LastResult);
	}
	ProcessPendingInvite();
}

void FListenServerSessionSubsystemImpl::StartTimeout(float Seconds, EListenServerOperation TimedOperation)
{
	ClearTimeout();
	const uint64 TimedOperationId = ActiveOperationId;
	TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
	TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakOwner, TimedOperationId, TimedOperation](float)
	{
		if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
		{
			Subsystem->Impl->TimeoutHandle.Reset();
			Subsystem->Impl->HandleTimeout(TimedOperationId, TimedOperation);
		}
		return false;
	}), FMath::Max(1.0f, Seconds));
	UE_LOG(LogListenServerNetwork, Verbose, TEXT("Timeout registered. Operation=%d Id=%llu Seconds=%.1f"), static_cast<int32>(TimedOperation), TimedOperationId, Seconds);
}

void FListenServerSessionSubsystemImpl::ClearTimeout()
{
	if (TimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
		TimeoutHandle.Reset();
		UE_LOG(LogListenServerNetwork, Verbose, TEXT("Timeout removed"));
	}
}

void FListenServerSessionSubsystemImpl::HandleTimeout(uint64 TimedOperationId, EListenServerOperation TimedOperation)
{
	if (ListenServerNetworkPolicy::IsStaleOperation(TimedOperationId, ActiveOperationId))
	{
		UE_LOG(LogListenServerNetwork, Verbose, TEXT("Ignored stale timeout. TimeoutId=%llu ActiveId=%llu"), TimedOperationId, ActiveOperationId);
		return;
	}
	UE_LOG(LogListenServerNetwork, Error, TEXT("Operation timed out. Operation=%d Id=%llu"), static_cast<int32>(TimedOperation), TimedOperationId);
	ClearOperationDelegates();
	if (PendingTravel.IsValid() && PendingTravel->bReturnToMenu)
	{
		PendingTravel.Reset();
		CompleteLocalCleanup();
		if (bFailureRecoveryActive)
		{
			FinishFailure(RecoveryError, *RecoveryUserMessage, RecoveryInternalMessage + TEXT("; return-to-menu travel also timed out"));
		}
		else
		{
			FinishFailure(EListenServerError::TravelTimeout, TEXT("The session was left, but returning to the menu timed out."), TEXT("Return-to-menu PostLoadMap was not observed"));
		}
		return;
	}
	if (bFailureRecoveryActive)
	{
		IOnlineSessionPtr Sessions = GetSessionInterface();
		if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr)
		{
			Sessions->RemoveNamedSession(NAME_GameSession);
		}
		CompleteLocalCleanup();
		ReturnToMainMenu(TimedOperationId, true);
		return;
	}
	if (TimedOperation == EListenServerOperation::Traveling || Operation == EListenServerOperation::Traveling)
	{
		BeginRecovery(EListenServerError::TravelTimeout, TEXT("Map travel timed out."), TEXT("Travel completion delegate did not arrive before timeout"));
		return;
	}
	if (TimedOperation == EListenServerOperation::Creating || TimedOperation == EListenServerOperation::Joining)
	{
		CleanupStaleCreatedOrJoinedSession(TEXT("operation timeout"));
	}
	if (TimedOperation == EListenServerOperation::Searching || TimedOperation == EListenServerOperation::CancellingSearch)
	{
		ActiveSearch.Reset();
		SearchResults.Reset();
		RawSearchResults.Reset();
		if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnSearchResultsChanged.Broadcast();
		}
	}
	if (Purpose == EListenServerAsyncPurpose::LeaveEnd)
	{
		bLeaveEndFailed = true;
		StartDestroy(TimedOperationId, EListenServerAsyncPurpose::LeaveDestroy);
		return;
	}
	if (Purpose == EListenServerAsyncPurpose::LeaveDestroy)
	{
		IOnlineSessionPtr Sessions = GetSessionInterface();
		if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr)
		{
			Sessions->RemoveNamedSession(NAME_GameSession);
		}
		CompleteLocalCleanup();
		ReturnToMainMenu(TimedOperationId, false);
		return;
	}
	if (Operation == EListenServerOperation::Leaving || Operation == EListenServerOperation::Destroying || Operation == EListenServerOperation::Ending)
	{
		CompleteLocalCleanup();
	}
	FinishFailure(EListenServerError::Timeout, TEXT("The Steam session operation timed out."), FString::Printf(TEXT("Operation %d timed out"), static_cast<int32>(TimedOperation)));
}

bool FListenServerSessionSubsystemImpl::Host(const FListenServerHostRequest& Request)
{
	FString MapPackage;
	FString AttributeError;
	if (Request.MaxPlayers < 1 || Request.SessionDisplayName.Len() > ListenServerNetworkPolicy::MaxDisplayStringLength || !IsMapConfiguredAndPresent(Request.LobbyMap, MapPackage))
	{
		return Reject(EListenServerOperation::Creating, EListenServerError::InvalidRequest, TEXT("The host request is invalid."), TEXT("MaxPlayers, display name, or LobbyMap is invalid"));
	}
	if (!ListenServerNetworkPolicy::ValidateAttributes(Request.ExtraAdvertisedAttributes, true, AttributeError))
	{
		return Reject(EListenServerOperation::Creating, EListenServerError::InvalidRequest, TEXT("The advertised session attributes are invalid."), AttributeError);
	}
	if (!ValidateOnlineAccess(EListenServerOperation::Creating) || !BeginOperation(EListenServerOperation::Creating))
	{
		return false;
	}

	ActiveHostRequest = Request;
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions->GetNamedSession(NAME_GameSession) != nullptr)
	{
		StartDestroy(ActiveOperationId, EListenServerAsyncPurpose::HostReplace);
	}
	else
	{
		StartCreate(ActiveOperationId);
	}
	return true;
}

void FListenServerSessionSubsystemImpl::StartCreate(uint64 OperationId)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		FinishFailure(EListenServerError::SessionInterfaceUnavailable, TEXT("Steam sessions became unavailable."), TEXT("Session interface invalid before CreateSession"));
		return;
	}
	SetOperation(EListenServerOperation::Creating, TEXT("creating lobby session"));
	Purpose = bQuickMatchActive ? EListenServerAsyncPurpose::HostCreate : EListenServerAsyncPurpose::HostCreate;

	const UListenServerNetworkSettings* Settings = GetSettings();
	FOnlineSessionSettings SessionSettings;
	SessionSettings.bIsLANMatch = false;
	SessionSettings.NumPublicConnections = ActiveHostRequest.MaxPlayers;
	SessionSettings.NumPrivateConnections = 0;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowJoinInProgress = Settings->bAllowJoinInProgress;
	SessionSettings.bAllowInvites = true;
	SessionSettings.bUsesPresence = true;
	SessionSettings.bUseLobbiesIfAvailable = true;
	SessionSettings.bAllowJoinViaPresence = true;
	SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;
	SessionSettings.Set(ListenServerNetworkKeys::ProjectKey, Settings->ProjectKey, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(ListenServerNetworkKeys::BuildVersion, FString::FromInt(Settings->BuildUniqueId), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(ListenServerNetworkKeys::GameMode, ActiveHostRequest.GameModeId.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(ListenServerNetworkKeys::MapId, ActiveHostRequest.LobbyMap.GetAssetName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(ListenServerNetworkKeys::Region, ActiveHostRequest.Region.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(ListenServerNetworkKeys::LobbyState, FString(TEXT("Lobby")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(ListenServerNetworkKeys::Joinable, FString(TEXT("1")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(ListenServerNetworkKeys::SessionDisplayName, ActiveHostRequest.SessionDisplayName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	for (const FListenServerSessionAttribute& Attribute : ActiveHostRequest.ExtraAdvertisedAttributes)
	{
		SessionSettings.Set(Attribute.Key, Attribute.Value, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}

	TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateLambda([WeakOwner, OperationId](FName SessionName, bool bWasSuccessful)
	{
		if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
		{
			Subsystem->Impl->HandleCreateComplete(OperationId, SessionName, bWasSuccessful);
		}
	}));
	StartTimeout(Settings->CreateTimeoutSeconds, EListenServerOperation::Creating);
	UE_LOG(LogListenServerNetwork, Log, TEXT("CreateSession started. Id=%llu Players=%d Build=%d"), OperationId, ActiveHostRequest.MaxPlayers, Settings->BuildUniqueId);
	if (!Sessions->CreateSession(0, NAME_GameSession, SessionSettings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		CreateHandle.Reset();
		ClearTimeout();
		FinishFailure(EListenServerError::CreateFailed, TEXT("Steam rejected the create-session request."), TEXT("CreateSession returned false"));
	}
}

void FListenServerSessionSubsystemImpl::HandleCreateComplete(uint64 CallbackOperationId, FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && CreateHandle.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}
	CreateHandle.Reset();
	ClearTimeout();
	if (!IsExpectedCallback(CallbackOperationId, EListenServerOperation::Creating, TEXT("CreateSession")))
	{
		if (bWasSuccessful && SessionName == NAME_GameSession)
		{
			CleanupStaleCreatedOrJoinedSession(TEXT("late CreateSession success"));
		}
		return;
	}
	UE_LOG(LogListenServerNetwork, Log, TEXT("CreateSession completed. Id=%llu Success=%d"), CallbackOperationId, bWasSuccessful);
	if (bOperationCancelled)
	{
		if (bWasSuccessful)
		{
			CleanupStaleCreatedOrJoinedSession(TEXT("cancelled CreateSession success"));
		}
		FinishFailure(EListenServerError::Cancelled, TEXT("Session creation was cancelled."), TEXT("Create completion arrived after cancellation"));
		return;
	}
	if (!bWasSuccessful)
	{
		FinishFailure(EListenServerError::CreateFailed, TEXT("Steam could not create the lobby."), TEXT("CreateSession completion reported failure"));
		return;
	}
	if (!BeginTravel(CallbackOperationId, ActiveHostRequest.LobbyMap, EListenServerRole::Host, EListenServerConnectionState::Lobby, true, false))
	{
		CleanupStaleCreatedOrJoinedSession(TEXT("listen travel request failed"));
		FinishFailure(EListenServerError::TravelFailed, TEXT("The lobby map could not be opened."), TEXT("Listen OpenLevel validation or dispatch failed"));
	}
}

void FListenServerSessionSubsystemImpl::StartDestroy(uint64 OperationId, EListenServerAsyncPurpose DestroyPurpose)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		if (DestroyPurpose == EListenServerAsyncPurpose::LeaveDestroy || DestroyPurpose == EListenServerAsyncPurpose::RecoveryDestroy)
		{
			CompleteLocalCleanup();
			ReturnToMainMenu(OperationId, DestroyPurpose == EListenServerAsyncPurpose::RecoveryDestroy);
			return;
		}
		FinishFailure(EListenServerError::SessionInterfaceUnavailable, TEXT("Steam sessions became unavailable."), TEXT("Session interface invalid before DestroySession"));
		return;
	}
	Purpose = DestroyPurpose;
	const EListenServerOperation DestroyOperation = (DestroyPurpose == EListenServerAsyncPurpose::HostReplace || DestroyPurpose == EListenServerAsyncPurpose::InviteDestroy)
		? EListenServerOperation::DestroyingExistingSession : EListenServerOperation::Destroying;
	SetOperation(DestroyOperation, TEXT("destroying named session"));
	TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateLambda([WeakOwner, OperationId](FName SessionName, bool bWasSuccessful)
	{
		if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
		{
			Subsystem->Impl->HandleDestroyComplete(OperationId, SessionName, bWasSuccessful);
		}
	}));
	StartTimeout(GetSettings()->DestroyTimeoutSeconds, DestroyOperation);
	UE_LOG(LogListenServerNetwork, Log, TEXT("DestroySession started. Id=%llu Purpose=%d"), OperationId, static_cast<int32>(DestroyPurpose));
	if (!Sessions->DestroySession(NAME_GameSession))
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		DestroyHandle.Reset();
		ClearTimeout();
		HandleDestroyComplete(OperationId, NAME_GameSession, false);
	}
}

void FListenServerSessionSubsystemImpl::HandleDestroyComplete(uint64 CallbackOperationId, FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && DestroyHandle.IsValid())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	DestroyHandle.Reset();
	ClearTimeout();
	const bool bExpectedOperation = Operation == EListenServerOperation::Destroying || Operation == EListenServerOperation::DestroyingExistingSession || Operation == EListenServerOperation::Recovering;
	if (ListenServerNetworkPolicy::IsStaleOperation(CallbackOperationId, ActiveOperationId) || !bExpectedOperation)
	{
		UE_LOG(LogListenServerNetwork, Warning, TEXT("Ignored stale DestroySession callback. Id=%llu Active=%llu State=%d"), CallbackOperationId, ActiveOperationId, Sessions.IsValid() ? static_cast<int32>(Sessions->GetSessionState(NAME_GameSession)) : -1);
		return;
	}
	UE_LOG(LogListenServerNetwork, Log, TEXT("DestroySession completed. Id=%llu Success=%d Purpose=%d"), CallbackOperationId, bWasSuccessful, static_cast<int32>(Purpose));

	const EListenServerAsyncPurpose CompletedPurpose = Purpose;
	if (bOperationCancelled)
	{
		if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr)
		{
			Sessions->RemoveNamedSession(NAME_GameSession);
		}
		FinishFailure(EListenServerError::Cancelled, TEXT("The session operation was cancelled."), TEXT("Destroy completion arrived after cancellation"));
		return;
	}
	if (!bWasSuccessful && Sessions.IsValid())
	{
		Sessions->RemoveNamedSession(NAME_GameSession);
	}

	switch (CompletedPurpose)
	{
	case EListenServerAsyncPurpose::HostReplace:
		if (bWasSuccessful)
		{
			StartCreate(CallbackOperationId);
		}
		else
		{
			FinishFailure(EListenServerError::DestroyFailed, TEXT("The existing Steam session could not be replaced."), TEXT("DestroySession failed before host create"));
		}
		break;
	case EListenServerAsyncPurpose::InviteDestroy:
		if (!bWasSuccessful)
		{
			FinishFailure(EListenServerError::DestroyFailed, TEXT("The current session could not be left for the invite."), TEXT("DestroySession failed before invite join"));
		}
		else if (PendingInvite.IsValid())
		{
			FOnlineSessionSearchResult InviteCopy = *PendingInvite;
			PendingInvite.Reset();
			PendingInviteSessionId.Reset();
			StartJoinRaw(CallbackOperationId, InviteCopy, EListenServerAsyncPurpose::InviteJoin);
		}
		else
		{
			FinishFailure(EListenServerError::InviteFailed, TEXT("The pending Steam invite was lost."), TEXT("No PendingInvite after DestroySession"));
		}
		break;
	case EListenServerAsyncPurpose::LeaveDestroy:
		CompleteLocalCleanup();
		ReturnToMainMenu(CallbackOperationId, false);
		break;
	case EListenServerAsyncPurpose::RecoveryDestroy:
		CompleteLocalCleanup();
		ReturnToMainMenu(CallbackOperationId, true);
		break;
	default:
		if (bWasSuccessful)
		{
			CompleteLocalCleanup();
			FinishSuccess(TEXT("The Steam session was destroyed."));
		}
		else
		{
			FinishFailure(EListenServerError::DestroyFailed, TEXT("The Steam session could not be destroyed."), TEXT("DestroySession completion reported failure"));
		}
		break;
	}
}

void FListenServerSessionSubsystemImpl::CleanupStaleCreatedOrJoinedSession(const TCHAR* Reason)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || Sessions->GetNamedSession(NAME_GameSession) == nullptr)
	{
		return;
	}
	UE_LOG(LogListenServerNetwork, Warning, TEXT("Cleaning stale named session: %s"), Reason);
	if (!Sessions->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateLambda([](FName, bool bWasSuccessful)
	{
		UE_LOG(LogListenServerNetwork, Verbose, TEXT("Stale session cleanup completed. Success=%d"), bWasSuccessful);
	})))
	{
		Sessions->RemoveNamedSession(NAME_GameSession);
	}
}

bool FListenServerSessionSubsystemImpl::Find(const FListenServerSearchRequest& Request, bool bForQuickMatch)
{
	FString AttributeError;
	if (Request.MaxSearchResults < 1 || !ListenServerNetworkPolicy::ValidateAttributes(Request.RequiredAttributes, false, AttributeError))
	{
		return Reject(EListenServerOperation::Searching, EListenServerError::InvalidRequest, TEXT("The session search request is invalid."), AttributeError.IsEmpty() ? TEXT("MaxSearchResults must be positive") : AttributeError);
	}
	if (!ValidateOnlineAccess(EListenServerOperation::Searching) || !BeginOperation(EListenServerOperation::Searching))
	{
		return false;
	}
	ActiveSearchRequest = Request;
	StartFind(ActiveOperationId, bForQuickMatch);
	return true;
}

void FListenServerSessionSubsystemImpl::StartFind(uint64 OperationId, bool bForQuickMatch)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		FinishFailure(EListenServerError::SessionInterfaceUnavailable, TEXT("Steam sessions became unavailable."), TEXT("Session interface invalid before FindSessions"));
		return;
	}
	SetOperation(EListenServerOperation::Searching, TEXT("searching Steam lobbies"));
	Purpose = bForQuickMatch ? EListenServerAsyncPurpose::QuickFind : EListenServerAsyncPurpose::Find;
	if (SearchGeneration == MAX_int32)
	{
		SearchGeneration = 0;
	}
	else
	{
		++SearchGeneration;
	}
	SearchResults.Reset();
	RawSearchResults.Reset();
	ActiveSearch = MakeShared<FOnlineSessionSearch>();
	ActiveSearch->bIsLanQuery = false;
	ActiveSearch->MaxSearchResults = ActiveSearchRequest.MaxSearchResults;
	ActiveSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	for (const FListenServerSessionAttribute& Required : ActiveSearchRequest.RequiredAttributes)
	{
		ActiveSearch->QuerySettings.Set(Required.Key, Required.Value, EOnlineComparisonOp::Equals);
	}
	const UListenServerNetworkSettings* Settings = GetSettings();
	ActiveSearch->QuerySettings.Set(ListenServerNetworkKeys::ProjectKey, Settings->ProjectKey, EOnlineComparisonOp::Equals);
	ActiveSearch->QuerySettings.Set(ListenServerNetworkKeys::BuildVersion, FString::FromInt(Settings->BuildUniqueId), EOnlineComparisonOp::Equals);
	if (!ActiveSearchRequest.GameModeId.IsNone())
	{
		ActiveSearch->QuerySettings.Set(ListenServerNetworkKeys::GameMode, ActiveSearchRequest.GameModeId.ToString(), EOnlineComparisonOp::Equals);
	}
	if (!ActiveSearchRequest.Region.IsNone())
	{
		ActiveSearch->QuerySettings.Set(ListenServerNetworkKeys::Region, ActiveSearchRequest.Region.ToString(), EOnlineComparisonOp::Equals);
	}
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnSearchResultsChanged.Broadcast();
	}

	TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateLambda([WeakOwner, OperationId](bool bWasSuccessful)
	{
		if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
		{
			Subsystem->Impl->HandleFindComplete(OperationId, bWasSuccessful);
		}
	}));
	StartTimeout(GetSettings()->SearchTimeoutSeconds, EListenServerOperation::Searching);
	UE_LOG(LogListenServerNetwork, Log, TEXT("FindSessions started. Id=%llu Generation=%d MaxResults=%d Quick=%d Project=%s Build=%d Filters=%d"), OperationId, SearchGeneration, ActiveSearchRequest.MaxSearchResults, bForQuickMatch, *Settings->ProjectKey, Settings->BuildUniqueId, ActiveSearch->QuerySettings.SearchParams.Num());
	if (!Sessions->FindSessions(0, ActiveSearch.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		FindHandle.Reset();
		ClearTimeout();
		FinishFailure(EListenServerError::SearchFailed, TEXT("Steam rejected the session search."), TEXT("FindSessions returned false"));
	}
}

void FListenServerSessionSubsystemImpl::HandleFindComplete(uint64 CallbackOperationId, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && FindHandle.IsValid())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}
	FindHandle.Reset();
	ClearTimeout();
	if (!IsExpectedCallback(CallbackOperationId, Operation == EListenServerOperation::CancellingSearch ? EListenServerOperation::CancellingSearch : EListenServerOperation::Searching, TEXT("FindSessions")))
	{
		ActiveSearch.Reset();
		return;
	}
	if (bOperationCancelled || Operation == EListenServerOperation::CancellingSearch)
	{
		SearchResults.Reset();
		RawSearchResults.Reset();
		ActiveSearch.Reset();
		if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnSearchResultsChanged.Broadcast();
		}
		FinishFailure(EListenServerError::SearchCancelled, TEXT("The session search was cancelled."), TEXT("Find completion arrived after cancellation"));
		return;
	}
	UE_LOG(LogListenServerNetwork, Log, TEXT("FindSessions completed. Id=%llu Success=%d Raw=%d"), CallbackOperationId, bWasSuccessful, ActiveSearch.IsValid() ? ActiveSearch->SearchResults.Num() : 0);
	if (!bWasSuccessful || !ActiveSearch.IsValid())
	{
		ActiveSearch.Reset();
		FinishFailure(EListenServerError::SearchFailed, TEXT("Steam session search failed."), TEXT("FindSessions completion reported failure"));
		return;
	}

	FilterSearchResults();
	ActiveSearch.Reset();
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnSearchResultsChanged.Broadcast();
	}
	if (bQuickMatchActive)
	{
		ContinueQuickMatchAfterSearch();
		return;
	}
	if (SearchResults.IsEmpty())
	{
		FinishFailure(EListenServerError::NoSessionsFound, TEXT("No compatible Steam lobbies were found."), TEXT("Search succeeded with zero compatible results"));
	}
	else
	{
		FinishSuccess(TEXT("Steam lobby search completed."), FString::Printf(TEXT("Compatible results: %d"), SearchResults.Num()));
	}
}

EListenServerError FListenServerSessionSubsystemImpl::CheckRawCompatibility(const FOnlineSessionSearchResult& RawResult, const FListenServerSearchRequest& Request, bool bRequireOpenSlot) const
{
	if (!RawResult.IsValid())
	{
		return EListenServerError::Unknown;
	}
	const UListenServerNetworkSettings* Settings = GetSettings();
	const FOnlineSessionSettings& RawSettings = RawResult.Session.SessionSettings;
	FString ProjectKey;
	FString BuildVersion;
	FString GameMode;
	FString Region;
	FString MapId;
	FString LobbyState;
	FString Joinable;
	FString DisplayName;
	if (!GetStringSetting(RawSettings, ListenServerNetworkKeys::ProjectKey, ProjectKey)
		|| !GetStringSetting(RawSettings, ListenServerNetworkKeys::BuildVersion, BuildVersion)
		|| !GetStringSetting(RawSettings, ListenServerNetworkKeys::GameMode, GameMode)
		|| !GetStringSetting(RawSettings, ListenServerNetworkKeys::Region, Region)
		|| !GetStringSetting(RawSettings, ListenServerNetworkKeys::MapId, MapId)
		|| !GetStringSetting(RawSettings, ListenServerNetworkKeys::LobbyState, LobbyState)
		|| !GetStringSetting(RawSettings, ListenServerNetworkKeys::SessionDisplayName, DisplayName))
	{
		return EListenServerError::Unknown;
	}
	if (ProjectKey.Len() > ListenServerNetworkPolicy::MaxAttributeValueLength
		|| BuildVersion.Len() > 16
		|| GameMode.Len() > ListenServerNetworkPolicy::MaxAttributeValueLength
		|| Region.Len() > ListenServerNetworkPolicy::MaxAttributeValueLength
		|| MapId.IsEmpty() || MapId.Len() > ListenServerNetworkPolicy::MaxAttributeValueLength
		|| LobbyState.IsEmpty() || LobbyState.Len() > ListenServerNetworkPolicy::MaxAttributeValueLength
		|| DisplayName.Len() > ListenServerNetworkPolicy::MaxDisplayStringLength)
	{
		return EListenServerError::Unknown;
	}
	const bool bHasJoinable = GetStringSetting(RawSettings, ListenServerNetworkKeys::Joinable, Joinable);
	if (bHasJoinable && Joinable != TEXT("0") && Joinable != TEXT("1"))
	{
		return EListenServerError::Unknown;
	}
	int32 AdvertisedBuildUniqueId = 0;
	if (!LexTryParseString(AdvertisedBuildUniqueId, *BuildVersion))
	{
		return EListenServerError::IncompatibleBuild;
	}

	ListenServerNetworkPolicy::FCompatibilityCandidate Candidate;
	Candidate.ProjectKey = ProjectKey;
	// Steam OSS owns RawSettings.BuildUniqueId; plugin compatibility uses the advertised BUILD_VERSION.
	Candidate.BuildUniqueId = AdvertisedBuildUniqueId;
	Candidate.GameMode = FName(*GameMode);
	Candidate.Region = FName(*Region);
	Candidate.OpenPublicConnections = bRequireOpenSlot ? RawResult.Session.NumOpenPublicConnections : 1;
	Candidate.bJoinable = !bHasJoinable || Joinable == TEXT("1");
	for (const FListenServerSessionAttribute& Required : Request.RequiredAttributes)
	{
		FString Value;
		if (GetStringSetting(RawSettings, Required.Key, Value) && Value.Len() <= ListenServerNetworkPolicy::MaxAttributeValueLength)
		{
			Candidate.Attributes.Add(Required.Key, Value);
		}
	}

	ListenServerNetworkPolicy::FCompatibilityRequest CompatibilityRequest;
	CompatibilityRequest.ProjectKey = Settings->ProjectKey;
	CompatibilityRequest.BuildUniqueId = Settings->BuildUniqueId;
	CompatibilityRequest.GameMode = Request.GameModeId;
	CompatibilityRequest.Region = Request.Region;
	CompatibilityRequest.RequiredAttributes = Request.RequiredAttributes;
	const EListenServerError Compatibility = ListenServerNetworkPolicy::CheckCompatibility(Candidate, CompatibilityRequest);
	if (Compatibility != EListenServerError::None)
	{
		return Compatibility;
	}
	return EListenServerError::None;
}

void FListenServerSessionSubsystemImpl::FilterSearchResults()
{
	SearchResults.Reset();
	RawSearchResults.Reset();
	if (!ActiveSearch.IsValid())
	{
		return;
	}
	SearchResults.Reserve(ActiveSearch->SearchResults.Num());
	RawSearchResults.Reserve(ActiveSearch->SearchResults.Num());
	TSet<FString> SeenSessionIds;
	SeenSessionIds.Reserve(ActiveSearch->SearchResults.Num());
	TMap<EListenServerError, int32> ExclusionCounts;
	int32 DuplicateCount = 0;

	for (const FOnlineSessionSearchResult& RawResult : ActiveSearch->SearchResults)
	{
		const EListenServerError Compatibility = CheckRawCompatibility(RawResult, ActiveSearchRequest, true);
		if (Compatibility != EListenServerError::None)
		{
			++ExclusionCounts.FindOrAdd(Compatibility);
			continue;
		}
		const FString SessionId = RawResult.GetSessionIdStr();
		if (SessionId.IsEmpty() || SeenSessionIds.Contains(SessionId))
		{
			++DuplicateCount;
			continue;
		}
		SeenSessionIds.Add(SessionId);
		const int32 PublicIndex = SearchResults.Num();
		RawSearchResults.Add(RawResult);
		FListenServerSearchResult& PublicResult = SearchResults.AddDefaulted_GetRef();
		FillPublicSearchResult(RawResult, PublicIndex, PublicResult);
	}

	UE_LOG(LogListenServerNetwork, Log, TEXT("Search filtered. Raw=%d Compatible=%d Duplicates=%d"), ActiveSearch->SearchResults.Num(), SearchResults.Num(), DuplicateCount);
	for (const TPair<EListenServerError, int32>& Pair : ExclusionCounts)
	{
		UE_LOG(LogListenServerNetwork, Verbose, TEXT("Search exclusions. Reason=%d Count=%d"), static_cast<int32>(Pair.Key), Pair.Value);
	}
}

void FListenServerSessionSubsystemImpl::FillPublicSearchResult(const FOnlineSessionSearchResult& RawResult, int32 PublicIndex, FListenServerSearchResult& OutResult) const
{
	const FOnlineSessionSettings& Settings = RawResult.Session.SessionSettings;
	OutResult.Handle.SearchGeneration = SearchGeneration;
	OutResult.Handle.ResultIndex = PublicIndex;
	OutResult.SessionId = ListenServerNetworkPolicy::SanitizeExternalString(RawResult.GetSessionIdStr(), ListenServerNetworkPolicy::MaxDisplayStringLength);
	OutResult.OwningUserName = ListenServerNetworkPolicy::SanitizeExternalString(RawResult.Session.OwningUserName, ListenServerNetworkPolicy::MaxDisplayStringLength);
	FString Value;
	if (GetStringSetting(Settings, ListenServerNetworkKeys::SessionDisplayName, Value)) OutResult.SessionDisplayName = ListenServerNetworkPolicy::SanitizeExternalString(Value, ListenServerNetworkPolicy::MaxDisplayStringLength);
	if (GetStringSetting(Settings, ListenServerNetworkKeys::GameMode, Value)) OutResult.GameModeId = FName(*ListenServerNetworkPolicy::SanitizeExternalString(Value, ListenServerNetworkPolicy::MaxAttributeKeyLength));
	if (GetStringSetting(Settings, ListenServerNetworkKeys::MapId, Value)) OutResult.MapId = FName(*ListenServerNetworkPolicy::SanitizeExternalString(Value, ListenServerNetworkPolicy::MaxAttributeKeyLength));
	if (GetStringSetting(Settings, ListenServerNetworkKeys::Region, Value)) OutResult.Region = FName(*ListenServerNetworkPolicy::SanitizeExternalString(Value, ListenServerNetworkPolicy::MaxAttributeKeyLength));
	if (GetStringSetting(Settings, ListenServerNetworkKeys::LobbyState, Value)) OutResult.LobbyState = FName(*ListenServerNetworkPolicy::SanitizeExternalString(Value, ListenServerNetworkPolicy::MaxAttributeKeyLength));
	OutResult.bIsJoinable = !GetStringSetting(Settings, ListenServerNetworkKeys::Joinable, Value) || Value == TEXT("1");
	OutResult.MaxPlayers = Settings.NumPublicConnections;
	OutResult.CurrentPlayers = FMath::Clamp(Settings.NumPublicConnections - RawResult.Session.NumOpenPublicConnections, 0, Settings.NumPublicConnections);
	OutResult.PingMilliseconds = RawResult.PingInMs >= 0 ? RawResult.PingInMs : INDEX_NONE;
	OutResult.AdvertisedAttributes.Reserve(Settings.Settings.Num());
	for (const TPair<FName, FOnlineSessionSetting>& Pair : Settings.Settings)
	{
		if (ListenServerNetworkPolicy::IsReservedKey(Pair.Key))
		{
			continue;
		}
		FString AttributeValue;
		if (Pair.Value.Data.GetType() == EOnlineKeyValuePairDataType::String)
		{
			Pair.Value.Data.GetValue(AttributeValue);
			FListenServerSessionAttribute& Attribute = OutResult.AdvertisedAttributes.AddDefaulted_GetRef();
			Attribute.Key = Pair.Key;
			Attribute.Value = ListenServerNetworkPolicy::SanitizeExternalString(AttributeValue);
		}
	}
}

bool FListenServerSessionSubsystemImpl::Join(const FListenServerSearchResultHandle& ResultHandle)
{
	if (!ListenServerNetworkPolicy::IsSearchHandleValid(ResultHandle, SearchGeneration, RawSearchResults.Num()))
	{
		return Reject(EListenServerOperation::Joining, EListenServerError::InvalidSearchHandle, TEXT("That search result is no longer valid."), TEXT("Search generation or result index mismatch"));
	}
	if (Role != EListenServerRole::None || ConnectionState != EListenServerConnectionState::Offline)
	{
		return Reject(EListenServerOperation::Joining, EListenServerError::InvalidState, TEXT("Leave the current session before joining another one."), TEXT("Join requested while already connected"));
	}
	const FOnlineSessionSearchResult& RawResult = RawSearchResults[ResultHandle.ResultIndex];
	const EListenServerError Compatibility = CheckRawCompatibility(RawResult, ActiveSearchRequest, true);
	if (Compatibility != EListenServerError::None)
	{
		return Reject(EListenServerOperation::Joining, Compatibility, TEXT("That Steam lobby is no longer compatible."), FString::Printf(TEXT("Compatibility check failed with %d"), static_cast<int32>(Compatibility)));
	}
	if (!ValidateOnlineAccess(EListenServerOperation::Joining) || !BeginOperation(EListenServerOperation::Joining))
	{
		return false;
	}
	StartJoinRaw(ActiveOperationId, RawResult, EListenServerAsyncPurpose::Join);
	return true;
}

void FListenServerSessionSubsystemImpl::StartJoinRaw(uint64 OperationId, const FOnlineSessionSearchResult& RawResult, EListenServerAsyncPurpose JoinPurpose)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		FinishFailure(EListenServerError::SessionInterfaceUnavailable, TEXT("Steam sessions became unavailable."), TEXT("Session interface invalid before JoinSession"));
		return;
	}
	SetOperation(EListenServerOperation::Joining, TEXT("joining Steam lobby"));
	Purpose = JoinPurpose;
	FString LobbyState;
	GetStringSetting(RawResult.Session.SessionSettings, ListenServerNetworkKeys::LobbyState, LobbyState);
	PendingJoinConnectionState = LobbyState.Equals(TEXT("InGame"), ESearchCase::IgnoreCase) || LobbyState.Equals(TEXT("Starting"), ESearchCase::IgnoreCase)
		? EListenServerConnectionState::InGame : EListenServerConnectionState::Lobby;

	TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateLambda([WeakOwner, OperationId](FName SessionName, EOnJoinSessionCompleteResult::Type Result)
	{
		if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
		{
			Subsystem->Impl->HandleJoinComplete(OperationId, SessionName, Result);
		}
	}));
	StartTimeout(GetSettings()->JoinTimeoutSeconds, EListenServerOperation::Joining);
	UE_LOG(LogListenServerNetwork, Log, TEXT("JoinSession started. Id=%llu Purpose=%d"), OperationId, static_cast<int32>(JoinPurpose));
	if (!Sessions->JoinSession(0, NAME_GameSession, RawResult))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		JoinHandle.Reset();
		ClearTimeout();
		if (bQuickMatchActive)
		{
			ContinueQuickMatchAfterJoinFailure(EListenServerError::JoinFailed, TEXT("JoinSession returned false"));
		}
		else
		{
			FinishFailure(EListenServerError::JoinFailed, TEXT("Steam rejected the join request."), TEXT("JoinSession returned false"));
		}
	}
}

void FListenServerSessionSubsystemImpl::HandleJoinComplete(uint64 CallbackOperationId, FName SessionName, EOnJoinSessionCompleteResult::Type JoinResult)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && JoinHandle.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}
	JoinHandle.Reset();
	ClearTimeout();
	if (!IsExpectedCallback(CallbackOperationId, EListenServerOperation::Joining, TEXT("JoinSession")))
	{
		if (JoinResult == EOnJoinSessionCompleteResult::Success)
		{
			CleanupStaleCreatedOrJoinedSession(TEXT("late JoinSession success"));
		}
		return;
	}
	UE_LOG(LogListenServerNetwork, Log, TEXT("JoinSession completed. Id=%llu Result=%s"), CallbackOperationId, LexToString(JoinResult));
	if (bOperationCancelled)
	{
		if (JoinResult == EOnJoinSessionCompleteResult::Success)
		{
			CleanupStaleCreatedOrJoinedSession(TEXT("cancelled JoinSession success"));
		}
		FinishFailure(EListenServerError::Cancelled, TEXT("Joining the session was cancelled."), TEXT("Join completion arrived after cancellation"));
		return;
	}
	if (JoinResult != EOnJoinSessionCompleteResult::Success)
	{
		const EListenServerError Error = ListenServerNetworkPolicy::MapJoinFailure(TranslateJoinFailure(JoinResult));
		if (Sessions.IsValid())
		{
			Sessions->RemoveNamedSession(NAME_GameSession);
		}
		if (bQuickMatchActive)
		{
			ContinueQuickMatchAfterJoinFailure(Error, FString::Printf(TEXT("Join result: %s"), LexToString(JoinResult)));
		}
		else
		{
			FinishFailure(Error, TEXT("The Steam lobby could not be joined."), FString::Printf(TEXT("Join result: %s"), LexToString(JoinResult)));
		}
		return;
	}

	FString ConnectString;
	if (!Sessions.IsValid() || !Sessions->GetResolvedConnectString(NAME_GameSession, ConnectString) || ConnectString.IsEmpty())
	{
		CleanupStaleCreatedOrJoinedSession(TEXT("connect string resolution failed"));
		if (bQuickMatchActive)
		{
			ContinueQuickMatchAfterJoinFailure(EListenServerError::ConnectStringFailed, TEXT("GetResolvedConnectString failed"));
		}
		else
		{
			FinishFailure(EListenServerError::ConnectStringFailed, TEXT("Steam did not provide a server address."), TEXT("GetResolvedConnectString failed"));
		}
		return;
	}
	UE_LOG(LogListenServerNetwork, Log, TEXT("Resolved connect string. Success=1 (value redacted)"));
	if (!BeginClientTravel(CallbackOperationId, ConnectString, PendingJoinConnectionState))
	{
		CleanupStaleCreatedOrJoinedSession(TEXT("ClientTravel dispatch failed"));
		FinishFailure(EListenServerError::TravelFailed, TEXT("The connection travel could not start."), TEXT("Primary PlayerController unavailable or ClientTravel validation failed"));
	}
}

void FListenServerSessionSubsystemImpl::ContinueQuickMatchAfterJoinFailure(EListenServerError Error, const FString& InternalMessage)
{
	UE_LOG(LogListenServerNetwork, Warning, TEXT("Quick Match join candidate %d failed. Error=%d"), QuickJoinIndex, static_cast<int32>(Error));
	++QuickJoinIndex;
	if (RawSearchResults.IsValidIndex(QuickJoinIndex))
	{
		StartJoinRaw(ActiveOperationId, RawSearchResults[QuickJoinIndex], EListenServerAsyncPurpose::QuickJoin);
		return;
	}
	if (QuickSearchAttempt < 1)
	{
		++QuickSearchAttempt;
		ActiveSearchRequest = ActiveQuickMatchRequest.SearchRequest;
		StartFind(ActiveOperationId, true);
		return;
	}
	if (ActiveQuickMatchRequest.bAllowHostFallback)
	{
		ActiveHostRequest = ActiveQuickMatchRequest.HostRequest;
		IOnlineSessionPtr Sessions = GetSessionInterface();
		if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr)
		{
			StartDestroy(ActiveOperationId, EListenServerAsyncPurpose::HostReplace);
		}
		else
		{
			StartCreate(ActiveOperationId);
		}
		return;
	}
	FinishFailure(Error == EListenServerError::None ? EListenServerError::NoSessionsFound : Error, TEXT("Quick Match could not join a compatible lobby."), InternalMessage);
}

void FListenServerSessionSubsystemImpl::ContinueQuickMatchAfterSearch()
{
	if (!SearchResults.IsEmpty())
	{
		QuickJoinIndex = 0;
		UE_LOG(LogListenServerNetwork, Log, TEXT("Quick Match trying %d candidates in Steam result order"), SearchResults.Num());
		StartJoinRaw(ActiveOperationId, RawSearchResults[0], EListenServerAsyncPurpose::QuickJoin);
		return;
	}
	if (QuickSearchAttempt < 1)
	{
		++QuickSearchAttempt;
		StartFind(ActiveOperationId, true);
		return;
	}
	if (ActiveQuickMatchRequest.bAllowHostFallback)
	{
		ActiveHostRequest = ActiveQuickMatchRequest.HostRequest;
		StartCreate(ActiveOperationId);
		return;
	}
	FinishFailure(EListenServerError::NoSessionsFound, TEXT("Quick Match found no compatible Steam lobbies."), TEXT("Two searches completed without a compatible result"));
}

bool FListenServerSessionSubsystemImpl::QuickMatch(const FListenServerQuickMatchRequest& Request)
{
	FString AttributeError;
	FString LobbyPackage;
	if (Request.SearchRequest.MaxSearchResults < 1
		|| !ListenServerNetworkPolicy::ValidateAttributes(Request.SearchRequest.RequiredAttributes, false, AttributeError)
		|| (Request.bAllowHostFallback && (Request.HostRequest.MaxPlayers < 1
			|| !ListenServerNetworkPolicy::ValidateAttributes(Request.HostRequest.ExtraAdvertisedAttributes, true, AttributeError)
			|| !IsMapConfiguredAndPresent(Request.HostRequest.LobbyMap, LobbyPackage))))
	{
		return Reject(EListenServerOperation::Searching, EListenServerError::InvalidRequest, TEXT("The Quick Match request is invalid."), AttributeError.IsEmpty() ? TEXT("Search or host fallback settings are invalid") : AttributeError);
	}
	if (!ValidateOnlineAccess(EListenServerOperation::Searching) || !BeginOperation(EListenServerOperation::Searching))
	{
		return false;
	}
	bQuickMatchActive = true;
	QuickSearchAttempt = 0;
	QuickJoinIndex = INDEX_NONE;
	ActiveQuickMatchRequest = Request;
	ActiveSearchRequest = Request.SearchRequest;
	ActiveHostRequest = Request.HostRequest;
	StartFind(ActiveOperationId, true);
	return true;
}

bool FListenServerSessionSubsystemImpl::Leave()
{
	if (Operation != EListenServerOperation::None)
	{
		return Reject(EListenServerOperation::Leaving, EListenServerError::AlreadyBusy, TEXT("Another network operation is already running."), TEXT("Leave requested while busy"));
	}
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Role == EListenServerRole::None && (!Sessions.IsValid() || Sessions->GetNamedSession(NAME_GameSession) == nullptr))
	{
		return Reject(EListenServerOperation::Leaving, EListenServerError::InvalidState, TEXT("There is no active session to leave."), TEXT("Role is None and no named session exists"));
	}
	if (!BeginOperation(EListenServerOperation::Leaving))
	{
		return false;
	}
	bLeaveEndFailed = false;
	if (!Sessions.IsValid() || Sessions->GetNamedSession(NAME_GameSession) == nullptr)
	{
		CompleteLocalCleanup();
		ReturnToMainMenu(ActiveOperationId, false);
		return true;
	}
	if (Sessions->GetSessionState(NAME_GameSession) == EOnlineSessionState::InProgress)
	{
		StartEndForLeave(ActiveOperationId);
	}
	else
	{
		StartDestroy(ActiveOperationId, EListenServerAsyncPurpose::LeaveDestroy);
	}
	return true;
}

void FListenServerSessionSubsystemImpl::StartEndForLeave(uint64 OperationId)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		bLeaveEndFailed = true;
		StartDestroy(OperationId, EListenServerAsyncPurpose::LeaveDestroy);
		return;
	}
	Purpose = EListenServerAsyncPurpose::LeaveEnd;
	SetOperation(EListenServerOperation::Ending, TEXT("ending active session before leave"));
	TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
	EndHandle = Sessions->AddOnEndSessionCompleteDelegate_Handle(FOnEndSessionCompleteDelegate::CreateLambda([WeakOwner, OperationId](FName SessionName, bool bWasSuccessful)
	{
		if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
		{
			Subsystem->Impl->HandleEndComplete(OperationId, SessionName, bWasSuccessful);
		}
	}));
	StartTimeout(GetSettings()->UpdateTimeoutSeconds, EListenServerOperation::Ending);
	UE_LOG(LogListenServerNetwork, Log, TEXT("EndSession started. Id=%llu"), OperationId);
	if (!Sessions->EndSession(NAME_GameSession))
	{
		Sessions->ClearOnEndSessionCompleteDelegate_Handle(EndHandle);
		EndHandle.Reset();
		ClearTimeout();
		HandleEndComplete(OperationId, NAME_GameSession, false);
	}
}

void FListenServerSessionSubsystemImpl::HandleEndComplete(uint64 CallbackOperationId, FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && EndHandle.IsValid())
	{
		Sessions->ClearOnEndSessionCompleteDelegate_Handle(EndHandle);
	}
	EndHandle.Reset();
	ClearTimeout();
	if (!IsExpectedCallback(CallbackOperationId, EListenServerOperation::Ending, TEXT("EndSession")))
	{
		UE_LOG(LogListenServerNetwork, Verbose, TEXT("Current session state after stale EndSession=%d"), Sessions.IsValid() ? static_cast<int32>(Sessions->GetSessionState(NAME_GameSession)) : -1);
		return;
	}
	bLeaveEndFailed = !bWasSuccessful;
	UE_LOG(LogListenServerNetwork, Log, TEXT("EndSession completed. Id=%llu Success=%d"), CallbackOperationId, bWasSuccessful);
	StartDestroy(CallbackOperationId, EListenServerAsyncPurpose::LeaveDestroy);
}

void FListenServerSessionSubsystemImpl::CompleteLocalCleanup()
{
	SearchResults.Reset();
	RawSearchResults.Reset();
	ActiveSearch.Reset();
	PendingInvite.Reset();
	PendingInviteSessionId.Reset();
	if (SearchGeneration == MAX_int32)
	{
		SearchGeneration = 0;
	}
	else
	{
		++SearchGeneration;
	}
	SetRoleAndConnection(EListenServerRole::None, EListenServerConnectionState::Offline, TEXT("local session state cleared"));
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnSearchResultsChanged.Broadcast();
	}
}

void FListenServerSessionSubsystemImpl::ReturnToMainMenu(uint64 OperationId, bool bFromRecovery)
{
	FString MainMenuPackage;
	if (IsMapConfiguredAndPresent(GetSettings()->MainMenuMap, MainMenuPackage))
	{
		BeginTravel(OperationId, GetSettings()->MainMenuMap, EListenServerRole::None, EListenServerConnectionState::Offline, false, true);
		return;
	}
	PendingTravel.Reset();
	if (bFromRecovery)
	{
		FinishFailure(RecoveryError, *RecoveryUserMessage, RecoveryInternalMessage);
	}
	else
	{
		FinishSuccess(TEXT("The Steam session was left."), bLeaveEndFailed ? TEXT("EndSession failed; local cleanup and DestroySession were still attempted") : FString(), true);
	}
}

bool FListenServerSessionSubsystemImpl::HostTravel(const FSoftObjectPath& Map)
{
	FString MapPackage;
	UWorld* World = GetWorld();
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Role != EListenServerRole::Host || ConnectionState == EListenServerConnectionState::Offline || World == nullptr || World->GetNetMode() == NM_Client)
	{
		return Reject(EListenServerOperation::Updating, EListenServerError::InvalidState, TEXT("Only the listen-server host can start game travel."), TEXT("Role, connection, World, or NetMode authority check failed"));
	}
	if (!IsMapConfiguredAndPresent(Map, MapPackage))
	{
		return Reject(EListenServerOperation::Updating, EListenServerError::InvalidRequest, TEXT("The game map is not configured or does not exist."), Map.ToString());
	}
	if (!Sessions.IsValid() || Sessions->GetNamedSession(NAME_GameSession) == nullptr)
	{
		return Reject(EListenServerOperation::Updating, EListenServerError::SessionNotFound, TEXT("The host Steam session no longer exists."), TEXT("No NAME_GameSession before host game travel"));
	}
	if (!BeginOperation(EListenServerOperation::Updating))
	{
		return false;
	}
	PendingHostGameMap = Map;
	StartHostGameUpdate(ActiveOperationId);
	return true;
}

bool FListenServerSessionSubsystemImpl::UpdateHostedSessionState(
	EListenServerAdvertisedSessionState NewState,
	bool bAllowNewParticipants
)
{
	UWorld* World = GetWorld();
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Role != EListenServerRole::Host
		|| ConnectionState == EListenServerConnectionState::Offline
		|| World == nullptr
		|| World->GetNetMode() == NM_Client)
	{
		return Reject(EListenServerOperation::Updating, EListenServerError::InvalidState, TEXT("Only the listen-server host can update session availability."), TEXT("Role, connection, World, or NetMode authority check failed"));
	}
	if (NewState == EListenServerAdvertisedSessionState::Closed && bAllowNewParticipants)
	{
		return Reject(EListenServerOperation::Updating, EListenServerError::InvalidRequest, TEXT("A closed session cannot allow new participants."), TEXT("Closed state requested with bAllowNewParticipants=true"));
	}
	const bool bStateMatchesWorld =
		(NewState == EListenServerAdvertisedSessionState::Lobby
			&& ConnectionState == EListenServerConnectionState::Lobby)
		|| (NewState == EListenServerAdvertisedSessionState::InGame
			&& ConnectionState == EListenServerConnectionState::InGame)
		|| NewState == EListenServerAdvertisedSessionState::Closed;
	if (!bStateMatchesWorld)
	{
		return Reject(EListenServerOperation::Updating, EListenServerError::InvalidState, TEXT("The advertised session state does not match the current world state."), TEXT("Lobby/InGame advertisement must match ConnectionState"));
	}
	if (!Sessions.IsValid() || Sessions->GetNamedSession(NAME_GameSession) == nullptr)
	{
		return Reject(EListenServerOperation::Updating, EListenServerError::SessionNotFound, TEXT("The host Steam session no longer exists."), TEXT("No NAME_GameSession before session state update"));
	}
	if (!BeginOperation(EListenServerOperation::Updating))
	{
		return false;
	}

	PendingAdvertisedSessionState = NewState;
	bPendingAllowNewParticipants = bAllowNewParticipants;
	StartSessionStateUpdate(ActiveOperationId);
	return true;
}

void FListenServerSessionSubsystemImpl::StartSessionStateUpdate(uint64 OperationId)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	FOnlineSessionSettings* ExistingSettings = Sessions.IsValid()
		? Sessions->GetSessionSettings(NAME_GameSession)
		: nullptr;
	if (ExistingSettings == nullptr)
	{
		FinishFailure(EListenServerError::UpdateFailed, TEXT("The host session settings are unavailable."), TEXT("GetSessionSettings returned null"));
		return;
	}

	SetOperation(EListenServerOperation::Updating, TEXT("updating advertised session state and joinability"));
	Purpose = EListenServerAsyncPurpose::SessionStateUpdate;
	FOnlineSessionSettings UpdatedSettings = *ExistingSettings;
	UpdatedSettings.Set(
		ListenServerNetworkKeys::LobbyState,
		FString(GetAdvertisedSessionStateValue(PendingAdvertisedSessionState)),
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
	);
	ApplySessionJoinability(UpdatedSettings, bPendingAllowNewParticipants);

	TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
	UpdateHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateLambda([WeakOwner, OperationId](FName SessionName, bool bWasSuccessful)
	{
		if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
		{
			Subsystem->Impl->HandleUpdateComplete(OperationId, SessionName, bWasSuccessful);
		}
	}));
	StartTimeout(GetSettings()->UpdateTimeoutSeconds, EListenServerOperation::Updating);
	UE_LOG(LogListenServerNetwork, Log, TEXT("Session state UpdateSession started. Id=%llu State=%d Joinable=%d"), OperationId, static_cast<int32>(PendingAdvertisedSessionState), bPendingAllowNewParticipants);
	if (!Sessions->UpdateSession(NAME_GameSession, UpdatedSettings, true))
	{
		Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateHandle);
		UpdateHandle.Reset();
		ClearTimeout();
		HandleUpdateComplete(OperationId, NAME_GameSession, false);
	}
}

void FListenServerSessionSubsystemImpl::StartHostGameUpdate(uint64 OperationId)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	FOnlineSessionSettings* ExistingSettings = Sessions.IsValid() ? Sessions->GetSessionSettings(NAME_GameSession) : nullptr;
	if (ExistingSettings == nullptr)
	{
		FinishFailure(EListenServerError::UpdateFailed, TEXT("The host session settings are unavailable."), TEXT("GetSessionSettings returned null"));
		return;
	}
	SetOperation(EListenServerOperation::Updating, TEXT("advertising game map and lobby state"));
	Purpose = EListenServerAsyncPurpose::HostGameUpdate;
	FOnlineSessionSettings UpdatedSettings = *ExistingSettings;
	UpdatedSettings.Set(ListenServerNetworkKeys::MapId, PendingHostGameMap.GetAssetName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	UpdatedSettings.Set(ListenServerNetworkKeys::LobbyState, FString(TEXT("InGame")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	ApplySessionJoinability(UpdatedSettings, GetSettings()->bAllowJoinInProgress);
	TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
	UpdateHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateLambda([WeakOwner, OperationId](FName SessionName, bool bWasSuccessful)
	{
		if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
		{
			Subsystem->Impl->HandleUpdateComplete(OperationId, SessionName, bWasSuccessful);
		}
	}));
	StartTimeout(GetSettings()->UpdateTimeoutSeconds, EListenServerOperation::Updating);
	UE_LOG(LogListenServerNetwork, Log, TEXT("UpdateSession started. Id=%llu"), OperationId);
	if (!Sessions->UpdateSession(NAME_GameSession, UpdatedSettings, true))
	{
		Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateHandle);
		UpdateHandle.Reset();
		ClearTimeout();
		HandleUpdateComplete(OperationId, NAME_GameSession, false);
	}
}

void FListenServerSessionSubsystemImpl::HandleUpdateComplete(uint64 CallbackOperationId, FName SessionName, bool bWasSuccessful)
{
	const EListenServerAsyncPurpose CompletedPurpose = Purpose;
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && UpdateHandle.IsValid())
	{
		Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateHandle);
	}
	UpdateHandle.Reset();
	ClearTimeout();
	if (!IsExpectedCallback(CallbackOperationId, EListenServerOperation::Updating, TEXT("UpdateSession")))
	{
		UE_LOG(LogListenServerNetwork, Verbose, TEXT("Current session state after stale UpdateSession=%d"), Sessions.IsValid() ? static_cast<int32>(Sessions->GetSessionState(NAME_GameSession)) : -1);
		return;
	}
	UE_LOG(LogListenServerNetwork, Log, TEXT("UpdateSession completed. Id=%llu Success=%d"), CallbackOperationId, bWasSuccessful);
	if (bOperationCancelled)
	{
		FinishFailure(EListenServerError::Cancelled, TEXT("The session update was cancelled."), TEXT("Update completion arrived after cancellation"));
		return;
	}
	if (!bWasSuccessful)
	{
		FinishFailure(EListenServerError::UpdateFailed, TEXT("The host session advertisement could not be updated."), TEXT("UpdateSession completion reported failure"));
		return;
	}
	if (CompletedPurpose == EListenServerAsyncPurpose::SessionStateUpdate)
	{
		FinishSuccess(TEXT("The hosted session state and joinability were updated."));
		return;
	}
	if (Sessions->GetSessionState(NAME_GameSession) == EOnlineSessionState::InProgress)
	{
		if (!BeginTravel(CallbackOperationId, PendingHostGameMap, EListenServerRole::Host, EListenServerConnectionState::InGame, false, false))
		{
			FinishFailure(EListenServerError::TravelFailed, TEXT("Server travel could not start."), TEXT("ServerTravel validation or dispatch failed"));
		}
	}
	else
	{
		StartHostGameSession(CallbackOperationId);
	}
}

void FListenServerSessionSubsystemImpl::StartHostGameSession(uint64 OperationId)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		FinishFailure(EListenServerError::SessionInterfaceUnavailable, TEXT("Steam sessions became unavailable."), TEXT("Session interface invalid before StartSession"));
		return;
	}
	SetOperation(EListenServerOperation::Starting, TEXT("starting host session"));
	Purpose = EListenServerAsyncPurpose::HostGameStart;
	TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
	StartHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateLambda([WeakOwner, OperationId](FName SessionName, bool bWasSuccessful)
	{
		if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
		{
			Subsystem->Impl->HandleStartComplete(OperationId, SessionName, bWasSuccessful);
		}
	}));
	StartTimeout(GetSettings()->UpdateTimeoutSeconds, EListenServerOperation::Starting);
	UE_LOG(LogListenServerNetwork, Log, TEXT("StartSession started. Id=%llu"), OperationId);
	if (!Sessions->StartSession(NAME_GameSession))
	{
		Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartHandle);
		StartHandle.Reset();
		ClearTimeout();
		HandleStartComplete(OperationId, NAME_GameSession, false);
	}
}

void FListenServerSessionSubsystemImpl::HandleStartComplete(uint64 CallbackOperationId, FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && StartHandle.IsValid())
	{
		Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartHandle);
	}
	StartHandle.Reset();
	ClearTimeout();
	if (!IsExpectedCallback(CallbackOperationId, EListenServerOperation::Starting, TEXT("StartSession")))
	{
		UE_LOG(LogListenServerNetwork, Verbose, TEXT("Current session state after stale StartSession=%d"), Sessions.IsValid() ? static_cast<int32>(Sessions->GetSessionState(NAME_GameSession)) : -1);
		return;
	}
	UE_LOG(LogListenServerNetwork, Log, TEXT("StartSession completed. Id=%llu Success=%d"), CallbackOperationId, bWasSuccessful);
	if (bOperationCancelled)
	{
		FinishFailure(EListenServerError::Cancelled, TEXT("Starting the session was cancelled."), TEXT("Start completion arrived after cancellation"));
		return;
	}
	if (!bWasSuccessful)
	{
		FinishFailure(EListenServerError::StartFailed, TEXT("The host Steam session could not start."), TEXT("StartSession completion reported failure"));
		return;
	}
	if (!BeginTravel(CallbackOperationId, PendingHostGameMap, EListenServerRole::Host, EListenServerConnectionState::InGame, false, false))
	{
		FinishFailure(EListenServerError::TravelFailed, TEXT("Server travel could not start."), TEXT("ServerTravel validation or dispatch failed"));
	}
}

bool FListenServerSessionSubsystemImpl::BeginTravel(uint64 OperationId, const FSoftObjectPath& Map, EListenServerRole TravelRole, EListenServerConnectionState TargetState, bool bListenOpenLevel, bool bReturnToMenu)
{
	FString MapPackage;
	UWorld* World = GetWorld();
	if (World == nullptr || !IsMapConfiguredAndPresent(Map, MapPackage))
	{
		return false;
	}
	SetOperation(EListenServerOperation::Traveling, TEXT("map travel dispatched"));
	PendingTravel = MakeUnique<FPendingTravel>();
	PendingTravel->OperationId = OperationId;
	PendingTravel->ExpectedPackage = MapPackage;
	PendingTravel->PreviousPackage = World->GetPackage() != nullptr ? World->GetPackage()->GetName() : FString();
	PendingTravel->Role = TravelRole;
	PendingTravel->ConnectionState = TargetState;
	PendingTravel->bReturnToMenu = bReturnToMenu;
	bHostListenRetryPending = false;
	HostListenRetryCount = 0;
	HostListenRetryDelayRemaining = 0.0f;
	StartTimeout(GetSettings()->TravelTimeoutSeconds, EListenServerOperation::Traveling);
	UE_LOG(LogListenServerNetwork, Log, TEXT("Travel started. Id=%llu Target=%s Role=%d"), OperationId, *MapPackage, static_cast<int32>(TravelRole));

	if (TravelRole == EListenServerRole::Host && bListenOpenLevel)
	{
		return StartHostListenTravel() || bHostListenRetryPending;
	}
	if (TravelRole == EListenServerRole::Host)
	{
		// Reuse the active SteamSockets listener; ?listen would create a second socket on the same virtual port.
		return World->ServerTravel(MapPackage + TEXT("?SeamlessTravel"), true);
	}
	UGameplayStatics::OpenLevel(World, FName(*MapPackage), true);
	return true;
}

bool FListenServerSessionSubsystemImpl::StartHostListenTravel()
{
	UWorld* World = GetWorld();
	if (World == nullptr || !PendingTravel.IsValid())
	{
		return false;
	}
	if (World->GetNetMode() != NM_ListenServer)
	{
		FURL ListenURL = World->URL;
		if (!World->Listen(ListenURL))
		{
			return false;
		}
	}
	if (World->ServerTravel(PendingTravel->ExpectedPackage + TEXT("?SeamlessTravel"), true))
	{
		return true;
	}
	if (GEngine != nullptr && World->GetNetDriver() != nullptr)
	{
		GEngine->DestroyNamedNetDriver(World, NAME_GameNetDriver);
	}
	return false;
}

bool FListenServerSessionSubsystemImpl::BeginClientTravel(uint64 OperationId, const FString& ConnectString, EListenServerConnectionState TargetState)
{
	UGameInstance* GameInstance = GetGameInstance();
	ULocalPlayer* LocalPlayer = GameInstance != nullptr ? GameInstance->GetFirstGamePlayer() : nullptr;
	APlayerController* PlayerController = LocalPlayer != nullptr ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	UWorld* World = GetWorld();
	if (PlayerController == nullptr || World == nullptr || ConnectString.IsEmpty())
	{
		return false;
	}
	SetOperation(EListenServerOperation::Traveling, TEXT("client travel dispatched"));
	PendingTravel = MakeUnique<FPendingTravel>();
	PendingTravel->OperationId = OperationId;
	PendingTravel->PreviousPackage = World->GetPackage() != nullptr ? World->GetPackage()->GetName() : FString();
	PendingTravel->Role = EListenServerRole::Client;
	PendingTravel->ConnectionState = TargetState;
	PendingTravel->bAcceptAnyRemotePackage = true;
	StartTimeout(GetSettings()->TravelTimeoutSeconds, EListenServerOperation::Traveling);
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
	UE_LOG(LogListenServerNetwork, Log, TEXT("ClientTravel dispatched. Id=%llu ConnectString=<redacted>"), OperationId);
	return true;
}

void FListenServerSessionSubsystemImpl::HandlePreLoadMap(const FString& MapName)
{
	if (PendingTravel.IsValid() && PendingTravel->OperationId == ActiveOperationId)
	{
		UE_LOG(LogListenServerNetwork, Verbose, TEXT("PreLoadMap observed. Id=%llu Map=%s"), ActiveOperationId, *ListenServerNetworkPolicy::SanitizeExternalString(MapName, ListenServerNetworkPolicy::MaxDisplayStringLength));
	}
}

void FListenServerSessionSubsystemImpl::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadedWorld == nullptr || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	if (!PendingTravel.IsValid())
	{
		if (Role == EListenServerRole::Client && LoadedWorld->GetNetMode() == NM_Client)
		{
			const FString LoadedPackage = LoadedWorld->GetPackage() != nullptr ? LoadedWorld->GetPackage()->GetName() : FString();
			FString LobbyPackage;
			const bool bLoadedLobby = IsMapConfiguredAndPresent(GetSettings()->LobbyMap, LobbyPackage) && PackageNamesMatch(LoadedPackage, LobbyPackage);
			const EListenServerConnectionState FollowedState = bLoadedLobby ? EListenServerConnectionState::Lobby : EListenServerConnectionState::InGame;
			SetRoleAndConnection(EListenServerRole::Client, FollowedState, TEXT("remote server travel completed"));
			UE_LOG(LogListenServerNetwork, Log, TEXT("Remote server travel completed. Map=%s State=%d"), *LoadedPackage, static_cast<int32>(FollowedState));
		}
		return;
	}
	const uint64 TravelOperationId = PendingTravel->OperationId;
	if (ListenServerNetworkPolicy::IsStaleOperation(TravelOperationId, ActiveOperationId) || Operation != EListenServerOperation::Traveling)
	{
		UE_LOG(LogListenServerNetwork, Warning, TEXT("Ignored stale map-load completion. TravelId=%llu ActiveId=%llu"), TravelOperationId, ActiveOperationId);
		return;
	}
	const FString LoadedPackage = LoadedWorld->GetPackage() != nullptr ? LoadedWorld->GetPackage()->GetName() : FString();
	const bool bPackageMatches = PendingTravel->bAcceptAnyRemotePackage
		? !LoadedPackage.IsEmpty()
		: PackageNamesMatch(LoadedPackage, PendingTravel->ExpectedPackage);
	const ENetMode NetMode = LoadedWorld->GetNetMode();
	const bool bNetModeMatches = PendingTravel->Role == EListenServerRole::Host ? NetMode == NM_ListenServer
		: PendingTravel->Role == EListenServerRole::Client ? NetMode == NM_Client : NetMode != NM_ListenServer;
	if (!bPackageMatches || !bNetModeMatches)
	{
		UE_LOG(LogListenServerNetwork, Verbose, TEXT("Map load did not complete pending travel. PackageMatch=%d NetModeMatch=%d Loaded=%s NetMode=%d"), bPackageMatches, bNetModeMatches, *LoadedPackage, static_cast<int32>(NetMode));
		return;
	}

	const EListenServerRole CompletedRole = PendingTravel->Role;
	const EListenServerConnectionState CompletedConnection = PendingTravel->ConnectionState;
	const bool bReturnedToMenu = PendingTravel->bReturnToMenu;
	PendingTravel.Reset();
	bHostListenRetryPending = false;
	ClearTimeout();
	SetRoleAndConnection(CompletedRole, CompletedConnection, TEXT("map travel completed"));
	UE_LOG(LogListenServerNetwork, Log, TEXT("Travel completed. Id=%llu Map=%s NetMode=%d"), TravelOperationId, *LoadedPackage, static_cast<int32>(NetMode));
	if (bReturnedToMenu && bFailureRecoveryActive)
	{
		FinishFailure(RecoveryError, *RecoveryUserMessage, RecoveryInternalMessage);
	}
	else if (bReturnedToMenu)
	{
		FinishSuccess(TEXT("The Steam session was left."), bLeaveEndFailed ? TEXT("EndSession failed; DestroySession and local cleanup completed") : FString());
	}
	else if (CompletedRole == EListenServerRole::Host && CompletedConnection == EListenServerConnectionState::Lobby)
	{
		FinishSuccess(TEXT("The Steam listen-server lobby is ready."));
	}
	else if (CompletedRole == EListenServerRole::Host)
	{
		FinishSuccess(TEXT("Server travel completed."));
	}
	else
	{
		FinishSuccess(TEXT("Joined the Steam listen server."));
	}
}

bool FListenServerSessionSubsystemImpl::Cancel()
{
	if (Operation == EListenServerOperation::None)
	{
		return Reject(EListenServerOperation::None, EListenServerError::InvalidState, TEXT("There is no network operation to cancel."), TEXT("Cancel requested while idle"));
	}
	if (Operation == EListenServerOperation::Traveling || Operation == EListenServerOperation::Recovering || Operation == EListenServerOperation::Leaving || Operation == EListenServerOperation::Ending || Operation == EListenServerOperation::Starting)
	{
		return Reject(Operation, EListenServerError::InvalidState, TEXT("This network operation cannot be safely cancelled."), TEXT("Travel, recovery, leave, end, and start are non-cancellable"));
	}
	bOperationCancelled = true;
	if (Operation == EListenServerOperation::Searching)
	{
		IOnlineSessionPtr Sessions = GetSessionInterface();
		if (!Sessions.IsValid())
		{
			SearchResults.Reset();
			RawSearchResults.Reset();
			ActiveSearch.Reset();
			FinishFailure(EListenServerError::SearchCancelled, TEXT("The session search was cancelled."), TEXT("Session interface unavailable during cancellation"));
			return true;
		}
		SetOperation(EListenServerOperation::CancellingSearch, TEXT("cancelling Steam search"));
		const uint64 OperationId = ActiveOperationId;
		TWeakObjectPtr<UListenServerSessionSubsystem> WeakOwner = Owner;
		CancelFindHandle = Sessions->AddOnCancelFindSessionsCompleteDelegate_Handle(FOnCancelFindSessionsCompleteDelegate::CreateLambda([WeakOwner, OperationId](bool bWasSuccessful)
		{
			if (UListenServerSessionSubsystem* Subsystem = WeakOwner.Get())
			{
				Subsystem->Impl->HandleCancelFindComplete(OperationId, bWasSuccessful);
			}
		}));
		StartTimeout(GetSettings()->SearchTimeoutSeconds, EListenServerOperation::CancellingSearch);
		if (!Sessions->CancelFindSessions())
		{
			Sessions->ClearOnCancelFindSessionsCompleteDelegate_Handle(CancelFindHandle);
			Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
			CancelFindHandle.Reset();
			FindHandle.Reset();
			SearchResults.Reset();
			RawSearchResults.Reset();
			ActiveSearch.Reset();
			FinishFailure(EListenServerError::SearchCancelled, TEXT("The session search was cancelled locally."), TEXT("CancelFindSessions returned false; operation generation invalidated"));
		}
		return true;
	}
	UE_LOG(LogListenServerNetwork, Log, TEXT("Cancellation recorded. Operation=%d Id=%llu; waiting for backend completion"), static_cast<int32>(Operation), ActiveOperationId);
	return true;
}

void FListenServerSessionSubsystemImpl::HandleCancelFindComplete(uint64 CallbackOperationId, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		if (CancelFindHandle.IsValid()) Sessions->ClearOnCancelFindSessionsCompleteDelegate_Handle(CancelFindHandle);
		if (FindHandle.IsValid()) Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}
	CancelFindHandle.Reset();
	FindHandle.Reset();
	ClearTimeout();
	if (!IsExpectedCallback(CallbackOperationId, EListenServerOperation::CancellingSearch, TEXT("CancelFindSessions")))
	{
		return;
	}
	SearchResults.Reset();
	RawSearchResults.Reset();
	ActiveSearch.Reset();
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnSearchResultsChanged.Broadcast();
	}
	FinishFailure(EListenServerError::SearchCancelled, TEXT("The session search was cancelled."), bWasSuccessful ? TEXT("CancelFindSessions completed") : TEXT("CancelFindSessions completion reported failure; local results were discarded"));
}

bool FListenServerSessionSubsystemImpl::ShowInviteUI()
{
	if (!ValidateOnlineAccess(EListenServerOperation::None))
	{
		return false;
	}
	IOnlineSessionPtr Sessions = GetSessionInterface();
	IOnlineExternalUIPtr ExternalUI = GetExternalUIInterface();
	if (!ExternalUI.IsValid())
	{
		return Reject(EListenServerOperation::None, EListenServerError::ExternalUIUnavailable, TEXT("The Steam overlay invite UI is unavailable."), TEXT("GetExternalUIInterface returned null"));
	}
	if (!Sessions.IsValid() || Sessions->GetNamedSession(NAME_GameSession) == nullptr)
	{
		return Reject(EListenServerOperation::None, EListenServerError::SessionNotFound, TEXT("Create or join a session before inviting friends."), TEXT("No NAME_GameSession for ShowInviteUI"));
	}
	const bool bShown = ExternalUI->ShowInviteUI(0, NAME_GameSession);
	if (!bShown)
	{
		return Reject(EListenServerOperation::None, EListenServerError::InviteFailed, TEXT("Steam could not open the invite UI."), TEXT("ShowInviteUI returned false"));
	}
	UE_LOG(LogListenServerNetwork, Log, TEXT("Steam overlay invite UI requested"));
	return true;
}

void FListenServerSessionSubsystemImpl::HandleInviteReceived(const FUniqueNetId&, const FUniqueNetId&, const FString& AppId, const FOnlineSessionSearchResult& InviteResult)
{
	UE_LOG(LogListenServerNetwork, Log, TEXT("Steam session invite received. AppId=%s Valid=%d"), *ListenServerNetworkPolicy::SanitizeExternalString(AppId, 32), InviteResult.IsValid());
}

void FListenServerSessionSubsystemImpl::HandleInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	UE_LOG(LogListenServerNetwork, Log, TEXT("Steam session invite accepted. Success=%d Controller=%d Valid=%d"), bWasSuccessful, ControllerId, InviteResult.IsValid());
	if (!bWasSuccessful || ControllerId != 0 || !UserId.IsValid() || !InviteResult.IsValid())
	{
		Reject(EListenServerOperation::Joining, EListenServerError::InviteRejected, TEXT("The Steam invitation could not be accepted."), TEXT("Invite accepted delegate supplied invalid data"));
		return;
	}
	FListenServerSearchRequest CompatibilityRequest;
	CompatibilityRequest.MaxSearchResults = 1;
	const EListenServerError Compatibility = CheckRawCompatibility(InviteResult, CompatibilityRequest, true);
	if (Compatibility != EListenServerError::None)
	{
		Reject(EListenServerOperation::Joining, Compatibility, TEXT("The invited session is incompatible with this build."), FString::Printf(TEXT("Invite compatibility failed with %d"), static_cast<int32>(Compatibility)));
		return;
	}
	QueueOrJoinInvite(InviteResult);
}

void FListenServerSessionSubsystemImpl::QueueOrJoinInvite(const FOnlineSessionSearchResult& InviteResult)
{
	const FString SessionId = InviteResult.GetSessionIdStr();
	IOnlineSessionPtr ExistingSessions = GetSessionInterface();
	const FNamedOnlineSession* ExistingSession = ExistingSessions.IsValid() ? ExistingSessions->GetNamedSession(NAME_GameSession) : nullptr;
	if (SessionId.IsEmpty() || SessionId == PendingInviteSessionId || (ExistingSession != nullptr && ExistingSession->GetSessionIdStr() == SessionId))
	{
		UE_LOG(LogListenServerNetwork, Verbose, TEXT("Duplicate or invalid invite ignored"));
		return;
	}
	if (Operation != EListenServerOperation::None)
	{
		PendingInvite = MakeUnique<FOnlineSessionSearchResult>(InviteResult);
		PendingInviteSessionId = SessionId;
		UE_LOG(LogListenServerNetwork, Log, TEXT("Invite queued until the active operation completes"));
		return;
	}
	if (!ValidateOnlineAccess(EListenServerOperation::Joining))
	{
		return;
	}
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr)
	{
		PendingInvite = MakeUnique<FOnlineSessionSearchResult>(InviteResult);
		PendingInviteSessionId = SessionId;
		if (BeginOperation(EListenServerOperation::DestroyingExistingSession))
		{
			StartDestroy(ActiveOperationId, EListenServerAsyncPurpose::InviteDestroy);
		}
		return;
	}
	if (BeginOperation(EListenServerOperation::Joining))
	{
		StartJoinRaw(ActiveOperationId, InviteResult, EListenServerAsyncPurpose::InviteJoin);
	}
}

void FListenServerSessionSubsystemImpl::ProcessPendingInvite()
{
	if (Operation != EListenServerOperation::None || !PendingInvite.IsValid())
	{
		return;
	}
	FOnlineSessionSearchResult InviteCopy = *PendingInvite;
	PendingInvite.Reset();
	PendingInviteSessionId.Reset();
	QueueOrJoinInvite(InviteCopy);
}

void FListenServerSessionSubsystemImpl::HandleNetworkFailure(UWorld* FailedWorld, UNetDriver* FailedNetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (FailedWorld == nullptr || FailedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	const bool bPendingHostTravel = Operation == EListenServerOperation::Traveling && PendingTravel.IsValid();
	const bool bPendingHostRole = bPendingHostTravel && PendingTravel->Role == EListenServerRole::Host;
	const bool bPendingHostLobbyTravel = bPendingHostRole && PendingTravel->ConnectionState == EListenServerConnectionState::Lobby;
	const bool bCanRetryHostListen = bPendingHostLobbyTravel && ListenServerNetworkPolicy::IsRetryableHostListenFailure(FailureType);
	if (bCanRetryHostListen && HostListenRetryCount < MaxHostListenRetryCount)
	{
		bHostListenRetryPending = true;
		HostListenRetryDelayRemaining = HostListenRetryDelaySeconds;
		UE_LOG(LogListenServerNetwork, Warning, TEXT("Host listen startup failed; retry scheduled. Type=%s Attempt=%d/%d"), ENetworkFailure::ToString(FailureType), HostListenRetryCount + 1, MaxHostListenRetryCount);
		return;
	}
	const ENetMode FailedNetMode = FailedNetDriver != nullptr ? FailedNetDriver->GetNetMode() : NM_Standalone;
	if (!ListenServerNetworkPolicy::ShouldRecoverFromNetworkFailure(FailureType, FailedNetMode))
	{
		UE_LOG(LogListenServerNetwork, Log, TEXT("Ignored host-side connection failure. Type=%s"), ENetworkFailure::ToString(FailureType));
		return;
	}
	const EListenServerError Error = Role == EListenServerRole::Client ? EListenServerError::HostDisconnected : EListenServerError::ConnectionLost;
	BeginRecovery(Error, Role == EListenServerRole::Client ? TEXT("The host disconnected.") : TEXT("The network connection was lost."), FString::Printf(TEXT("NetworkFailure %d: %s"), static_cast<int32>(FailureType), *ListenServerNetworkPolicy::SanitizeExternalString(ErrorString)));
}

void FListenServerSessionSubsystemImpl::HandleTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (FailedWorld == nullptr || FailedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	BeginRecovery(EListenServerError::TravelFailed, TEXT("Map travel failed."), FString::Printf(TEXT("TravelFailure %d: %s"), static_cast<int32>(FailureType), *ListenServerNetworkPolicy::SanitizeExternalString(ErrorString)));
}

void FListenServerSessionSubsystemImpl::HandleSessionFailure(const FUniqueNetId&, ESessionFailure::Type FailureType)
{
	BeginRecovery(EListenServerError::SessionFailure, TEXT("The Steam session failed."), FString::Printf(TEXT("SessionFailure %d"), static_cast<int32>(FailureType)));
}

void FListenServerSessionSubsystemImpl::BeginRecovery(EListenServerError Error, const TCHAR* UserMessage, const FString& InternalMessage)
{
	if (bFailureRecoveryActive)
	{
		UE_LOG(LogListenServerNetwork, Verbose, TEXT("Duplicate failure event suppressed: %s"), *InternalMessage);
		return;
	}
	bFailureRecoveryActive = true;
	RecoveryError = Error;
	RecoveryUserMessage = UserMessage;
	RecoveryInternalMessage = InternalMessage;
	ClearTimeout();
	ClearOperationDelegates();
	PendingTravel.Reset();
	bHostListenRetryPending = false;
	ActiveOperationId = ListenServerNetworkPolicy::AdvanceOperationId(OperationCounter);
	bOperationCancelled = false;
	SetOperation(EListenServerOperation::Recovering, TEXT("recovering from network/session failure"));
	LastResult = MakeResult(false, Error, UserMessage, InternalMessage);
	UE_LOG(LogListenServerNetwork, Error, TEXT("Recovery started. Error=%d Id=%llu Internal=%s"), static_cast<int32>(Error), ActiveOperationId, *InternalMessage);
	if (UListenServerSessionSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->OnNetworkFailure.Broadcast(LastResult);
	}
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr)
	{
		StartDestroy(ActiveOperationId, EListenServerAsyncPurpose::RecoveryDestroy);
	}
	else
	{
		CompleteLocalCleanup();
		ReturnToMainMenu(ActiveOperationId, true);
	}
}

FListenServerDebugSnapshot FListenServerSessionSubsystemImpl::GetDebugSnapshot() const
{
	FListenServerDebugSnapshot Snapshot;
	Snapshot.Role = Role;
	Snapshot.ConnectionState = ConnectionState;
	Snapshot.Operation = Operation;
	Snapshot.OperationId = static_cast<int64>(ActiveOperationId);
	IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystem();
	Snapshot.OnlineSubsystemName = OnlineSubsystem != nullptr ? OnlineSubsystem->GetSubsystemName() : NAME_None;
	Snapshot.bSteamAvailable = OnlineSubsystem != nullptr && OnlineSubsystem->GetSubsystemName() == STEAM_SUBSYSTEM;
	IOnlineIdentityPtr Identity = GetIdentityInterface();
	Snapshot.bLoggedIn = Identity.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
	IOnlineSessionPtr Sessions = GetSessionInterface();
	Snapshot.bSessionExists = Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
	Snapshot.SearchGeneration = SearchGeneration;
	Snapshot.SearchResultCount = SearchResults.Num();
	UWorld* World = GetWorld();
	Snapshot.CurrentMap = World != nullptr && World->GetPackage() != nullptr ? World->GetPackage()->GetName() : FString();
	Snapshot.LastError = LastResult.Error;
	Snapshot.LastInternalError = LastResult.InternalError;
	Snapshot.RegisteredDelegateCount = GetRegisteredDelegateCount();
	Snapshot.bPendingInvite = PendingInvite.IsValid();
	Snapshot.bPendingTravel = PendingTravel.IsValid();
	Snapshot.bTimeoutActive = TimeoutHandle.IsValid();
	return Snapshot;
}

FListenServerConfigurationReport FListenServerSessionSubsystemImpl::ValidateConfiguration() const
{
	FListenServerConfigurationReport Report;
	auto AddIssue = [&Report](EListenServerConfigurationIssueSeverity Severity, const TCHAR* Description, const TCHAR* Resolution)
	{
		FListenServerConfigurationIssue& Issue = Report.Issues.AddDefaulted_GetRef();
		Issue.Severity = Severity;
		Issue.Description = FText::FromString(Description);
		Issue.Resolution = FText::FromString(Resolution);
	};

	const UListenServerNetworkSettings* Settings = GetSettings();
	IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystem();
	if (OnlineSubsystem == nullptr)
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("The default online subsystem is unavailable."), TEXT("Enable OnlineSubsystemSteam, start Steam, and verify DefaultPlatformService=Steam."));
	}
	else if (OnlineSubsystem->GetSubsystemName() != STEAM_SUBSYSTEM)
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("The active online subsystem is not Steam."), TEXT("Set [OnlineSubsystem] DefaultPlatformService=Steam."));
	}
	if (!GetSessionInterface().IsValid())
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("The online session interface is unavailable."), TEXT("Enable OnlineSubsystemSteam and check the Steam client log."));
	}
	IOnlineIdentityPtr Identity = GetIdentityInterface();
	if (!Identity.IsValid())
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("The online identity interface is unavailable."), TEXT("Verify Steam initialized successfully."));
	}
	else if (Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn)
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("LocalUserNum 0 is not logged in."), TEXT("Start Steam and run a standalone game instance under a signed-in Steam account."));
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance == nullptr || GameInstance->GetFirstGamePlayer() == nullptr)
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("The primary LocalPlayer is unavailable."), TEXT("Run validation after the primary player has been created."));
	}
	else if (GameInstance->GetLocalPlayers().Num() != 1)
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("Multiple local players are active."), TEXT("Disable split screen; this plugin supports one LocalPlayer per process."));
	}
	if (Settings->ProjectKey.TrimStartAndEnd().IsEmpty())
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("ProjectKey is empty."), TEXT("Set a stable project-specific key in Project Settings > Listen Server Network."));
	}
	if (Settings->BuildUniqueId <= 0)
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("BuildUniqueId is invalid."), TEXT("Set BuildUniqueId to a positive integer and increment it for incompatible builds."));
	}
	if (Settings->DefaultMaxPlayers < 1 || Settings->DefaultMaxSearchResults < 1)
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("Player or search-result limits are invalid."), TEXT("Use positive values for DefaultMaxPlayers and DefaultMaxSearchResults."));
	}
	if (Settings->CreateTimeoutSeconds <= 0.0f || Settings->SearchTimeoutSeconds <= 0.0f || Settings->JoinTimeoutSeconds <= 0.0f
		|| Settings->UpdateTimeoutSeconds <= 0.0f || Settings->DestroyTimeoutSeconds <= 0.0f || Settings->TravelTimeoutSeconds <= 0.0f)
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("One or more timeout values are invalid."), TEXT("Set every timeout to a positive number of seconds."));
	}

	auto ValidateMap = [&AddIssue](const FSoftObjectPath& Map, const TCHAR* Label)
	{
		FString Package;
		if (Map.IsNull())
		{
			AddIssue(EListenServerConfigurationIssueSeverity::Error, *FString::Printf(TEXT("%s is not configured."), Label), TEXT("Assign a local World asset in Project Settings > Listen Server Network."));
		}
		else if (!IsMapConfiguredAndPresent(Map, Package))
		{
			AddIssue(EListenServerConfigurationIssueSeverity::Error, *FString::Printf(TEXT("%s does not exist."), Label), TEXT("Create the configured map asset and include it in the packaged build."));
		}
	};
	ValidateMap(Settings->MainMenuMap, TEXT("MainMenuMap"));
	ValidateMap(Settings->LobbyMap, TEXT("LobbyMap"));
	ValidateMap(Settings->DefaultGameMap, TEXT("DefaultGameMap"));

	TSharedPtr<IPlugin> SteamPlugin = IPluginManager::Get().FindPlugin(TEXT("OnlineSubsystemSteam"));
	if (!SteamPlugin.IsValid() || !SteamPlugin->IsEnabled())
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("OnlineSubsystemSteam is not enabled."), TEXT("Enable OnlineSubsystemSteam in the project or plugin descriptor."));
	}
	TSharedPtr<IPlugin> SteamSocketsPlugin = IPluginManager::Get().FindPlugin(TEXT("SteamSockets"));
	if (!SteamSocketsPlugin.IsValid() || !SteamSocketsPlugin->IsEnabled())
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("SteamSockets is not enabled."), TEXT("Enable the SteamSockets plugin for UE 5.7 Steam P2P travel."));
	}
	FString DefaultService;
	GConfig->GetString(TEXT("OnlineSubsystem"), TEXT("DefaultPlatformService"), DefaultService, GEngineIni);
	if (!DefaultService.Equals(TEXT("Steam"), ESearchCase::IgnoreCase))
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("DefaultPlatformService is not Steam."), TEXT("Set [OnlineSubsystem] DefaultPlatformService=Steam in DefaultEngine.ini."));
	}
	FString SteamAppId;
	GConfig->GetString(TEXT("OnlineSubsystemSteam"), TEXT("SteamDevAppId"), SteamAppId, GEngineIni);
	if (SteamAppId == TEXT("480"))
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Warning, TEXT("SteamDevAppId 480 is the shared Spacewar development application."), TEXT("Expect unrelated public lobbies and filtering limits; use your own App ID before release."));
	}
	else if (SteamAppId.IsEmpty())
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("SteamDevAppId is not configured."), TEXT("Set SteamDevAppId in [OnlineSubsystemSteam]."));
	}
	TArray<FString> NetDriverDefinitions;
	GConfig->GetArray(TEXT("/Script/Engine.GameEngine"), TEXT("NetDriverDefinitions"), NetDriverDefinitions, GEngineIni);
	bool bSteamSocketsNetDriverFound = false;
	bool bLegacySteamNetDriverFound = false;
	for (const FString& Definition : NetDriverDefinitions)
	{
		bSteamSocketsNetDriverFound |= Definition.Contains(TEXT("SteamSockets.SteamSocketsNetDriver"));
		bLegacySteamNetDriverFound |= Definition.Contains(TEXT("OnlineSubsystemSteam.SteamNetDriver"));
	}
	if (!bSteamSocketsNetDriverFound)
	{
		const TCHAR* Description = bLegacySteamNetDriverFound
			? TEXT("The GameNetDriver uses the legacy OnlineSubsystemSteam.SteamNetDriver, which is unavailable in UE 5.7.")
			: TEXT("A SteamSockets GameNetDriver definition was not found.");
		AddIssue(EListenServerConfigurationIssueSeverity::Error, Description, TEXT("Configure one GameNetDriver using /Script/SteamSockets.SteamSocketsNetDriver with IpNetDriver fallback."));
	}
	FString SteamSocketsNetConnectionClass;
	GConfig->GetString(TEXT("/Script/SteamSockets.SteamSocketsNetDriver"), TEXT("NetConnectionClassName"), SteamSocketsNetConnectionClass, GEngineIni);
	if (!SteamSocketsNetConnectionClass.Contains(TEXT("SteamSockets.SteamSocketsNetConnection")))
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Error, TEXT("The SteamSockets NetConnection class is not configured."), TEXT("Set NetConnectionClassName=/Script/SteamSockets.SteamSocketsNetConnection for SteamSocketsNetDriver."));
	}

	Report.bIsValid = !Report.Issues.ContainsByPredicate([](const FListenServerConfigurationIssue& Issue)
	{
		return Issue.Severity == EListenServerConfigurationIssueSeverity::Error;
	});
	if (Report.Issues.IsEmpty())
	{
		AddIssue(EListenServerConfigurationIssueSeverity::Valid, TEXT("No configuration problems were detected."), TEXT("Perform the two-account standalone Steam test before shipping."));
		Report.bIsValid = true;
	}
	return Report;
}

bool UListenServerSessionSubsystem::HostSession(const FListenServerHostRequest& Request)
{
	return Impl->Host(Request);
}

bool UListenServerSessionSubsystem::FindSessions(const FListenServerSearchRequest& Request)
{
	return Impl->Find(Request);
}

bool UListenServerSessionSubsystem::JoinSession(const FListenServerSearchResultHandle& ResultHandle)
{
	return Impl->Join(ResultHandle);
}

bool UListenServerSessionSubsystem::StartQuickMatch(const FListenServerQuickMatchRequest& Request)
{
	return Impl->QuickMatch(Request);
}

bool UListenServerSessionSubsystem::LeaveSession()
{
	return Impl->Leave();
}

bool UListenServerSessionSubsystem::HostTravelToMap(const FSoftObjectPath& Map)
{
	return Impl->HostTravel(Map);
}

bool UListenServerSessionSubsystem::UpdateHostedSessionState(
	EListenServerAdvertisedSessionState NewState,
	bool bAllowNewParticipants
)
{
	return Impl->UpdateHostedSessionState(NewState, bAllowNewParticipants);
}

bool UListenServerSessionSubsystem::ShowInviteUI()
{
	return Impl->ShowInviteUI();
}

bool UListenServerSessionSubsystem::CancelCurrentOperation()
{
	return Impl->Cancel();
}

bool UListenServerSessionSubsystem::HostDefaultSession()
{
	const UListenServerNetworkSettings* Settings = GetDefault<UListenServerNetworkSettings>();
	FListenServerHostRequest Request;
	Request.MaxPlayers = Settings->DefaultMaxPlayers;
	Request.GameModeId = Settings->DefaultGameModeId;
	Request.Region = Settings->DefaultRegion;
	Request.SessionDisplayName = Settings->DefaultSessionDisplayName;
	Request.LobbyMap = Settings->LobbyMap;
	return HostSession(Request);
}

bool UListenServerSessionSubsystem::FindDefaultSessions()
{
	const UListenServerNetworkSettings* Settings = GetDefault<UListenServerNetworkSettings>();
	FListenServerSearchRequest Request;
	Request.MaxSearchResults = Settings->DefaultMaxSearchResults;
	Request.GameModeId = Settings->DefaultGameModeId;
	Request.Region = Settings->DefaultRegion;
	return FindSessions(Request);
}

bool UListenServerSessionSubsystem::QuickMatchDefault()
{
	const UListenServerNetworkSettings* Settings = GetDefault<UListenServerNetworkSettings>();
	FListenServerQuickMatchRequest Request;
	Request.SearchRequest.MaxSearchResults = Settings->DefaultMaxSearchResults;
	Request.SearchRequest.GameModeId = Settings->DefaultGameModeId;
	Request.SearchRequest.Region = Settings->DefaultRegion;
	Request.HostRequest.MaxPlayers = Settings->DefaultMaxPlayers;
	Request.HostRequest.GameModeId = Settings->DefaultGameModeId;
	Request.HostRequest.Region = Settings->DefaultRegion;
	Request.HostRequest.SessionDisplayName = Settings->DefaultSessionDisplayName;
	Request.HostRequest.LobbyMap = Settings->LobbyMap;
	Request.bAllowHostFallback = true;
	return StartQuickMatch(Request);
}

bool UListenServerSessionSubsystem::LeaveAndReturnToMenu()
{
	return LeaveSession();
}

bool UListenServerSessionSubsystem::HostTravelToDefaultGameMap()
{
	return HostTravelToMap(GetDefault<UListenServerNetworkSettings>()->DefaultGameMap);
}

EListenServerRole UListenServerSessionSubsystem::GetCurrentRole() const
{
	return Impl->Role;
}

EListenServerConnectionState UListenServerSessionSubsystem::GetConnectionState() const
{
	return Impl->ConnectionState;
}

EListenServerOperation UListenServerSessionSubsystem::GetCurrentOperation() const
{
	return Impl->Operation;
}

FListenServerOperationResult UListenServerSessionSubsystem::GetLastOperationResult() const
{
	return Impl->LastResult;
}

bool UListenServerSessionSubsystem::GetCurrentSessionAttribute(
	FName Key,
	FString& OutValue
) const
{
	OutValue.Reset();
	if (Key.IsNone())
	{
		return false;
	}

	const IOnlineSessionPtr Sessions = Impl->GetSessionInterface();
	const FOnlineSessionSettings* SessionSettings = Sessions.IsValid()
		? Sessions->GetSessionSettings(NAME_GameSession)
		: nullptr;
	return SessionSettings != nullptr
		&& GetStringSetting(*SessionSettings, Key, OutValue);
}

int32 UListenServerSessionSubsystem::GetSearchResultCount() const
{
	return Impl->SearchResults.Num();
}

bool UListenServerSessionSubsystem::GetSearchResultByIndex(int32 Index, FListenServerSearchResult& OutResult) const
{
	if (!Impl->SearchResults.IsValidIndex(Index))
	{
		return false;
	}
	OutResult = Impl->SearchResults[Index];
	return true;
}

bool UListenServerSessionSubsystem::GetSearchResultByHandle(const FListenServerSearchResultHandle& Handle, FListenServerSearchResult& OutResult) const
{
	if (!ListenServerNetworkPolicy::IsSearchHandleValid(Handle, Impl->SearchGeneration, Impl->SearchResults.Num()))
	{
		return false;
	}
	OutResult = Impl->SearchResults[Handle.ResultIndex];
	return true;
}

int32 UListenServerSessionSubsystem::GetParticipantCount() const
{
	return Impl->Participants.Num();
}

bool UListenServerSessionSubsystem::GetParticipantByIndex(int32 Index, FListenServerParticipant& OutParticipant) const
{
	if (!Impl->Participants.IsValidIndex(Index))
	{
		return false;
	}
	OutParticipant = Impl->Participants[Index];
	return true;
}

TArray<FListenServerParticipant> UListenServerSessionSubsystem::GetParticipants() const
{
	return Impl->Participants;
}

FListenServerConnectionDiagnostics UListenServerSessionSubsystem::GetConnectionDiagnostics() const
{
	return Impl->ConnectionDiagnostics;
}

FListenServerDebugSnapshot UListenServerSessionSubsystem::GetDebugSnapshot() const
{
	return Impl->GetDebugSnapshot();
}

FListenServerConfigurationReport UListenServerSessionSubsystem::ValidateConfiguration() const
{
	return Impl->ValidateConfiguration();
}

void UListenServerSessionSubsystem::LogConfigurationReport() const
{
	const FListenServerConfigurationReport Report = ValidateConfiguration();
	UE_LOG(LogListenServerNetwork, Log, TEXT("Configuration report: Valid=%d Issues=%d"), Report.bIsValid, Report.Issues.Num());
	for (const FListenServerConfigurationIssue& Issue : Report.Issues)
	{
		UE_LOG(LogListenServerNetwork, Log, TEXT("  Severity=%d Problem=%s Resolution=%s"), static_cast<int32>(Issue.Severity), *Issue.Description.ToString(), *Issue.Resolution.ToString());
	}
}

TConstArrayView<FListenServerSearchResult> UListenServerSessionSubsystem::GetSearchResultsView() const
{
	return Impl->SearchResults;
}

TConstArrayView<FListenServerParticipant> UListenServerSessionSubsystem::GetParticipantsView() const
{
	return Impl->Participants;
}
