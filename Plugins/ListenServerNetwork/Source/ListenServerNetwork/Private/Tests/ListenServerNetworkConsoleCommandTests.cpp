#if WITH_DEV_AUTOMATION_TESTS

#include "ListenServerNetworkConsoleCommands.h"

#include "Engine/Console.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/StringOutputDevice.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FListenServerNetworkConsoleStateParserTest,
	"ListenServerNetwork.Console.StateParser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FListenServerNetworkConsoleStateParserTest::RunTest(const FString& Parameters)
{
	EListenServerAdvertisedSessionState State = EListenServerAdvertisedSessionState::Closed;
	TestTrue(TEXT("Lobby parses case-insensitively"), ListenServerNetworkConsole::TryParseAdvertisedSessionState(TEXT("lobby"), State));
	TestEqual(TEXT("Lobby value"), State, EListenServerAdvertisedSessionState::Lobby);
	TestTrue(TEXT("InGame parses"), ListenServerNetworkConsole::TryParseAdvertisedSessionState(TEXT("InGame"), State));
	TestEqual(TEXT("InGame value"), State, EListenServerAdvertisedSessionState::InGame);
	TestTrue(TEXT("Closed parses"), ListenServerNetworkConsole::TryParseAdvertisedSessionState(TEXT("Closed"), State));
	TestEqual(TEXT("Closed value"), State, EListenServerAdvertisedSessionState::Closed);
	TestFalse(TEXT("Unknown state is rejected"), ListenServerNetworkConsole::TryParseAdvertisedSessionState(TEXT("Waiting"), State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FListenServerNetworkConsoleBinaryFlagParserTest,
	"ListenServerNetwork.Console.BinaryFlagParser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FListenServerNetworkConsoleBinaryFlagParserTest::RunTest(const FString& Parameters)
{
	bool bValue = false;
	TestTrue(TEXT("0 parses"), ListenServerNetworkConsole::TryParseBinaryFlag(TEXT("0"), bValue));
	TestFalse(TEXT("0 value"), bValue);
	TestTrue(TEXT("1 parses"), ListenServerNetworkConsole::TryParseBinaryFlag(TEXT("1"), bValue));
	TestTrue(TEXT("1 value"), bValue);
	TestFalse(TEXT("true is rejected"), ListenServerNetworkConsole::TryParseBinaryFlag(TEXT("true"), bValue));
	TestFalse(TEXT("false is rejected"), ListenServerNetworkConsole::TryParseBinaryFlag(TEXT("false"), bValue));
	TestFalse(TEXT("Other numbers are rejected"), ListenServerNetworkConsole::TryParseBinaryFlag(TEXT("2"), bValue));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FListenServerNetworkConsoleAutoCompleteTest,
	"ListenServerNetwork.Console.AutoComplete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FListenServerNetworkConsoleAutoCompleteTest::RunTest(const FString& Parameters)
{
	IConsoleObject* DiagnosticsObject = IConsoleManager::Get().FindConsoleObject(TEXT("LSN.ConnectionDiagnostics"));
	TestNotNull(TEXT("Connection diagnostics command is registered"), DiagnosticsObject);
	if (DiagnosticsObject != nullptr)
	{
		FStringOutputDevice Output;
		TArray<FString> Args;
		static_cast<IConsoleCommand*>(DiagnosticsObject)->Execute(Args, nullptr, Output);
		TestFalse(TEXT("Connection diagnostics command writes to its output device"), Output.IsEmpty());
	}

	TArray<FAutoCompleteCommand> Entries;
	UConsole::RegisterConsoleAutoCompleteEntries.Broadcast(Entries);
	const auto ContainsCommand = [&Entries](const TCHAR* Expected)
	{
		return Entries.ContainsByPredicate([Expected](const FAutoCompleteCommand& Entry)
		{
			return Entry.Command == Expected;
		});
	};

	TestTrue(TEXT("Lobby joinable recommendation"), ContainsCommand(TEXT("LSN.UpdateHostedSessionState Lobby 1")));
	TestTrue(TEXT("Lobby closed recommendation"), ContainsCommand(TEXT("LSN.UpdateHostedSessionState Lobby 0")));
	TestTrue(TEXT("InGame joinable recommendation"), ContainsCommand(TEXT("LSN.UpdateHostedSessionState InGame 1")));
	TestTrue(TEXT("InGame closed recommendation"), ContainsCommand(TEXT("LSN.UpdateHostedSessionState InGame 0")));
	TestTrue(TEXT("Closed recommendation"), ContainsCommand(TEXT("LSN.UpdateHostedSessionState Closed 0")));
	TestFalse(TEXT("Invalid Closed 1 is not recommended"), ContainsCommand(TEXT("LSN.UpdateHostedSessionState Closed 1")));
	return true;
}

#endif
