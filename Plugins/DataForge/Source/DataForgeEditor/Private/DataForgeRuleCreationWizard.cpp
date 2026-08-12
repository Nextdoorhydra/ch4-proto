#include "DataForgeRuleCreationWizard.h"

#include "DataForgePipeline.h"
#include "DataForgeEditorService.h"
#include "DataForgeRuleCreationWorkflow.h"
#include "DataForgeRuleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyEditorDelegates.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DataForgeRuleCreationWizard"

namespace DataForgeRuleCreationWizard
{
	class SWizard final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWizard) {}
			SLATE_ARGUMENT(UDataForgeRuleSet*, RuleSet)
			SLATE_ARGUMENT(TSharedPtr<SWindow>, OwnerWindow)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			OwnerWindow = Args._OwnerWindow;
			Workflow = MakeShared<FDataForgeRuleCreationWorkflow>(*Args._RuleSet);

			for (const FDataForgeSourceDescriptor& Descriptor : FDataForgeSourceAdapterRegistry::Get().DescribeAll())
			{
				AdapterOptions.Add(MakeShared<FDataForgeSourceDescriptor>(Descriptor));
			}

			FDetailsViewArgs DetailsArgs;
			DetailsArgs.bAllowSearch = true;
			DetailsArgs.bHideSelectionTip = true;
			DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
			DetailsView = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(DetailsArgs);
			DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SWizard::IsPropertyVisible));
			DetailsView->SetObject(&Workflow->GetDraft());
			DetailsView->OnFinishedChangingProperties().AddSP(this, &SWizard::OnDraftPropertyChanged);

			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 10.0f, 12.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(this, &SWizard::GetStepTitle)
					.Font(FAppStyle::GetFontStyle(TEXT("HeadingMedium")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 10.0f)
				[
					SNew(STextBlock).Text(this, &SWizard::GetGuidanceText).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("Adapter", "Source Adapter"))
						.Visibility(this, &SWizard::GetSourceVisibility)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SComboBox<TSharedPtr<FDataForgeSourceDescriptor>>)
						.OptionsSource(&AdapterOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<FDataForgeSourceDescriptor> Item)
						{
							return SNew(STextBlock).Text(Item.IsValid() ? Item->DisplayName : FText::GetEmpty());
						})
						.OnSelectionChanged(this, &SWizard::OnAdapterSelected)
						.Visibility(this, &SWizard::GetSourceVisibility)
						[
							SNew(STextBlock).Text(this, &SWizard::GetSelectedAdapterText)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(12.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("PrimaryKey", "Primary Key"))
						.Visibility(this, &SWizard::GetSchemaVisibility)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SAssignNew(PrimaryKeyCombo, SComboBox<TSharedPtr<FName>>)
						.OptionsSource(&PrimaryKeyOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<FName> Item)
						{
							return SNew(STextBlock).Text(Item.IsValid() ? FText::FromName(*Item) : FText::GetEmpty());
						})
						.OnSelectionChanged(this, &SWizard::OnPrimaryKeySelected)
						.Visibility(this, &SWizard::GetSchemaVisibility)
						[
							SNew(STextBlock).Text(this, &SWizard::GetSelectedPrimaryKeyText)
						]
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f)
				[
					SNew(SSplitter)
					+ SSplitter::Slot().Value(0.58f)
					[
						DetailsView.ToSharedRef()
					]
					+ SSplitter::Slot().Value(0.42f)
					[
						SNew(SBorder)
						.Padding(10.0f)
						[
							SNew(STextBlock).Text(this, &SWizard::GetInspectionText).AutoWrapText(true)
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 4.0f)
				[
					SNew(STextBlock).Text(this, &SWizard::GetStatusText).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 8.0f, 12.0f, 12.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton).Text(LOCTEXT("Back", "Back")).OnClicked(this, &SWizard::OnBack).IsEnabled(this, &SWizard::CanGoBack)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton).Text(this, &SWizard::GetActionText).OnClicked(this, &SWizard::OnStepAction).Visibility(this, &SWizard::GetActionVisibility)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton).Text(LOCTEXT("Cancel", "Cancel")).OnClicked(this, &SWizard::OnCancel)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton).Text(LOCTEXT("Next", "Next")).OnClicked(this, &SWizard::OnNext).Visibility(this, &SWizard::GetNextVisibility)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton).Text(LOCTEXT("Finish", "Finish")).OnClicked(this, &SWizard::OnFinish).Visibility(this, &SWizard::GetFinishVisibility)
					]
				]
			];
		}

	private:
		static int32 StepNumber(EDataForgeWizardStep Step)
		{
			return static_cast<int32>(Step) + 1;
		}

		FText GetStepTitle() const
		{
			static const TCHAR* Names[] = { TEXT("Source"), TEXT("Probe"), TEXT("Schema"), TEXT("Output"), TEXT("Bindings"), TEXT("Preview") };
			return FText::FromString(FString::Printf(TEXT("Step %d of 6 — %s"), StepNumber(Workflow->GetStep()), Names[static_cast<int32>(Workflow->GetStep())]));
		}

		FText GetGuidanceText() const
		{
			switch (Workflow->GetStep())
			{
			case EDataForgeWizardStep::Source: return LOCTEXT("SourceHelp", "Choose an adapter from the list. CSV/JSON use the file browser; Google Sheet Cache uses a GoogleSheetConfig Source Asset. Advanced adapter options stay folded.");
			case EDataForgeWizardStep::Probe: return LOCTEXT("ProbeHelp", "Read a bounded sample and normalize it into canonical Parsed Data. Probe never mutates project content.");
			case EDataForgeWizardStep::Schema: return LOCTEXT("SchemaHelp", "Probe inferred required columns and suggested a primary key. Review the suggestion; choose another detected field when needed.");
			case EDataForgeWizardStep::Output: return LOCTEXT("OutputHelp", "Select the DataTable row struct and choose a Content Browser destination. Creation, deletion, and save behavior are Advanced options.");
			case EDataForgeWizardStep::Bindings: return LOCTEXT("BindingsHelp", "Use exact-name Auto Map, then review the mappings. In Asset Rules, {ColumnName} is replaced with that row's value; names are case-sensitive and invalid tokens fail Preview.");
			case EDataForgeWizardStep::Preview: return LOCTEXT("PreviewHelp", "Compile and inspect the mutation-free desired-state plan. Finish commits only the RuleSet draft; it does not Apply content changes.");
			default: return FText::GetEmpty();
			}
		}

		FText GetInspectionText() const
		{
			const FDataForgeDataSet& DataSet = Workflow->GetProbedDataSet();
			FString Text = FString::Printf(TEXT("Canonical Parsed Data\nAdapter: %s\nColumns: %d\nSample Rows: %d\nRevision: %s"),
				*Workflow->GetDraft().Source.AdapterId.ToString(), DataSet.Columns.Num(), DataSet.Rows.Num(), *DataSet.SourceRevision.Left(12));
			if (!DataSet.Columns.IsEmpty())
			{
				Text += TEXT("\n\nFields\n");
				for (const FName Column : DataSet.Columns)
				{
					Text += TEXT("• ") + Column.ToString() + TEXT("\n");
				}
			}
			const FDataForgeApplyPlan& Plan = Workflow->GetPreviewPlan();
			if (!Plan.Rows.IsEmpty() || !Plan.ManagedAssets.IsEmpty())
			{
				Text += TEXT("\nPreview\n") + Plan.MakeSummary();
			}
			if (!DataSet.Rows.IsEmpty() && !Workflow->GetDraft().Bindings.IsEmpty())
			{
				Text += TEXT("\n\nBinding Conversions\n");
				for (const FDataForgeBindingRule& Binding : Workflow->GetDraft().Bindings)
				{
					const FString Source = Binding.Source == EDataForgeBindingSource::GeneratedOutput
						? Binding.SourceOutput.ToString()
						: Binding.SourceColumn.ToString();
					Text += FString::Printf(
						TEXT("• %s -> %s\n  %s\n"),
						*Source,
						*Binding.TargetProperty,
						*FDataForgeEditorService::AnalyzeBinding(Workflow->GetDraft(), Binding, DataSet).ToDisplayString());
				}
			}
			return FText::FromString(Text);
		}

		FText GetStatusText() const
		{
			return FText::FromString(StatusMessage.IsEmpty() ? Workflow->GetLastMessage() : StatusMessage);
		}

		FText GetSelectedAdapterText() const
		{
			const FName AdapterId = Workflow->GetDraft().Source.AdapterId;
			for (const TSharedPtr<FDataForgeSourceDescriptor>& Option : AdapterOptions)
			{
				if (Option.IsValid() && Option->AdapterId == AdapterId)
				{
					return Option->DisplayName;
				}
			}
			return FText::FromName(AdapterId);
		}

		FText GetSelectedPrimaryKeyText() const
		{
			const FName PrimaryKey = Workflow->GetDraft().Schema.PrimaryKey;
			return PrimaryKey.IsNone() ? LOCTEXT("SelectPrimaryKey", "Select a detected field") : FText::FromName(PrimaryKey);
		}

		FText GetActionText() const
		{
			switch (Workflow->GetStep())
			{
			case EDataForgeWizardStep::Probe: return LOCTEXT("RunProbe", "Run Probe");
			case EDataForgeWizardStep::Bindings: return LOCTEXT("AutoMap", "Auto Map Exact Names");
			case EDataForgeWizardStep::Preview: return LOCTEXT("RunPreview", "Run Preview");
			default: return FText::GetEmpty();
			}
		}

		EVisibility GetSourceVisibility() const { return Workflow->GetStep() == EDataForgeWizardStep::Source ? EVisibility::Visible : EVisibility::Collapsed; }
		EVisibility GetSchemaVisibility() const { return Workflow->GetStep() == EDataForgeWizardStep::Schema ? EVisibility::Visible : EVisibility::Collapsed; }
		EVisibility GetActionVisibility() const
		{
			const EDataForgeWizardStep Step = Workflow->GetStep();
			return Step == EDataForgeWizardStep::Probe || Step == EDataForgeWizardStep::Bindings || Step == EDataForgeWizardStep::Preview
				? EVisibility::Visible : EVisibility::Collapsed;
		}
		EVisibility GetNextVisibility() const { return Workflow->GetStep() == EDataForgeWizardStep::Preview ? EVisibility::Collapsed : EVisibility::Visible; }
		EVisibility GetFinishVisibility() const { return Workflow->GetStep() == EDataForgeWizardStep::Preview ? EVisibility::Visible : EVisibility::Collapsed; }
		bool CanGoBack() const { return Workflow->GetStep() != EDataForgeWizardStep::Source; }

		bool IsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const
		{
			const auto IsInSection = [&PropertyAndParent](FName Section)
			{
				if (PropertyAndParent.Property.GetFName() == Section)
				{
					return true;
				}
				return PropertyAndParent.ParentProperties.ContainsByPredicate([Section](const FProperty* Parent)
				{
					return Parent && Parent->GetFName() == Section;
				});
			};

			switch (Workflow->GetStep())
			{
			case EDataForgeWizardStep::Source:
				return IsInSection(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Source))
					&& PropertyAndParent.Property.GetFName() != GET_MEMBER_NAME_CHECKED(FDataForgeSourceConfig, AdapterId);
			case EDataForgeWizardStep::Schema:
				return IsInSection(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Schema))
					&& PropertyAndParent.Property.GetFName() != GET_MEMBER_NAME_CHECKED(FDataForgeSchemaRule, PrimaryKey);
			case EDataForgeWizardStep::Output:
				return IsInSection(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Output));
			case EDataForgeWizardStep::Bindings:
				return IsInSection(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Bindings))
					|| IsInSection(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, AssetRules))
					|| IsInSection(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, GeneratedOutputs));
			default:
				return false;
			}
		}

		void OnAdapterSelected(TSharedPtr<FDataForgeSourceDescriptor> Item, ESelectInfo::Type)
		{
			if (Item.IsValid())
			{
				Workflow->GetDraft().Source.AdapterId = Item->AdapterId;
				Workflow->NotifyDraftChanged(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Source));
				StatusMessage.Reset();
				DetailsView->ForceRefresh();
			}
		}

		void OnPrimaryKeySelected(TSharedPtr<FName> Item, ESelectInfo::Type)
		{
			if (Item.IsValid())
			{
				Workflow->GetDraft().Schema.PrimaryKey = *Item;
				Workflow->NotifyDraftChanged(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Schema));
				StatusMessage.Reset();
				DetailsView->ForceRefresh();
			}
		}

		void OnDraftPropertyChanged(const FPropertyChangedEvent& Event)
		{
			Workflow->NotifyDraftChanged(Event.MemberProperty ? Event.MemberProperty->GetFName() : NAME_None);
			StatusMessage.Reset();
		}

		FReply OnStepAction()
		{
			StatusMessage.Reset();
			switch (Workflow->GetStep())
			{
			case EDataForgeWizardStep::Probe:
				Workflow->Probe();
				PrimaryKeyOptions.Reset();
				for (const FName Column : Workflow->GetProbedDataSet().Columns)
				{
					PrimaryKeyOptions.Add(MakeShared<FName>(Column));
				}
				PrimaryKeyCombo->RefreshOptions();
				break;
			case EDataForgeWizardStep::Bindings:
				Workflow->AutoMapExactNames();
				DetailsView->ForceRefresh();
				break;
			case EDataForgeWizardStep::Preview:
				Workflow->Preview();
				break;
			default:
				break;
			}
			return FReply::Handled();
		}

		FReply OnNext()
		{
			if (!Workflow->Next(StatusMessage))
			{
				return FReply::Handled();
			}
			StatusMessage.Reset();
			DetailsView->ForceRefresh();
			return FReply::Handled();
		}

		FReply OnBack()
		{
			Workflow->Back();
			StatusMessage.Reset();
			DetailsView->ForceRefresh();
			return FReply::Handled();
		}

		FReply OnFinish()
		{
			if (Workflow->Finish(StatusMessage))
			{
				if (const TSharedPtr<SWindow> Window = OwnerWindow.Pin())
				{
					Window->RequestDestroyWindow();
				}
			}
			return FReply::Handled();
		}

		FReply OnCancel()
		{
			if (const TSharedPtr<SWindow> Window = OwnerWindow.Pin())
			{
				Window->RequestDestroyWindow();
			}
			return FReply::Handled();
		}

		TSharedPtr<FDataForgeRuleCreationWorkflow> Workflow;
		TWeakPtr<SWindow> OwnerWindow;
		TSharedPtr<IDetailsView> DetailsView;
		TArray<TSharedPtr<FDataForgeSourceDescriptor>> AdapterOptions;
		TArray<TSharedPtr<FName>> PrimaryKeyOptions;
		TSharedPtr<SComboBox<TSharedPtr<FName>>> PrimaryKeyCombo;
		FString StatusMessage;
	};
}

void OpenDataForgeRuleCreationWizard(UDataForgeRuleSet& RuleSet)
{
	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "DataForge Rule Creation Wizard"))
		.ClientSize(FVector2D(1100.0f, 760.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(false);
	Window->SetContent(SNew(DataForgeRuleCreationWizard::SWizard).RuleSet(&RuleSet).OwnerWindow(Window));
	FSlateApplication::Get().AddWindow(Window);
}

#undef LOCTEXT_NAMESPACE
