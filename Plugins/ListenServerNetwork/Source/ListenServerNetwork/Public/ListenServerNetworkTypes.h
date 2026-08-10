#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

#include "ListenServerNetworkTypes.generated.h"

UENUM(BlueprintType)
enum class EListenServerRole : uint8
{
	None,
	Host,
	Client
};

UENUM(BlueprintType)
enum class EListenServerConnectionState : uint8
{
	Offline,
	Lobby,
	InGame
};

UENUM(BlueprintType)
enum class EListenServerOperation : uint8
{
	None,
	DestroyingExistingSession,
	Creating,
	Searching,
	CancellingSearch,
	Joining,
	Updating,
	Starting,
	Ending,
	Leaving,
	Destroying,
	Traveling,
	Recovering
};

UENUM(BlueprintType)
enum class EListenServerError : uint8
{
	None,
	InvalidRequest,
	InvalidState,
	InvalidLocalUser,
	MultipleLocalUsersUnsupported,
	SteamUnavailable,
	OnlineSubsystemUnavailable,
	SessionInterfaceUnavailable,
	IdentityInterfaceUnavailable,
	ExternalUIUnavailable,
	NotLoggedIn,
	AlreadyBusy,
	SessionAlreadyExists,
	CreateFailed,
	SearchFailed,
	SearchCancelled,
	NoSessionsFound,
	InvalidSearchHandle,
	JoinFailed,
	SessionFull,
	SessionNotFound,
	IncompatibleProject,
	IncompatibleBuild,
	IncompatibleGameMode,
	IncompatibleRegion,
	IncompatibleAttribute,
	ConnectStringFailed,
	UpdateFailed,
	StartFailed,
	EndFailed,
	TravelFailed,
	TravelTimeout,
	ConnectionLost,
	HostDisconnected,
	SessionFailure,
	DestroyFailed,
	InviteFailed,
	InviteRejected,
	Cancelled,
	Timeout,
	Unknown
};

USTRUCT(BlueprintType)
struct LISTENSERVERNETWORK_API FListenServerOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	bool bRecoverable = true;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	EListenServerError Error = EListenServerError::None;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FText UserMessage;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FString InternalError;
};

USTRUCT(BlueprintType)
struct LISTENSERVERNETWORK_API FListenServerSessionAttribute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	FString Value;
};

USTRUCT(BlueprintType)
struct LISTENSERVERNETWORK_API FListenServerHostRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	int32 MaxPlayers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	FName GameModeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	FName Region;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	FString SessionDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	FSoftObjectPath LobbyMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	TArray<FListenServerSessionAttribute> ExtraAdvertisedAttributes;
};

USTRUCT(BlueprintType)
struct LISTENSERVERNETWORK_API FListenServerSearchRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	int32 MaxSearchResults = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	FName GameModeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	FName Region;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	TArray<FListenServerSessionAttribute> RequiredAttributes;
};

USTRUCT(BlueprintType)
struct LISTENSERVERNETWORK_API FListenServerQuickMatchRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	FListenServerSearchRequest SearchRequest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	FListenServerHostRequest HostRequest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Listen Server Network")
	bool bAllowHostFallback = true;
};

USTRUCT(BlueprintType)
struct LISTENSERVERNETWORK_API FListenServerSearchResultHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	int32 SearchGeneration = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	int32 ResultIndex = INDEX_NONE;

	bool IsValid() const
	{
		return SearchGeneration >= 0 && ResultIndex >= 0;
	}
};

USTRUCT(BlueprintType)
struct LISTENSERVERNETWORK_API FListenServerSearchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FListenServerSearchResultHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FString OwningUserName;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FString SessionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FName GameModeId;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FName MapId;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FName Region;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FName LobbyState;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	int32 PingMilliseconds = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	TArray<FListenServerSessionAttribute> AdvertisedAttributes;
};

USTRUCT(BlueprintType)
struct LISTENSERVERNETWORK_API FListenServerParticipant
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FString PlatformUserId;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	int32 PingMilliseconds = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	bool bIsHost = false;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	bool bIsLocalPlayer = false;

	bool operator==(const FListenServerParticipant& Other) const
	{
		return PlayerId == Other.PlayerId
			&& DisplayName == Other.DisplayName
			&& PlatformUserId == Other.PlatformUserId
			&& PingMilliseconds == Other.PingMilliseconds
			&& bIsHost == Other.bIsHost
			&& bIsLocalPlayer == Other.bIsLocalPlayer;
	}
};

USTRUCT(BlueprintType)
struct LISTENSERVERNETWORK_API FListenServerDebugSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	EListenServerRole Role = EListenServerRole::None;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	EListenServerConnectionState ConnectionState = EListenServerConnectionState::Offline;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	EListenServerOperation Operation = EListenServerOperation::None;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	int64 OperationId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FName OnlineSubsystemName;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	bool bSteamAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	bool bLoggedIn = false;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	bool bSessionExists = false;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	int32 SearchGeneration = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	int32 SearchResultCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FString CurrentMap;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	EListenServerError LastError = EListenServerError::None;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FString LastInternalError;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	int32 RegisteredDelegateCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	bool bPendingInvite = false;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	bool bPendingTravel = false;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	bool bTimeoutActive = false;
};

UENUM(BlueprintType)
enum class EListenServerConfigurationIssueSeverity : uint8
{
	Valid,
	Warning,
	Error
};

USTRUCT(BlueprintType)
struct LISTENSERVERNETWORK_API FListenServerConfigurationIssue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	EListenServerConfigurationIssueSeverity Severity = EListenServerConfigurationIssueSeverity::Valid;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	FText Resolution;
};

USTRUCT(BlueprintType)
struct LISTENSERVERNETWORK_API FListenServerConfigurationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category="Listen Server Network")
	TArray<FListenServerConfigurationIssue> Issues;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FListenServerStateChanged, EListenServerRole, Role, EListenServerConnectionState, ConnectionState, EListenServerOperation, Operation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FListenServerOperationCompleted, EListenServerOperation, CompletedOperation, const FListenServerOperationResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FListenServerSearchResultsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FListenServerParticipantsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FListenServerNetworkFailure, const FListenServerOperationResult&, Result);
