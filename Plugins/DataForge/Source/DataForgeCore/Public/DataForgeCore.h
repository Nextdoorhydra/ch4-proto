#pragma once

#include "Modules/ModuleManager.h"

DATAFORGECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogDataForge, Log, All);

class FDataForgeCoreModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
