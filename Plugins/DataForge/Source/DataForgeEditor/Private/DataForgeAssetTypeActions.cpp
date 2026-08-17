#include "DataForgeAssetTypeActions.h"

#include "DataForgeEditorService.h"
#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeBindingPreset.h"
#include "DataForgeRuleSet.h"
#include "DataForgeRuleSetEditorToolkit.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Toolkits/SimpleAssetEditor.h"

#define LOCTEXT_NAMESPACE "DataForgeAssetTypeActions"

FText FDataForgeAssetTypeActions::GetName() const
{
	return LOCTEXT("AssetName", "DataForge RuleSet");
}

FColor FDataForgeAssetTypeActions::GetTypeColor() const
{
	return FColor(70, 170, 255);
}

UClass* FDataForgeAssetTypeActions::GetSupportedClass() const
{
	return UDataForgeRuleSet::StaticClass();
}

uint32 FDataForgeAssetTypeActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

void FDataForgeAssetTypeActions::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (UObject* Object : InObjects)
	{
		if (UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Object))
		{
			TSharedRef<FDataForgeRuleSetEditorToolkit> Editor = MakeShared<FDataForgeRuleSetEditorToolkit>();
			Editor->InitEditor(RuleSet, Mode, EditWithinLevelEditor);
		}
	}
}

void FDataForgeAssetTypeActions::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	const TArray<TWeakObjectPtr<UDataForgeRuleSet>> RuleSets = GetTypedWeakObjectPtrs<UDataForgeRuleSet>(InObjects);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Probe", "DataForge: Probe Source"),
		LOCTEXT("ProbeTooltip", "Read a sample without changing project content."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([RuleSets]()
		{
			for (const TWeakObjectPtr<UDataForgeRuleSet>& RuleSet : RuleSets)
			{
				if (RuleSet.IsValid())
				{
					FDataForgeEditorService::Probe(*RuleSet.Get());
				}
			}
		})));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Preview", "DataForge: Preview"),
		LOCTEXT("PreviewTooltip", "Compile and show the desired DataTable changes without mutating content."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([RuleSets]()
		{
			for (const TWeakObjectPtr<UDataForgeRuleSet>& RuleSet : RuleSets)
			{
				if (RuleSet.IsValid())
				{
					FDataForgeEditorService::Preview(*RuleSet.Get());
				}
			}
		})));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Apply", "DataForge: Apply Last Preview"),
		LOCTEXT("ApplyTooltip", "Apply the last successful, still-current preview."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([RuleSets]()
		{
			for (const TWeakObjectPtr<UDataForgeRuleSet>& RuleSet : RuleSets)
			{
				if (RuleSet.IsValid())
				{
					FDataForgeEditorService::Apply(*RuleSet.Get());
				}
			}
		})));
}

FText FDataForgeAssetLayoutProfileActions::GetName() const
{
	return LOCTEXT("LayoutProfileAssetName", "DataForge Asset Layout Profile");
}

FColor FDataForgeAssetLayoutProfileActions::GetTypeColor() const
{
	return FColor(155, 105, 235);
}

UClass* FDataForgeAssetLayoutProfileActions::GetSupportedClass() const
{
	return UDataForgeAssetLayoutProfile::StaticClass();
}

uint32 FDataForgeAssetLayoutProfileActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

void FDataForgeAssetLayoutProfileActions::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (UObject* Object : InObjects)
	{
		if (UDataForgeAssetLayoutProfile* Profile = Cast<UDataForgeAssetLayoutProfile>(Object))
		{
			FSimpleAssetEditor::CreateEditor(Mode, EditWithinLevelEditor, Profile);
		}
	}
}

FText FDataForgeBindingPresetActions::GetName() const
{
	return LOCTEXT("BindingPresetAssetName", "DataForge Binding Preset");
}

FColor FDataForgeBindingPresetActions::GetTypeColor() const
{
	return FColor(75, 170, 210);
}

UClass* FDataForgeBindingPresetActions::GetSupportedClass() const
{
	return UDataForgeBindingPreset::StaticClass();
}

uint32 FDataForgeBindingPresetActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

void FDataForgeBindingPresetActions::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (UObject* Object : InObjects)
	{
		if (UDataForgeBindingPreset* Preset = Cast<UDataForgeBindingPreset>(Object))
		{
			FSimpleAssetEditor::CreateEditor(Mode, EditWithinLevelEditor, Preset);
		}
	}
}

#undef LOCTEXT_NAMESPACE
