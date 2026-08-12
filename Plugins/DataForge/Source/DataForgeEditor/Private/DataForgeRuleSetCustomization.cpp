#include "DataForgeRuleSetCustomization.h"

#include "DataForgeEditorService.h"
#include "DataForgeRuleSet.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DataForgeRuleSetCustomization"

TSharedRef<IDetailCustomization> FDataForgeRuleSetCustomization::MakeInstance()
{
	return MakeShared<FDataForgeRuleSetCustomization>();
}

void FDataForgeRuleSetCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}
	RuleSet = Cast<UDataForgeRuleSet>(Objects[0].Get());
	if (!RuleSet.IsValid())
	{
		return;
	}
	if (RuleSet->GetPackage() == GetTransientPackage())
	{
		return;
	}

	IDetailCategoryBuilder& Actions = DetailBuilder.EditCategory(TEXT("DataForge Actions"), LOCTEXT("Actions", "DataForge Actions"), ECategoryPriority::Important);
	Actions.AddCustomRow(LOCTEXT("ActionsSearch", "Probe Lint Preview Apply"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(LOCTEXT("Probe", "Probe"))
			.ToolTipText(LOCTEXT("ProbeTooltip", "Read columns and sample rows without changing content."))
			.OnClicked_Lambda([WeakRuleSet = RuleSet]()
			{
				if (WeakRuleSet.IsValid())
				{
					FDataForgeEditorService::Probe(*WeakRuleSet.Get());
				}
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(LOCTEXT("LintPreview", "Lint + Preview"))
			.ToolTipText(LOCTEXT("PreviewTooltip", "Compile rules and calculate a mutation-free diff."))
			.OnClicked_Lambda([WeakRuleSet = RuleSet]()
			{
				if (WeakRuleSet.IsValid())
				{
					FDataForgeEditorService::Preview(*WeakRuleSet.Get());
				}
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(LOCTEXT("Apply", "Apply Last Preview"))
			.ToolTipText(LOCTEXT("ApplyTooltip", "Apply only if the source and rules still match the last successful preview."))
			.OnClicked_Lambda([WeakRuleSet = RuleSet]()
			{
				if (WeakRuleSet.IsValid())
				{
					FDataForgeEditorService::Apply(*WeakRuleSet.Get());
				}
				return FReply::Handled();
			})
		]
	];

	Actions.AddCustomRow(LOCTEXT("Workflow", "Workflow"))
	.WholeRowContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("WorkflowText", "Workflow: Probe -> Primary Key / Output -> Asset Rules -> Generated Outputs -> Bindings -> Lint + Preview -> Apply"))
		.AutoWrapText(true)
	];
}

#undef LOCTEXT_NAMESPACE
