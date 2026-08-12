#pragma once

#include "Modules/ModuleManager.h"

class IAssetTypeActions;

class FDataForgeEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<IAssetTypeActions> RuleSetAssetActions;
};
