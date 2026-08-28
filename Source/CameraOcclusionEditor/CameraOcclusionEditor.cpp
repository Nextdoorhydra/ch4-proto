#include "CameraOcclusionEditor.h"

#include "CameraOcclusionMaterialConverter.h"
#include "ContentBrowserMenuContexts.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FCameraOcclusionEditorModule"

void FCameraOcclusionEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.AssetContextMenu"));
    FToolMenuSection& AssetSection = AssetMenu->FindOrAddSection(
        TEXT("ChimeraCameraOcclusion"),
        LOCTEXT("CameraOcclusionSection", "Chimera Camera Occlusion"));
    AssetSection.AddDynamicEntry(
        TEXT("CameraOcclusionSelectedMaterials"),
        FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& Section)
        {
            const UContentBrowserAssetContextMenuContext* Context =
                Section.FindContext<UContentBrowserAssetContextMenuContext>();
            if (!Context || Context->SelectedAssets.IsEmpty() || !Context->bCanBeModified)
            {
                return;
            }

            const TArray<FAssetData> SelectedAssets = Context->SelectedAssets;
            Section.AddMenuEntry(
                TEXT("ConvertSelectedMaterials"),
                LOCTEXT("ConvertSelectedMaterialsLabel", "Convert Selected Materials"),
                LOCTEXT("ConvertSelectedMaterialsTooltip", "Add Camera Occlusion support to the selected materials."),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([SelectedAssets]()
                {
                    FCameraOcclusionMaterialConverter::ConvertSelectedMaterials(SelectedAssets);
                })));
        }));

    UToolMenu* FolderMenu = UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.FolderContextMenu"));
    FToolMenuSection& FolderSection = FolderMenu->FindOrAddSection(
        TEXT("ChimeraCameraOcclusion"),
        LOCTEXT("CameraOcclusionFolderSection", "Chimera Camera Occlusion"));
    FolderSection.AddDynamicEntry(
        TEXT("CameraOcclusionFolderMaterials"),
        FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& Section)
        {
            const UContentBrowserFolderContext* Context =
                Section.FindContext<UContentBrowserFolderContext>();
            if (!Context || Context->SelectedPackagePaths.IsEmpty() || !Context->bCanBeModified)
            {
                return;
            }

            const TArray<FString> SelectedPaths = Context->SelectedPackagePaths;
            Section.AddMenuEntry(
                TEXT("ConvertFolderMaterials"),
                LOCTEXT("ConvertFolderMaterialsLabel", "Convert Materials Under Folder"),
                LOCTEXT("ConvertFolderMaterialsTooltip", "Recursively add Camera Occlusion support to materials under the selected folders."),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([SelectedPaths]()
                {
                    FCameraOcclusionMaterialConverter::ConvertMaterialsUnderFolders(SelectedPaths);
                })));
        }));
}

void FCameraOcclusionEditorModule::StartupModule()
{
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCameraOcclusionEditorModule::RegisterMenus));
}

void FCameraOcclusionEditorModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCameraOcclusionEditorModule, CameraOcclusionEditor)

