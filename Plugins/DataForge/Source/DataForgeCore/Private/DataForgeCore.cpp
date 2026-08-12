#include "DataForgeCore.h"

#include "DataForgePipeline.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogDataForge);

void FDataForgeCoreModule::StartupModule()
{
	FDataForgeSourceAdapterRegistry::Get().Register(MakeShared<FDataForgeCsvSourceAdapter>());
	FDataForgeSourceAdapterRegistry::Get().Register(MakeShared<FDataForgeJsonSourceAdapter>());
}

void FDataForgeCoreModule::ShutdownModule()
{
	FDataForgeSourceAdapterRegistry::Get().Unregister(TEXT("Csv"));
	FDataForgeSourceAdapterRegistry::Get().Unregister(TEXT("Json"));
}

IMPLEMENT_MODULE(FDataForgeCoreModule, DataForgeCore)
