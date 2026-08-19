#include "Modules/ModuleManager.h"
#include "GameplayMessageRuntime/Public/GameFramework/GameplayMessageSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMGore, Log, All);

class FCMGoreModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(
			LogCMGore,
			Log,
			TEXT("CMGore module started.")
		);
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(
			LogCMGore,
			Log,
			TEXT("CMGore module shutdown.")
		);
	}
};

IMPLEMENT_MODULE(FCMGoreModule, CMGore)