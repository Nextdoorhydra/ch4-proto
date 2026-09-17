#include "ListenServerNetworkConsoleCommands.h"

#include "Modules/ModuleManager.h"

class FListenServerNetworkModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		ConsoleCommands.Register();
	}

	virtual void ShutdownModule() override
	{
		ConsoleCommands.Unregister();
	}

private:
	FListenServerNetworkConsoleCommands ConsoleCommands;
};

IMPLEMENT_MODULE(FListenServerNetworkModule, ListenServerNetwork)
