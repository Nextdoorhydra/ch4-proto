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
	void RegisterMenus();
	void HandleInMemoryAssetDeleted(UObject* DeletedAsset);
	bool OpenPendingDeletionWizards(float DeltaTime);

	TSharedPtr<IAssetTypeActions> RuleSetAssetActions;
	TSharedPtr<IAssetTypeActions> LayoutProfileAssetActions;
	TSharedPtr<IAssetTypeActions> BindingPresetAssetActions;
	TSet<FSoftObjectPath> PendingDeletionRuleSets;
	FDelegateHandle AssetDeletedHandle;
	FTSTicker::FDelegateHandle DeletionWizardTickerHandle;
};
