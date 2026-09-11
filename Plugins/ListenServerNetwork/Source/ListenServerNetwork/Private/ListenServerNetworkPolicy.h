#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "ListenServerNetworkTypes.h"
#include "Net/Core/Connection/NetEnums.h"

namespace ListenServerNetworkKeys
{
	extern const FName ProjectKey;
	extern const FName BuildVersion;
	extern const FName GameMode;
	extern const FName MapId;
	extern const FName Region;
	extern const FName LobbyState;
	extern const FName Joinable;
	extern const FName SessionDisplayName;
}

namespace ListenServerNetworkPolicy
{
	constexpr int32 MaxAttributeKeyLength = 64;
	constexpr int32 MaxAttributeValueLength = 256;
	constexpr int32 MaxDisplayStringLength = 128;

	struct FOperationContextPod
	{
		uint64 OperationId = 0;
		double StartedAtSeconds = 0.0;
		EListenServerOperation Operation = EListenServerOperation::None;
		bool bCancelled = false;
	};

	struct FSearchCandidatePod
	{
		int32 RawIndex = INDEX_NONE;
		int32 OpenPublicConnections = 0;
		int32 PingMilliseconds = INDEX_NONE;
		bool bPingValid = false;
	};

	struct FCompatibilityCandidate
	{
		FString ProjectKey;
		int32 BuildUniqueId = 0;
		FName GameMode;
		FName Region;
		int32 OpenPublicConnections = 0;
		bool bJoinable = true;
		TMap<FName, FString> Attributes;
	};

	struct FCompatibilityRequest
	{
		FString ProjectKey;
		int32 BuildUniqueId = 0;
		FName GameMode;
		FName Region;
		TArray<FListenServerSessionAttribute> RequiredAttributes;
	};

	enum class EJoinFailureCode : uint8
	{
		SessionFull,
		SessionNotFound,
		ConnectString,
		AlreadyJoined,
		Unknown
	};

	bool IsValidOperationTransition(EListenServerOperation From, EListenServerOperation To);
	uint64 AdvanceOperationId(uint64& Counter);
	bool IsStaleOperation(uint64 CallbackOperationId, uint64 ActiveOperationId);
	bool IsSearchHandleValid(const FListenServerSearchResultHandle& Handle, int32 SearchGeneration, int32 ResultCount);
	bool IsReservedKey(FName Key);
	bool ValidateAttributes(const TArray<FListenServerSessionAttribute>& Attributes, bool bRejectReservedKeys, FString& OutReason);
	EListenServerError CheckCompatibility(const FCompatibilityCandidate& Candidate, const FCompatibilityRequest& Request);
	EListenServerError MapJoinFailure(EJoinFailureCode FailureCode);
	bool IsRetryableHostListenFailure(ENetworkFailure::Type FailureType);
	bool ShouldRecoverFromNetworkFailure(ENetworkFailure::Type FailureType, ENetMode FailedNetMode);
	FString SanitizeExternalString(const FString& Value, int32 MaxLength = MaxAttributeValueLength);
}
