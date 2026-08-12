#include "DataForgeEditor.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "DataForgeAutoReconciler.h"
#include "DataForgeAssetTypeActions.h"
#include "DataForgeBindingCustomization.h"
#include "DataForgeContentPathCustomization.h"
#include "DataForgeRuleSet.h"
#include "DataForgeRuleSetCustomization.h"
#include "DataForgeRuleCreationWizard.h"
#include "DataForgeSourceCustomization.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "IAssetTools.h"
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Misc/App.h"
#include "UObject/MetaData.h"

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
		TEXT("DataForgeGeneratedAssetOutputRule"),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataForgeGeneratedOutputCustomization::MakeInstance));
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

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetDeletedHandle = AssetRegistry.OnInMemoryAssetDeleted().AddRaw(this, &FDataForgeEditorModule::HandleInMemoryAssetDeleted);
	FDataForgeAutoReconciler::Get().Startup();
}

void FDataForgeEditorModule::ShutdownModule()
{
	FDataForgeAutoReconciler::Get().Shutdown();
	if (DeletionWizardTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DeletionWizardTickerHandle);
		DeletionWizardTickerHandle.Reset();
	}
	if (AssetDeletedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().OnInMemoryAssetDeleted().Remove(AssetDeletedHandle);
		AssetDeletedHandle.Reset();
	}
	if (FModuleManager::Get().IsModuleLoaded(TEXT("MessageLog")))
	{
		FModuleManager::GetModuleChecked<FMessageLogModule>(TEXT("MessageLog")).UnregisterLogListing(TEXT("DataForge"));
	}
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.UnregisterCustomClassLayout(UDataForgeRuleSet::StaticClass()->GetFName());
		PropertyEditor.UnregisterCustomPropertyTypeLayout(TEXT("DataForgeBindingRule"));
		PropertyEditor.UnregisterCustomPropertyTypeLayout(TEXT("DataForgeGeneratedAssetOutputRule"));
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

void FDataForgeEditorModule::HandleInMemoryAssetDeleted(UObject* DeletedAsset)
{
	if (!DeletedAsset || IsRunningCommandlet() || FApp::IsUnattended())
	{
		return;
	}

	const FString DeletedPackageName = DeletedAsset->GetOutermost()->GetName();
	FString ManagedRuleSetId;
	if (UDataAsset* DeletedDataAsset = Cast<UDataAsset>(DeletedAsset))
	{
		FMetaData& MetaData = DeletedDataAsset->GetOutermost()->GetMetaData();
		if (MetaData.GetValue(DeletedDataAsset, TEXT("DataForge.Managed")) == TEXT("true"))
		{
			ManagedRuleSetId = MetaData.GetValue(DeletedDataAsset, TEXT("DataForge.RuleSetId"));
		}
	}

	if (!DeletedAsset->IsA<UDataTable>() && ManagedRuleSetId.IsEmpty())
	{
		return;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> RuleSetAssets;
	AssetRegistry.GetAssetsByClass(UDataForgeRuleSet::StaticClass()->GetClassPathName(), RuleSetAssets, true);
	for (const FAssetData& RuleSetAsset : RuleSetAssets)
	{
		UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(RuleSetAsset.GetAsset());
		if (!RuleSet) continue;
		const bool bOwnsDataTable = DeletedAsset->IsA<UDataTable>() && RuleSet->Output.AssetPath == DeletedPackageName;
		const bool bOwnsManagedAsset = !ManagedRuleSetId.IsEmpty()
			&& RuleSet->RuleSetId.ToString(EGuidFormats::Digits) == ManagedRuleSetId;
		if (bOwnsDataTable || bOwnsManagedAsset)
		{
			PendingDeletionRuleSets.Add(FSoftObjectPath(RuleSet));
		}
	}

	if (!PendingDeletionRuleSets.IsEmpty() && !DeletionWizardTickerHandle.IsValid())
	{
		DeletionWizardTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FDataForgeEditorModule::OpenPendingDeletionWizards));
	}
}

bool FDataForgeEditorModule::OpenPendingDeletionWizards(float)
{
	DeletionWizardTickerHandle.Reset();
	TSet<FSoftObjectPath> RuleSets = MoveTemp(PendingDeletionRuleSets);
	PendingDeletionRuleSets.Reset();
	for (const FSoftObjectPath& RuleSetPath : RuleSets)
	{
		if (UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(RuleSetPath.TryLoad()))
		{
			OpenDataForgeRuleCreationWizard(*RuleSet);
		}
	}
	return false;
}

IMPLEMENT_MODULE(FDataForgeEditorModule, DataForgeEditor)

#undef LOCTEXT_NAMESPACE
