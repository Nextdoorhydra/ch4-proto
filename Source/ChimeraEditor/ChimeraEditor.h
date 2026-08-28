#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FChimeraEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void ConvertSelectedMaterials(const TArray<struct FAssetData>& SelectedAssets);
	void ConvertMaterialsUnderFolders(const TArray<FString>& SelectedPackagePaths);
	void OnGoogleSheetCacheUpdated(class UGoogleSheetConfig& Config);
	FDelegateHandle GoogleSheetCacheUpdatedHandle;
	class IConsoleObject* DataForgeMcpCommand = nullptr;
};
