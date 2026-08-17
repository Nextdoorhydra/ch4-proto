#include "Modules/ModuleManager.h"

#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "NKMLocalizationEditorLog.h"
#include "SNKMLocalizationDashboard.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FNKMLocalizationEditorModule"

DEFINE_LOG_CATEGORY(LogNKMLocalization);

namespace
{
	const FName DashboardTabName(TEXT("NKMLocalizationDashboard"));
}

class FNKMLocalizationEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (!GIsEditor || IsRunningCommandlet())
		{
			return;
		}

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			DashboardTabName,
			FOnSpawnTab::CreateRaw(this, &FNKMLocalizationEditorModule::SpawnDashboardTab))
			.SetDisplayName(LOCTEXT("DashboardTitle", "NKM Localization"))
			.SetTooltipText(LOCTEXT("DashboardTooltip", "Edit NKM text sources, build PO/LocRes, and audit gameplay CSV references."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

		ToolMenusHandle = UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FNKMLocalizationEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		if (UToolMenus::Get())
		{
			UToolMenus::UnRegisterStartupCallback(ToolMenusHandle);
			UToolMenus::UnregisterOwner(this);
		}
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DashboardTabName);
		}
	}

private:
	TSharedRef<SDockTab> SpawnDashboardTab(const FSpawnTabArgs& Args)
	{
		const TSharedRef<SNKMLocalizationDashboard> Dashboard = SNew(SNKMLocalizationDashboard);
		const TSharedRef<SDockTab> Tab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				Dashboard
			];
		Tab->SetCanCloseTab(SDockTab::FCanCloseTab::CreateSP(Dashboard, &SNKMLocalizationDashboard::CanCloseTab));
		return Tab;
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		if (!ToolsMenu)
		{
			return;
		}

		FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("NKMTools"));
		Section.AddMenuEntry(
			TEXT("OpenNKMLocalization"),
			LOCTEXT("OpenDashboard", "NKM Localization"),
			LOCTEXT("OpenDashboardTooltip", "Open the NKM localization source and translation dashboard."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(DashboardTabName); })));
	}

	FDelegateHandle ToolMenusHandle;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNKMLocalizationEditorModule, NKMLocalizationEditor);
