#pragma once

#include "CoreMinimal.h"
#include "ListenServerNetworkTypes.h"

class IConsoleObject;
struct FAutoCompleteCommand;

namespace ListenServerNetworkConsole
{
	bool TryParseAdvertisedSessionState(const FString& Value, EListenServerAdvertisedSessionState& OutState);
	bool TryParseBinaryFlag(const FString& Value, bool& bOutValue);
}

class FListenServerNetworkConsoleCommands
{
public:
	void Register();
	void Unregister();

private:
#if !UE_BUILD_SHIPPING
	void AddAutoCompleteEntries(TArray<FAutoCompleteCommand>& Entries);

	TArray<IConsoleObject*> RegisteredCommands;
	FDelegateHandle AutoCompleteHandle;
#endif
};
