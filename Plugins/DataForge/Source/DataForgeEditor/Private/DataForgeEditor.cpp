#include "DataForgeEditor.h"

#include "AssetToolsModule.h"
#include "DataForgeAssetTypeActions.h"
#include "DataForgeBindingCustomization.h"
#include "DataForgeContentPathCustomization.h"
#include "DataForgeRuleSet.h"
#include "DataForgeRuleSetCustomization.h"
#include "DataForgeSourceCustomization.h"
#include "IAssetTools.h"
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "DataForgeEditorModule"

void FDataForgeEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	RuleSetAssetActions = MakeShared<FDataForgeAssetTypeActions>();
	AssetTools.RegisterAssetTypeActions(RuleSetAssetActions.ToSharedRef());

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditor.RegisterCustomClassLayout(
		UDataForgeRuleSet::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FDataForgeRuleSetCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(
		TEXT("DataForgeBindingRule"),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataForgeBindingCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(
		TEXT("DataForgeSourceConfig"),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataForgeSourceCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(
		TEXT("DataForgeSourceInput"),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataForgeSourceInputCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(
		TEXT("DataForgeDataTableOutputRule"),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataForgeOutputCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(
		TEXT("DataForgeAssetRule"),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataForgeAssetRuleCustomization::MakeInstance));
	PropertyEditor.NotifyCustomizationModuleChanged();

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
	MessageLogModule.RegisterLogListing(TEXT("DataForge"), LOCTEXT("LogLabel", "DataForge"));
}

void FDataForgeEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("MessageLog")))
	{
		FModuleManager::GetModuleChecked<FMessageLogModule>(TEXT("MessageLog")).UnregisterLogListing(TEXT("DataForge"));
	}
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.UnregisterCustomClassLayout(UDataForgeRuleSet::StaticClass()->GetFName());
		PropertyEditor.UnregisterCustomPropertyTypeLayout(TEXT("DataForgeBindingRule"));
		PropertyEditor.UnregisterCustomPropertyTypeLayout(TEXT("DataForgeSourceConfig"));
		PropertyEditor.UnregisterCustomPropertyTypeLayout(TEXT("DataForgeSourceInput"));
		PropertyEditor.UnregisterCustomPropertyTypeLayout(TEXT("DataForgeDataTableOutputRule"));
		PropertyEditor.UnregisterCustomPropertyTypeLayout(TEXT("DataForgeAssetRule"));
		PropertyEditor.NotifyCustomizationModuleChanged();
	}
	if (RuleSetAssetActions.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
	{
		FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().UnregisterAssetTypeActions(RuleSetAssetActions.ToSharedRef());
	}
	RuleSetAssetActions.Reset();
}

IMPLEMENT_MODULE(FDataForgeEditorModule, DataForgeEditor)

#undef LOCTEXT_NAMESPACE
