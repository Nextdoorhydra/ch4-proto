#pragma once

#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"

class IAssetTypeActions;
class UObject;

class FDataForgeEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void HandleInMemoryAssetDeleted(UObject* DeletedAsset);
	bool OpenPendingDeletionWizards(float DeltaTime);

	TSharedPtr<IAssetTypeActions> RuleSetAssetActions;
	TSet<FSoftObjectPath> PendingDeletionRuleSets;
	FDelegateHandle AssetDeletedHandle;
	FTSTicker::FDelegateHandle DeletionWizardTickerHandle;
};
