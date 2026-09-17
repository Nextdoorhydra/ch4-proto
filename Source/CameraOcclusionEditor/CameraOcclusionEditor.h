#pragma once

#include "Modules/ModuleManager.h"

class CAMERAOCCLUSIONEDITOR_API FCameraOcclusionEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
};
