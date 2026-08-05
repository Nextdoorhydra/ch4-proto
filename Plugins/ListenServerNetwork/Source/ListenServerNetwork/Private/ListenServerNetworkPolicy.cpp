#include "ListenServerNetworkPolicy.h"

namespace ListenServerNetworkKeys
{
	const FName ProjectKey(TEXT("PROJECT_KEY"));
	const FName BuildVersion(TEXT("BUILD_VERSION"));
	const FName GameMode(TEXT("GAME_MODE"));
	const FName MapId(TEXT("MAP_ID"));
	const FName Region(TEXT("REGION"));
	const FName LobbyState(TEXT("LOBBY_STATE"));
	const FName SessionDisplayName(TEXT("SESSION_DISPLAY_NAME"));
}

namespace ListenServerNetworkPolicy
{
	static_assert(sizeof(FOperationContextPod) <= 24, "Operation context should remain compact");
	static_assert(alignof(FOperationContextPod) <= 8, "Unexpected operation context alignment");
	static_assert(sizeof(FSearchCandidatePod) <= 16, "Search candidate should remain compact");
	static_assert(alignof(FSearchCandidatePod) <= 4, "Unexpected search candidate alignment");

	bool IsValidOperationTransition(EListenServerOperation From, EListenServerOperation To)
	{
		if (From == To)
		{
			return true;
		}

		if (To == EListenServerOperation::None || To == EListenServerOperation::Recovering)
		{
			return true;
		}

		switch (From)
		{
		case EListenServerOperation::None:
			return true;
		case EListenServerOperation::DestroyingExistingSession:
			return To == EListenServerOperation::Creating || To == EListenServerOperation::Joining;
		case EListenServerOperation::Creating:
			return To == EListenServerOperation::Traveling || To == EListenServerOperation::Destroying || To == EListenServerOperation::DestroyingExistingSession;
		case EListenServerOperation::Searching:
			return To == EListenServerOperation::CancellingSearch || To == EListenServerOperation::Joining || To == EListenServerOperation::Creating;
		case EListenServerOperation::CancellingSearch:
			return To == EListenServerOperation::Searching;
		case EListenServerOperation::Joining:
			return To == EListenServerOperation::Traveling || To == EListenServerOperation::Searching || To == EListenServerOperation::Destroying;
		case EListenServerOperation::Updating:
			return To == EListenServerOperation::Starting || To == EListenServerOperation::Traveling;
		case EListenServerOperation::Starting:
			return To == EListenServerOperation::Traveling;
		case EListenServerOperation::Ending:
			return To == EListenServerOperation::Destroying;
		case EListenServerOperation::Leaving:
			return To == EListenServerOperation::Ending || To == EListenServerOperation::Destroying || To == EListenServerOperation::Traveling;
		case EListenServerOperation::Destroying:
			return To == EListenServerOperation::Creating || To == EListenServerOperation::Joining || To == EListenServerOperation::Traveling;
		case EListenServerOperation::Traveling:
			return To == EListenServerOperation::Updating;
		case EListenServerOperation::Recovering:
			return To == EListenServerOperation::Destroying || To == EListenServerOperation::Traveling;
		default:
			return false;
		}
	}

	uint64 AdvanceOperationId(uint64& Counter)
	{
		++Counter;
		if (Counter == 0)
		{
			++Counter;
		}
		return Counter;
	}

	bool IsStaleOperation(uint64 CallbackOperationId, uint64 ActiveOperationId)
	{
		return CallbackOperationId == 0 || CallbackOperationId != ActiveOperationId;
	}

	bool IsSearchHandleValid(const FListenServerSearchResultHandle& Handle, int32 SearchGeneration, int32 ResultCount)
	{
		return Handle.IsValid() && Handle.SearchGeneration == SearchGeneration && Handle.ResultIndex < ResultCount;
	}

	bool IsReservedKey(FName Key)
	{
		return Key == ListenServerNetworkKeys::ProjectKey
			|| Key == ListenServerNetworkKeys::BuildVersion
			|| Key == ListenServerNetworkKeys::GameMode
			|| Key == ListenServerNetworkKeys::MapId
			|| Key == ListenServerNetworkKeys::Region
			|| Key == ListenServerNetworkKeys::LobbyState
			|| Key == ListenServerNetworkKeys::SessionDisplayName;
	}

	bool ValidateAttributes(const TArray<FListenServerSessionAttribute>& Attributes, bool bRejectReservedKeys, FString& OutReason)
	{
		TSet<FName> Keys;
		Keys.Reserve(Attributes.Num());

		for (const FListenServerSessionAttribute& Attribute : Attributes)
		{
			if (Attribute.Key.IsNone())
			{
				OutReason = TEXT("Attribute key is empty");
				return false;
			}
			if (Attribute.Key.ToString().Len() > MaxAttributeKeyLength)
			{
				OutReason = TEXT("Attribute key exceeds 64 characters");
				return false;
			}
			if (Attribute.Value.Len() > MaxAttributeValueLength)
			{
				OutReason = TEXT("Attribute value exceeds 256 characters");
				return false;
			}
			if (bRejectReservedKeys && IsReservedKey(Attribute.Key))
			{
				OutReason = FString::Printf(TEXT("Attribute key '%s' is reserved"), *Attribute.Key.ToString());
				return false;
			}
			if (Keys.Contains(Attribute.Key))
			{
				OutReason = FString::Printf(TEXT("Attribute key '%s' is duplicated"), *Attribute.Key.ToString());
				return false;
			}
			Keys.Add(Attribute.Key);
		}

		OutReason.Reset();
		return true;
	}

	EListenServerError CheckCompatibility(const FCompatibilityCandidate& Candidate, const FCompatibilityRequest& Request)
	{
		if (Candidate.ProjectKey != Request.ProjectKey)
		{
			return EListenServerError::IncompatibleProject;
		}
		if (Candidate.BuildUniqueId != Request.BuildUniqueId)
		{
			return EListenServerError::IncompatibleBuild;
		}
		if (!Request.GameMode.IsNone() && Candidate.GameMode != Request.GameMode)
		{
			return EListenServerError::IncompatibleGameMode;
		}
		if (!Request.Region.IsNone() && Candidate.Region != Request.Region)
		{
			return EListenServerError::IncompatibleRegion;
		}
		if (Candidate.OpenPublicConnections <= 0)
		{
			return EListenServerError::SessionFull;
		}

		for (const FListenServerSessionAttribute& Required : Request.RequiredAttributes)
		{
			const FString* Found = Candidate.Attributes.Find(Required.Key);
			if (Found == nullptr || *Found != Required.Value)
			{
				return EListenServerError::IncompatibleAttribute;
			}
		}

		return EListenServerError::None;
	}

	EListenServerError MapJoinFailure(EJoinFailureCode FailureCode)
	{
		switch (FailureCode)
		{
		case EJoinFailureCode::SessionFull:
			return EListenServerError::SessionFull;
		case EJoinFailureCode::SessionNotFound:
			return EListenServerError::SessionNotFound;
		case EJoinFailureCode::ConnectString:
			return EListenServerError::ConnectStringFailed;
		case EJoinFailureCode::AlreadyJoined:
			return EListenServerError::InvalidState;
		default:
			return EListenServerError::JoinFailed;
		}
	}

	FString SanitizeExternalString(const FString& Value, int32 MaxLength)
	{
		FString Result;
		Result.Reserve(FMath::Min(Value.Len(), MaxLength));
		for (TCHAR Character : Value)
		{
			if (Result.Len() >= MaxLength)
			{
				break;
			}
			if (Character >= TEXT(' ') && Character != 0x7f)
			{
				Result.AppendChar(Character);
			}
			else if (Character == TEXT('\t'))
			{
				Result.AppendChar(TEXT(' '));
			}
		}
		return Result;
	}
}
