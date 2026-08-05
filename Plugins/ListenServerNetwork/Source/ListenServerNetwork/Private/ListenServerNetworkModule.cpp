#include "ListenServerNetworkLog.h"
#include "ListenServerNetworkPolicy.h"
#include "ListenServerSessionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

namespace
{
#if !UE_BUILD_SHIPPING
	template <typename CallbackType>
	void ForEachRuntimeSubsystem(CallbackType&& Callback)
	{
		if (GEngine == nullptr)
		{
			UE_LOG(LogListenServerNetwork, Warning, TEXT("GEngine is unavailable"));
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
				UE_LOG(LogListenServerNetwork, Log, TEXT("World[%d] Name=%s Type=%d"), MatchCount - 1, *World->GetName(), static_cast<int32>(World->WorldType));
				Callback(*Subsystem);
			}
		}
		if (MatchCount == 0)
		{
			UE_LOG(LogListenServerNetwork, Warning, TEXT("No Game or PIE ListenServerSessionSubsystem was found"));
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

	void ConsoleDumpSearchResults()
	{
		ForEachRuntimeSubsystem([](UListenServerSessionSubsystem& Subsystem)
		{
			for (const FListenServerSearchResult& Result : Subsystem.GetSearchResultsView())
			{
				UE_LOG(LogListenServerNetwork, Log, TEXT("Result[%d] Generation=%d Session=%s Owner=%s Display=%s Mode=%s Map=%s Region=%s State=%s Players=%d/%d Ping=%d"),
					Result.Handle.ResultIndex, Result.Handle.SearchGeneration, *Result.SessionId, *Result.OwningUserName, *Result.SessionDisplayName,
					*Result.GameModeId.ToString(), *Result.MapId.ToString(), *Result.Region.ToString(), *Result.LobbyState.ToString(),
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
#endif
}

class FListenServerNetworkModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if !UE_BUILD_SHIPPING
		IConsoleManager& ConsoleManager = IConsoleManager::Get();
		StatusCommand = ConsoleManager.RegisterConsoleCommand(TEXT("LSN.Status"), TEXT("Print Listen Server Network status for each Game/PIE world."), FConsoleCommandDelegate::CreateStatic(&ConsoleStatus), ECVF_Default);
		ValidateCommand = ConsoleManager.RegisterConsoleCommand(TEXT("LSN.Validate"), TEXT("Validate Listen Server Network configuration."), FConsoleCommandDelegate::CreateStatic(&ConsoleValidate), ECVF_Default);
		DumpSearchResultsCommand = ConsoleManager.RegisterConsoleCommand(TEXT("LSN.DumpSearchResults"), TEXT("Print cached compatible search results."), FConsoleCommandDelegate::CreateStatic(&ConsoleDumpSearchResults), ECVF_Default);
		DumpStructLayoutsCommand = ConsoleManager.RegisterConsoleCommand(TEXT("LSN.DumpStructLayouts"), TEXT("Print important structure sizes and alignments."), FConsoleCommandDelegate::CreateStatic(&ConsoleDumpStructLayouts), ECVF_Default);
#endif
	}

	virtual void ShutdownModule() override
	{
#if !UE_BUILD_SHIPPING
		IConsoleManager& ConsoleManager = IConsoleManager::Get();
		if (StatusCommand != nullptr) ConsoleManager.UnregisterConsoleObject(StatusCommand);
		if (ValidateCommand != nullptr) ConsoleManager.UnregisterConsoleObject(ValidateCommand);
		if (DumpSearchResultsCommand != nullptr) ConsoleManager.UnregisterConsoleObject(DumpSearchResultsCommand);
		if (DumpStructLayoutsCommand != nullptr) ConsoleManager.UnregisterConsoleObject(DumpStructLayoutsCommand);
		StatusCommand = nullptr;
		ValidateCommand = nullptr;
		DumpSearchResultsCommand = nullptr;
		DumpStructLayoutsCommand = nullptr;
#endif
	}

private:
#if !UE_BUILD_SHIPPING
	IConsoleObject* StatusCommand = nullptr;
	IConsoleObject* ValidateCommand = nullptr;
	IConsoleObject* DumpSearchResultsCommand = nullptr;
	IConsoleObject* DumpStructLayoutsCommand = nullptr;
#endif
};

IMPLEMENT_MODULE(FListenServerNetworkModule, ListenServerNetwork)
