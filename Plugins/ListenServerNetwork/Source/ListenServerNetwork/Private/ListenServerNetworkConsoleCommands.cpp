#include "ListenServerNetworkConsoleCommands.h"

#include "ListenServerNetworkLog.h"
#include "ListenServerNetworkPolicy.h"
#include "ListenServerSessionSubsystem.h"

#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace ListenServerNetworkConsole
{
	bool TryParseAdvertisedSessionState(const FString& Value, EListenServerAdvertisedSessionState& OutState)
	{
		if (Value.Equals(TEXT("Lobby"), ESearchCase::IgnoreCase))
		{
			OutState = EListenServerAdvertisedSessionState::Lobby;
			return true;
		}
		if (Value.Equals(TEXT("InGame"), ESearchCase::IgnoreCase))
		{
			OutState = EListenServerAdvertisedSessionState::InGame;
			return true;
		}
		if (Value.Equals(TEXT("Closed"), ESearchCase::IgnoreCase))
		{
			OutState = EListenServerAdvertisedSessionState::Closed;
			return true;
		}
		return false;
	}

	bool TryParseBinaryFlag(const FString& Value, bool& bOutValue)
	{
		if (Value == TEXT("1"))
		{
			bOutValue = true;
			return true;
		}
		if (Value == TEXT("0"))
		{
			bOutValue = false;
			return true;
		}
		return false;
	}
}

namespace
{
#if !UE_BUILD_SHIPPING
	template <typename CallbackType>
	void ForEachRuntimeSubsystem(CallbackType&& Callback, FOutputDevice* OutputDevice = nullptr)
	{
		if (GEngine == nullptr)
		{
			if (OutputDevice != nullptr)
			{
				OutputDevice->Logf(TEXT("GEngine is unavailable"));
			}
			else
			{
				UE_LOG(LogListenServerNetwork, Warning, TEXT("GEngine is unavailable"));
			}
			return;
		}
		int32 MatchCount = 0;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World == nullptr || (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE))
			{
				continue;
			}
			UGameInstance* GameInstance = World->GetGameInstance();
			UListenServerSessionSubsystem* Subsystem = GameInstance != nullptr ? GameInstance->GetSubsystem<UListenServerSessionSubsystem>() : nullptr;
			if (Subsystem != nullptr)
			{
				++MatchCount;
				if (OutputDevice != nullptr)
				{
					OutputDevice->Logf(TEXT("World[%d] Name=%s Type=%d"), MatchCount - 1, *World->GetName(), static_cast<int32>(World->WorldType));
				}
				else
				{
					UE_LOG(LogListenServerNetwork, Log, TEXT("World[%d] Name=%s Type=%d"), MatchCount - 1, *World->GetName(), static_cast<int32>(World->WorldType));
				}
				Callback(*Subsystem);
			}
		}
		if (MatchCount == 0)
		{
			if (OutputDevice != nullptr)
			{
				OutputDevice->Logf(TEXT("No Game or PIE ListenServerSessionSubsystem was found"));
			}
			else
			{
				UE_LOG(LogListenServerNetwork, Warning, TEXT("No Game or PIE ListenServerSessionSubsystem was found"));
			}
		}
	}

	void ConsoleStatus()
	{
		ForEachRuntimeSubsystem([](UListenServerSessionSubsystem& Subsystem)
		{
			const FListenServerDebugSnapshot Snapshot = Subsystem.GetDebugSnapshot();
			UE_LOG(LogListenServerNetwork, Log, TEXT("Status Role=%d Connection=%d Operation=%d OperationId=%lld OSS=%s Steam=%d LoggedIn=%d Session=%d SearchGeneration=%d Results=%d Map=%s LastError=%d Delegates=%d Invite=%d Travel=%d Timeout=%d"),
				static_cast<int32>(Snapshot.Role), static_cast<int32>(Snapshot.ConnectionState), static_cast<int32>(Snapshot.Operation), Snapshot.OperationId,
				*Snapshot.OnlineSubsystemName.ToString(), Snapshot.bSteamAvailable, Snapshot.bLoggedIn, Snapshot.bSessionExists, Snapshot.SearchGeneration,
				Snapshot.SearchResultCount, *Snapshot.CurrentMap, static_cast<int32>(Snapshot.LastError), Snapshot.RegisteredDelegateCount,
				Snapshot.bPendingInvite, Snapshot.bPendingTravel, Snapshot.bTimeoutActive);
		});
	}

	void ConsoleValidate()
	{
		ForEachRuntimeSubsystem([](UListenServerSessionSubsystem& Subsystem)
		{
			Subsystem.LogConfigurationReport();
		});
	}

	void ConsoleConnectionDiagnostics(FOutputDevice& OutputDevice)
	{
		ForEachRuntimeSubsystem([&OutputDevice](UListenServerSessionSubsystem& Subsystem)
		{
			const FListenServerConnectionDiagnostics Diagnostics = Subsystem.GetConnectionDiagnostics();
			const FString RoleName = StaticEnum<EListenServerRole>()->GetNameStringByValue(static_cast<int64>(Diagnostics.Role));
			const FString ConnectionStateName = StaticEnum<EListenServerConnectionState>()->GetNameStringByValue(static_cast<int64>(Diagnostics.ConnectionState));
			const FString LinkStateName = StaticEnum<EListenServerLinkState>()->GetNameStringByValue(static_cast<int64>(Diagnostics.LinkState));
			const FString QualityName = StaticEnum<EListenServerConnectionQuality>()->GetNameStringByValue(static_cast<int64>(Diagnostics.Quality));
			OutputDevice.Logf(TEXT("ConnectionDiagnostics Role=%s SessionState=%s LinkState=%s Quality=%s PingMs=%d IncomingLoss=%.2f%% OutgoingLoss=%.2f%%"),
				*RoleName, *ConnectionStateName, *LinkStateName, *QualityName, Diagnostics.PingMilliseconds,
				Diagnostics.IncomingPacketLossPercent, Diagnostics.OutgoingPacketLossPercent);
		}, &OutputDevice);
	}

	void ConsoleDumpSearchResults()
	{
		ForEachRuntimeSubsystem([](UListenServerSessionSubsystem& Subsystem)
		{
			for (const FListenServerSearchResult& Result : Subsystem.GetSearchResultsView())
			{
				UE_LOG(LogListenServerNetwork, Log, TEXT("Result[%d] Generation=%d Session=%s Owner=%s Display=%s Mode=%s Map=%s Region=%s State=%s Joinable=%d Players=%d/%d Ping=%d"),
					Result.Handle.ResultIndex, Result.Handle.SearchGeneration, *Result.SessionId, *Result.OwningUserName, *Result.SessionDisplayName,
					*Result.GameModeId.ToString(), *Result.MapId.ToString(), *Result.Region.ToString(), *Result.LobbyState.ToString(), Result.bIsJoinable,
					Result.CurrentPlayers, Result.MaxPlayers, Result.PingMilliseconds);
			}
		});
	}

	void ConsoleDumpStructLayouts()
	{
		UE_LOG(LogListenServerNetwork, Log, TEXT("Layout FOperationContextPod sizeof=%llu alignof=%llu"), sizeof(ListenServerNetworkPolicy::FOperationContextPod), alignof(ListenServerNetworkPolicy::FOperationContextPod));
		UE_LOG(LogListenServerNetwork, Log, TEXT("Layout FSearchCandidatePod sizeof=%llu alignof=%llu"), sizeof(ListenServerNetworkPolicy::FSearchCandidatePod), alignof(ListenServerNetworkPolicy::FSearchCandidatePod));
		UE_LOG(LogListenServerNetwork, Log, TEXT("Layout FListenServerSearchResultHandle sizeof=%llu alignof=%llu"), sizeof(FListenServerSearchResultHandle), alignof(FListenServerSearchResultHandle));
		UE_LOG(LogListenServerNetwork, Log, TEXT("Layout FListenServerSessionAttribute sizeof=%llu alignof=%llu"), sizeof(FListenServerSessionAttribute), alignof(FListenServerSessionAttribute));
	}

	void ConsoleUpdateHostedSessionState(const TArray<FString>& Args)
	{
		EListenServerAdvertisedSessionState State = EListenServerAdvertisedSessionState::Closed;
		bool bAllowNewParticipants = false;
		if (Args.Num() != 2
			|| !ListenServerNetworkConsole::TryParseAdvertisedSessionState(Args[0], State)
			|| !ListenServerNetworkConsole::TryParseBinaryFlag(Args[1], bAllowNewParticipants))
		{
			UE_LOG(LogListenServerNetwork, Warning, TEXT("Usage: LSN.UpdateHostedSessionState <Lobby|InGame|Closed> <0|1>"));
			return;
		}

		int32 HostCount = 0;
		ForEachRuntimeSubsystem([State, bAllowNewParticipants, &HostCount](UListenServerSessionSubsystem& Subsystem)
		{
			if (Subsystem.GetCurrentRole() != EListenServerRole::Host)
			{
				return;
			}
			++HostCount;
			const bool bStarted = Subsystem.UpdateHostedSessionState(State, bAllowNewParticipants);
			UE_LOG(LogListenServerNetwork, Log, TEXT("UpdateHostedSessionState console request: State=%d Joinable=%d Started=%d"),
				static_cast<int32>(State), bAllowNewParticipants, bStarted);
		});
		if (HostCount == 0)
		{
			UE_LOG(LogListenServerNetwork, Warning, TEXT("No hosted Listen Server session was found"));
		}
	}
#endif
}

void FListenServerNetworkConsoleCommands::Register()
{
#if !UE_BUILD_SHIPPING
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(TEXT("LSN.Status"), TEXT("Print Listen Server Network status for each Game/PIE world."), FConsoleCommandDelegate::CreateStatic(&ConsoleStatus), ECVF_Default));
	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(TEXT("LSN.Validate"), TEXT("Validate Listen Server Network configuration."), FConsoleCommandDelegate::CreateStatic(&ConsoleValidate), ECVF_Default));
	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(TEXT("LSN.ConnectionDiagnostics"), TEXT("Print connection state, ping, packet loss, and quality."), FConsoleCommandWithOutputDeviceDelegate::CreateStatic(&ConsoleConnectionDiagnostics), ECVF_Default));
	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(TEXT("LSN.DumpSearchResults"), TEXT("Print cached compatible search results."), FConsoleCommandDelegate::CreateStatic(&ConsoleDumpSearchResults), ECVF_Default));
	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(TEXT("LSN.DumpStructLayouts"), TEXT("Print important structure sizes and alignments."), FConsoleCommandDelegate::CreateStatic(&ConsoleDumpStructLayouts), ECVF_Default));
	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(TEXT("LSN.UpdateHostedSessionState"), TEXT("Update hosted session advertisement. Usage: LSN.UpdateHostedSessionState <Lobby|InGame|Closed> <0|1>."), FConsoleCommandWithArgsDelegate::CreateStatic(&ConsoleUpdateHostedSessionState), ECVF_Default));
	AutoCompleteHandle = UConsole::RegisterConsoleAutoCompleteEntries.AddRaw(this, &FListenServerNetworkConsoleCommands::AddAutoCompleteEntries);
#endif
}

void FListenServerNetworkConsoleCommands::Unregister()
{
#if !UE_BUILD_SHIPPING
	UConsole::RegisterConsoleAutoCompleteEntries.Remove(AutoCompleteHandle);
	AutoCompleteHandle.Reset();

	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	for (IConsoleObject* Command : RegisteredCommands)
	{
		if (Command != nullptr)
		{
			ConsoleManager.UnregisterConsoleObject(Command);
		}
	}
	RegisteredCommands.Reset();
#endif
}

#if !UE_BUILD_SHIPPING
void FListenServerNetworkConsoleCommands::AddAutoCompleteEntries(TArray<FAutoCompleteCommand>& Entries)
{
	const auto AddEntry = [&Entries](const TCHAR* Command, const TCHAR* Description)
	{
		FAutoCompleteCommand& Entry = Entries.AddDefaulted_GetRef();
		Entry.Command = Command;
		Entry.Desc = Description;
	};

	AddEntry(TEXT("LSN.UpdateHostedSessionState Lobby 1"), TEXT("Advertise the lobby and allow new participants."));
	AddEntry(TEXT("LSN.UpdateHostedSessionState Lobby 0"), TEXT("Keep the lobby active but block new participants."));
	AddEntry(TEXT("LSN.UpdateHostedSessionState InGame 1"), TEXT("Advertise the in-game session and allow join-in-progress."));
	AddEntry(TEXT("LSN.UpdateHostedSessionState InGame 0"), TEXT("Keep the in-game session active but block join-in-progress."));
	AddEntry(TEXT("LSN.UpdateHostedSessionState Closed 0"), TEXT("Close the session to all new participants."));
}
#endif
