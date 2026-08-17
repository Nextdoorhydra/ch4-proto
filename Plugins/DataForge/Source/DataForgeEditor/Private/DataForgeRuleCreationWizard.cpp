#include "DataForgeRuleCreationWizard.h"

#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "DataForgeAssetLayoutAuthoring.h"
#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeBindingPreset.h"
#include "DataForgeNamingPolicy.h"
#include "DataForgePipeline.h"
#include "DataForgeEditorService.h"
#include "DataForgeRuleCreationWorkflow.h"
#include "DataForgeRuleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailsView.h"
#include "IContentBrowserSingleton.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyEditorDelegates.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"

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
			BindingPresetOutputFolder = Workflow->GetBindingPresetOutputFolder();
			const FDataForgeAutomaticSetupDefaults AutomaticDefaults = Workflow->GetAutomaticSetupDefaults();
			AutomaticAssetRoot = AutomaticDefaults.AssetSearchRoot;
			AutomaticGeneratedFolder = AutomaticDefaults.GeneratedOutputFolder;
			AutomaticDefinitionFolder = AutomaticDefaults.DefinitionFolder;
			AutomaticNamingPolicy = AutomaticDefaults.NamingPolicy;
			AutomaticOutputClass = AutomaticDefaults.GeneratedOutputClass;

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
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 8.0f)
				[
					SNew(SBorder)
					.Visibility(this, &SWizard::GetPrimaryKeyVisibility)
					.Padding(10.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PrimaryKey", "Primary Key"))
							.Font(FAppStyle::GetFontStyle(TEXT("HeadingSmall")))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 0.0f, 2.0f, 6.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PrimaryKeyHelp", "Choose the probed source column whose value uniquely identifies each row. This value is also the default subject key used by folder and naming association rules."))
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							SAssignNew(PrimaryKeyCombo, SComboBox<TSharedPtr<FName>>)
							.OptionsSource(&PrimaryKeyOptions)
							.OnGenerateWidget_Lambda([](TSharedPtr<FName> Item)
							{
								return SNew(STextBlock).Text(Item.IsValid() ? FText::FromName(*Item) : FText::GetEmpty());
							})
							.OnSelectionChanged(this, &SWizard::OnPrimaryKeySelected)
							[
								SNew(STextBlock).Text(this, &SWizard::GetSelectedPrimaryKeyText)
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 8.0f)
				[
					SNew(SBorder)
					.Visibility(this, &SWizard::GetSourceVisibility)
					.Padding(10.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AutomaticSetupTitle", "Automatic Setup (Recommended)"))
							.Font(FAppStyle::GetFontStyle(TEXT("HeadingSmall")))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 2.0f, 2.0f, 6.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AutomaticSetupHelp", "Configure the source above, then select the asset root and outputs below. Analyze infers the Primary Key, folder layout, slots, cardinality, associations, and exact-name bindings. Advanced steps remain available for review."))
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							MakeAutomaticPathRow(LOCTEXT("AutomaticAssetRoot", "Asset Search Root"), EAutomaticPathField::AssetRoot)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("AutomaticRowStruct", "DataTable Row Struct"))]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SStructPropertyEntryBox)
									.MetaStruct(FTableRowBase::StaticStruct())
									.AllowNone(false)
									.SelectedStruct(this, &SWizard::GetAutomaticRowStruct)
									.OnSetStruct(this, &SWizard::OnAutomaticRowStructSet)
								]
							]
							+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(4.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("AutomaticOutputClass", "Generated PDA/DA Class"))]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SClassPropertyEntryBox)
									.MetaClass(UDataAsset::StaticClass())
									.AllowAbstract(false)
									.AllowNone(false)
									.SelectedClass(this, &SWizard::GetAutomaticOutputClass)
									.OnSetClass(this, &SWizard::OnAutomaticOutputClassSet)
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("AutomaticNamingPolicy", "Naming Policy Override (Optional)"))]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SObjectPropertyEntryBox)
								.AllowedClass(UDataForgeNamingPolicy::StaticClass())
								.ObjectPath(this, &SWizard::GetAutomaticNamingPolicyPath)
								.OnObjectChanged(this, &SWizard::OnAutomaticNamingPolicySelected)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							MakeAutomaticPathRow(LOCTEXT("AutomaticDataTable", "Output DataTable Path"), EAutomaticPathField::DataTable)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							MakeAutomaticPathRow(LOCTEXT("AutomaticGeneratedFolder", "Generated PDA/DA Folder"), EAutomaticPathField::GeneratedOutput)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							MakeAutomaticPathRow(LOCTEXT("AutomaticDefinitionFolder", "Rule Definition Folder"), EAutomaticPathField::Definitions)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(2.0f, 8.0f, 2.0f, 2.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("AnalyzeAutomaticSetup", "Analyze & Build Draft"))
							.ToolTipText(LOCTEXT("AnalyzeAutomaticSetupTooltip", "Probe the source, scan the selected asset root, infer a safe configuration, and update only the transient Wizard draft."))
							.OnClicked(this, &SWizard::OnAnalyzeAutomaticSetup)
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 8.0f)
				[
					SNew(SBorder)
					.Visibility(this, &SWizard::GetAssetLayoutVisibility)
					.Padding(10.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("LayoutModeHint", "Select a Profile to generate reusable rules. Leave it empty to continue with Manual Asset Rules."))
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 6.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SObjectPropertyEntryBox)
								.AllowedClass(UDataForgeAssetLayoutProfile::StaticClass())
								.ObjectPath(this, &SWizard::GetSelectedProfilePath)
								.OnObjectChanged(this, &SWizard::OnProfileSelected)
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("UseManualLayout", "Use Manual"))
								.ToolTipText(LOCTEXT("UseManualLayoutTooltip", "Detach Profile provenance in the transient draft and preserve all concrete rules for manual editing."))
								.OnClicked(this, &SWizard::OnUseManualLayout)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							SAssignNew(LayoutParameterRows, SVerticalBox)
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 8.0f)
				[
					SNew(SBorder)
					.Visibility(this, &SWizard::GetBindingPresetVisibility)
					.Padding(10.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("BindingPresetHint", "Optional: select a Binding Preset to create its Managed Asset Rule, PDA/DA output, and safe row-reference binding together."))
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 6.0f)
						[
							SNew(SObjectPropertyEntryBox)
							.AllowedClass(UDataForgeBindingPreset::StaticClass())
							.ObjectPath(this, &SWizard::GetSelectedBindingPresetPath)
							.OnObjectChanged(this, &SWizard::OnBindingPresetSelected)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SEditableTextBox)
								.Text(this, &SWizard::GetBindingPresetOutputFolderText)
								.OnTextCommitted(this, &SWizard::OnBindingPresetOutputFolderCommitted)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SAssignNew(BindingPresetPathButton, SComboButton)
								.OnGetMenuContent(this, &SWizard::MakeBindingPresetPathPicker)
								.ButtonContent()[SNew(STextBlock).Text(LOCTEXT("BrowseBindingPresetFolder", "Browse"))]
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("ApplyBindingPreset", "Apply Preset"))
								.IsEnabled_Lambda([this]() { return Workflow->GetSelectedBindingPreset() != nullptr; })
								.OnClicked(this, &SWizard::OnApplyBindingPreset)
							]
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
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(STextBlock).Text(this, &SWizard::GetInspectionText).AutoWrapText(true)
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 4.0f)
				[
					SNew(STextBlock).Text(this, &SWizard::GetStatusText).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 4.0f)
				[
					SNew(SCheckBox)
					.Visibility(this, &SWizard::GetAutomaticReviewVisibility)
					.IsEnabled_Lambda([this]() { return Workflow->HasSuccessfulPreview(); })
					.IsChecked(this, &SWizard::GetAutomaticReviewCheckState)
					.OnCheckStateChanged(this, &SWizard::OnAutomaticReviewCheckStateChanged)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ApproveAutomaticReview", "I reviewed the inferred files, assignments, diagnostics, and Preview effects."))
						.AutoWrapText(true)
					]
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
						SNew(SButton).Text(LOCTEXT("Finish", "Finish & Apply")).OnClicked(this, &SWizard::OnFinish).Visibility(this, &SWizard::GetFinishVisibility)
					]
				]
			];
			RebuildLayoutParameterRows();
		}

	private:
		enum class EAutomaticPathField : uint8
		{
			AssetRoot,
			DataTable,
			GeneratedOutput,
			Definitions
		};

		static int32 StepNumber(EDataForgeWizardStep Step)
		{
			return static_cast<int32>(Step) + 1;
		}

		FText GetStepTitle() const
		{
			static const TCHAR* Names[] = { TEXT("Source"), TEXT("Probe"), TEXT("Schema"), TEXT("Output"), TEXT("Asset Layout"), TEXT("Asset Rules"), TEXT("Generated Outputs"), TEXT("Bindings"), TEXT("Preview") };
			return FText::FromString(FString::Printf(TEXT("Step %d of 9 - %s"), StepNumber(Workflow->GetStep()), Names[static_cast<int32>(Workflow->GetStep())]));
		}

		FText GetGuidanceText() const
		{
			switch (Workflow->GetStep())
			{
			case EDataForgeWizardStep::Source: return LOCTEXT("SourceHelp", "Choose an adapter from the list. CSV/JSON use the file browser; Google Sheet Cache uses a GoogleSheetConfig Source Asset. Advanced adapter options stay folded.");
			case EDataForgeWizardStep::Probe: return LOCTEXT("ProbeHelp", "Read a bounded sample and normalize it into canonical Parsed Data. Probe never mutates project content.");
			case EDataForgeWizardStep::Schema: return LOCTEXT("SchemaHelp", "Probe inferred required columns and suggested a primary key. Review the suggestion; choose another detected field when needed.");
			case EDataForgeWizardStep::Output: return LOCTEXT("OutputHelp", "Select the DataTable row struct and choose a Content Browser destination. Creation, deletion, and save behavior are Advanced options.");
			case EDataForgeWizardStep::AssetLayout: return LOCTEXT("AssetLayoutHelp", "Choose a central AssetLayoutProfile or continue manually. Profile parameters use ${Name}; source columns use {ColumnName}. Materialization changes only this transient draft until Finish & Apply.");
			case EDataForgeWizardStep::AssetRules: return LOCTEXT("AssetRulesHelp", "Apply an optional Binding Preset first, then review its Managed destination or define rules manually. Rule Ids are selected from dropdowns later; {ColumnName} tokens are case-sensitive.");
			case EDataForgeWizardStep::GeneratedOutputs: return LOCTEXT("GeneratedOutputsHelp", "Define PDA/DA outputs and optional Association Sources. Association Sources may use any adapter and expose normalized match, path, kind, and role columns.");
			case EDataForgeWizardStep::Bindings: return LOCTEXT("BindingsHelp", "Review inferred source-to-row, source-to-PDA/DA, and generated-object-to-row mappings. Add only exceptional mappings manually.");
			case EDataForgeWizardStep::Preview: return LOCTEXT("PreviewHelp", "Compile and inspect the mutation-free desired-state plan. Finish saves the RuleSet, runs a fresh Preview, and immediately Applies the generated content.");
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
			if (Workflow->GetStep() == EDataForgeWizardStep::AssetLayout)
			{
				Text += TEXT("\n\nAsset Layout\n");
				if (Workflow->IsManualAssetLayout())
				{
					Text += TEXT("Mode: Manual\nConcrete rules remain directly editable.");
				}
				else if (const UDataForgeAssetLayoutProfile* Profile = Workflow->GetSelectedAssetLayoutProfile())
				{
					Text += FString::Printf(TEXT("Profile: %s\nVersion: %d\n"), *Profile->GetName(), Profile->ProfileVersion);
					for (const TPair<FName, FString>& Parameter : Workflow->GetAssetLayoutParameterValues())
					{
						Text += FString::Printf(TEXT("%s = %s\n"), *Parameter.Key.ToString(), *Parameter.Value);
					}
					Text += TEXT("\nDraft Status: ") + FDataForgeAssetLayoutAuthoring::Analyze(Workflow->GetDraft()).MakeSummary();
				}
			}
			if (Workflow->GetStep() == EDataForgeWizardStep::AssetRules && Workflow->GetSelectedBindingPreset())
			{
				Text += FString::Printf(TEXT("\n\nBinding Preset\nPreset: %s\nGenerated Folder: %s"),
					*Workflow->GetSelectedBindingPreset()->GetName(), *BindingPresetOutputFolder);
			}
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
			if (Workflow->HasAutomaticSetup())
			{
				Text += TEXT("\n\n") + Workflow->GetAutomaticSetupInspection();
			}
			return FText::FromString(Text);
		}

		TSharedRef<SWidget> MakeAutomaticPathRow(const FText& Label, EAutomaticPathField Field)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.32f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Label)
				]
				+ SHorizontalBox::Slot().FillWidth(0.58f).Padding(4.0f, 0.0f)
				[
					SNew(SEditableTextBox)
					.Text_Lambda([this, Field]() { return FText::FromString(GetAutomaticPath(Field)); })
					.OnTextCommitted_Lambda([this, Field](const FText& Text, ETextCommit::Type) { SetAutomaticPath(Field, Text.ToString()); })
				]
				+ SHorizontalBox::Slot().FillWidth(0.1f)
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda([this, Field]() { return MakeAutomaticPathPicker(Field); })
					.ButtonContent()[SNew(STextBlock).Text(LOCTEXT("BrowseAutomaticPath", "Browse"))]
				];
		}

		FString GetAutomaticPath(EAutomaticPathField Field) const
		{
			switch (Field)
			{
			case EAutomaticPathField::AssetRoot: return AutomaticAssetRoot;
			case EAutomaticPathField::DataTable: return Workflow->GetDraft().Output.AssetPath;
			case EAutomaticPathField::GeneratedOutput: return AutomaticGeneratedFolder;
			case EAutomaticPathField::Definitions: return AutomaticDefinitionFolder;
			default: return FString();
			}
		}

		void SetAutomaticPath(EAutomaticPathField Field, const FString& Path)
		{
			switch (Field)
			{
			case EAutomaticPathField::AssetRoot: AutomaticAssetRoot = Path; break;
			case EAutomaticPathField::DataTable:
				Workflow->GetDraft().Output.AssetPath = Path;
				Workflow->NotifyDraftChanged(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Output));
				break;
			case EAutomaticPathField::GeneratedOutput: AutomaticGeneratedFolder = Path; break;
			case EAutomaticPathField::Definitions: AutomaticDefinitionFolder = Path; break;
			}
			StatusMessage.Reset();
		}

		TSharedRef<SWidget> MakeAutomaticPathPicker(EAutomaticPathField Field)
		{
			FString DefaultPath = GetAutomaticPath(Field);
			if (Field == EAutomaticPathField::DataTable && FPackageName::IsValidLongPackageName(DefaultPath))
			{
				DefaultPath = FPackageName::GetLongPackagePath(DefaultPath);
			}
			if (!FPackageName::IsValidLongPackageName(DefaultPath)) DefaultPath = TEXT("/Game");
			FPathPickerConfig Config;
			Config.DefaultPath = DefaultPath;
			Config.bAllowClassesFolder = false;
			Config.bAddDefaultPath = false;
			Config.bAllowContextMenu = false;
			Config.OnPathSelected = FOnPathSelected::CreateLambda([this, Field](const FString& Folder)
			{
				if (Field == EAutomaticPathField::DataTable)
				{
					const FString Current = GetAutomaticPath(Field);
					const FString AssetName = FPackageName::IsValidLongPackageName(Current)
						? FPackageName::GetLongPackageAssetName(Current) : TEXT("DT_DataForge");
					SetAutomaticPath(Field, Folder / AssetName);
				}
				else SetAutomaticPath(Field, Folder);
			});
			return SNew(SBox).WidthOverride(360.0f).HeightOverride(480.0f)
			[
				FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().CreatePathPicker(Config)
			];
		}

		const UScriptStruct* GetAutomaticRowStruct() const { return Workflow->GetDraft().Output.RowStruct; }
		void OnAutomaticRowStructSet(const UScriptStruct* Struct)
		{
			Workflow->GetDraft().Output.RowStruct = const_cast<UScriptStruct*>(Struct);
			Workflow->NotifyDraftChanged(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Output));
			StatusMessage.Reset();
		}
		const UClass* GetAutomaticOutputClass() const { return AutomaticOutputClass.Get(); }
		void OnAutomaticOutputClassSet(const UClass* Class) { AutomaticOutputClass = const_cast<UClass*>(Class); StatusMessage.Reset(); }
		FString GetAutomaticNamingPolicyPath() const { return AutomaticNamingPolicy.IsValid() ? AutomaticNamingPolicy->GetPathName() : FString(); }
		void OnAutomaticNamingPolicySelected(const FAssetData& AssetData)
		{
			AutomaticNamingPolicy = Cast<UDataForgeNamingPolicy>(AssetData.GetAsset());
			StatusMessage.Reset();
		}

		FReply OnAnalyzeAutomaticSetup()
		{
			const FDataForgeResult Result = Workflow->ConfigureAutomatically(
				AutomaticAssetRoot, AutomaticNamingPolicy.Get(), AutomaticOutputClass.Get(),
				AutomaticGeneratedFolder, AutomaticDefinitionFolder);
			StatusMessage = Result.Summary;
			if (Result.bSuccess)
			{
				PrimaryKeyOptions.Reset();
				for (const FName Column : Workflow->GetProbedDataSet().Columns) PrimaryKeyOptions.Add(MakeShared<FName>(Column));
				PrimaryKeyCombo->RefreshOptions();
				BindingPresetOutputFolder = AutomaticGeneratedFolder;
				DetailsView->ForceRefresh();
			}
			return FReply::Handled();
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
			case EDataForgeWizardStep::AssetLayout: return LOCTEXT("MaterializeProfile", "Materialize Profile");
			case EDataForgeWizardStep::Bindings: return LOCTEXT("AutoMap", "Auto Map Exact Names");
			case EDataForgeWizardStep::Preview: return LOCTEXT("RunPreview", "Run Preview");
			default: return FText::GetEmpty();
			}
		}

		EVisibility GetSourceVisibility() const { return Workflow->GetStep() == EDataForgeWizardStep::Source ? EVisibility::Visible : EVisibility::Collapsed; }
		EVisibility GetPrimaryKeyVisibility() const
		{
			const EDataForgeWizardStep Step = Workflow->GetStep();
			const bool bRelevantStep = Step == EDataForgeWizardStep::Source
				|| Step == EDataForgeWizardStep::Probe
				|| Step == EDataForgeWizardStep::Schema;
			return bRelevantStep && !Workflow->GetProbedDataSet().Columns.IsEmpty()
				? EVisibility::Visible : EVisibility::Collapsed;
		}
		EVisibility GetAssetLayoutVisibility() const { return Workflow->GetStep() == EDataForgeWizardStep::AssetLayout ? EVisibility::Visible : EVisibility::Collapsed; }
		EVisibility GetBindingPresetVisibility() const { return Workflow->GetStep() == EDataForgeWizardStep::AssetRules ? EVisibility::Visible : EVisibility::Collapsed; }
		EVisibility GetActionVisibility() const
		{
			const EDataForgeWizardStep Step = Workflow->GetStep();
			return Step == EDataForgeWizardStep::Probe
				|| (Step == EDataForgeWizardStep::AssetLayout && !Workflow->IsManualAssetLayout())
				|| Step == EDataForgeWizardStep::Bindings || Step == EDataForgeWizardStep::Preview
				? EVisibility::Visible : EVisibility::Collapsed;
		}
		EVisibility GetNextVisibility() const { return Workflow->GetStep() == EDataForgeWizardStep::Preview ? EVisibility::Collapsed : EVisibility::Visible; }
		EVisibility GetFinishVisibility() const { return Workflow->GetStep() == EDataForgeWizardStep::Preview ? EVisibility::Visible : EVisibility::Collapsed; }
		EVisibility GetAutomaticReviewVisibility() const
		{
			return Workflow->GetStep() == EDataForgeWizardStep::Preview && Workflow->HasAutomaticSetup()
				? EVisibility::Visible : EVisibility::Collapsed;
		}
		ECheckBoxState GetAutomaticReviewCheckState() const
		{
			return Workflow->IsAutomaticReviewApproved() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		void OnAutomaticReviewCheckStateChanged(ECheckBoxState State)
		{
			Workflow->SetAutomaticReviewApproved(State == ECheckBoxState::Checked);
			StatusMessage.Reset();
		}
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
			case EDataForgeWizardStep::AssetLayout:
				return false;
			case EDataForgeWizardStep::AssetRules:
				return IsInSection(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, AssetRules));
			case EDataForgeWizardStep::GeneratedOutputs:
				return IsInSection(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, AssociationSources))
					|| IsInSection(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, GeneratedOutputs));
			case EDataForgeWizardStep::Bindings:
				return IsInSection(GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Bindings));
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

		FString GetSelectedProfilePath() const
		{
			const UDataForgeAssetLayoutProfile* Profile = Workflow->GetSelectedAssetLayoutProfile();
			return Profile ? Profile->GetPathName() : FString();
		}

		void OnProfileSelected(const FAssetData& AssetData)
		{
			Workflow->SelectAssetLayoutProfile(Cast<UDataForgeAssetLayoutProfile>(AssetData.GetAsset()));
			StatusMessage.Reset();
			RebuildLayoutParameterRows();
		}

		FString GetSelectedBindingPresetPath() const
		{
			const UDataForgeBindingPreset* Preset = Workflow->GetSelectedBindingPreset();
			return Preset ? Preset->GetPathName() : FString();
		}

		void OnBindingPresetSelected(const FAssetData& AssetData)
		{
			Workflow->SelectBindingPreset(Cast<UDataForgeBindingPreset>(AssetData.GetAsset()));
			StatusMessage.Reset();
		}

		FText GetBindingPresetOutputFolderText() const
		{
			return FText::FromString(BindingPresetOutputFolder);
		}

		void OnBindingPresetOutputFolderCommitted(const FText& Text, ETextCommit::Type)
		{
			BindingPresetOutputFolder = Text.ToString();
			Workflow->SetBindingPresetOutputFolder(BindingPresetOutputFolder);
			StatusMessage.Reset();
		}

		TSharedRef<SWidget> MakeBindingPresetPathPicker()
		{
			FPathPickerConfig Config;
			Config.DefaultPath = FPackageName::IsValidLongPackageName(BindingPresetOutputFolder) ? BindingPresetOutputFolder : TEXT("/Game");
			Config.bAllowClassesFolder = false;
			Config.bAddDefaultPath = false;
			Config.bAllowContextMenu = false;
			Config.OnPathSelected = FOnPathSelected::CreateSP(this, &SWizard::OnBindingPresetOutputFolderSelected);
			return SNew(SBox).WidthOverride(360.0f).HeightOverride(480.0f)
			[
				FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().CreatePathPicker(Config)
			];
		}

		void OnBindingPresetOutputFolderSelected(const FString& Folder)
		{
			BindingPresetOutputFolder = Folder;
			Workflow->SetBindingPresetOutputFolder(Folder);
			StatusMessage.Reset();
			if (BindingPresetPathButton.IsValid()) BindingPresetPathButton->SetIsOpen(false);
		}

		FReply OnApplyBindingPreset()
		{
			Workflow->MaterializeBindingPreset();
			StatusMessage.Reset();
			DetailsView->ForceRefresh();
			return FReply::Handled();
		}

		FReply OnUseManualLayout()
		{
			Workflow->SelectAssetLayoutProfile(nullptr);
			StatusMessage.Reset();
			RebuildLayoutParameterRows();
			return FReply::Handled();
		}

		void RebuildLayoutParameterRows()
		{
			if (!LayoutParameterRows.IsValid()) return;
			LayoutParameterRows->ClearChildren();
			const UDataForgeAssetLayoutProfile* Profile = Workflow->GetSelectedAssetLayoutProfile();
			if (!Profile)
			{
				LayoutParameterRows->AddSlot().AutoHeight().Padding(2.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("ManualLayout", "Manual mode: configure concrete rules in the next step."))
				];
				return;
			}
			for (const FDataForgeProfileParameter& Parameter : Profile->Parameters)
			{
				const FString Value = Workflow->GetAssetLayoutParameterValues().FindRef(Parameter.Name);
				LayoutParameterRows->AddSlot().AutoHeight().Padding(2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Parameter.Name.ToString() + (Parameter.bRequired ? TEXT(" *") : TEXT(""))))
						.ToolTipText(FText::FromString(Parameter.Description))
					]
					+ SHorizontalBox::Slot().FillWidth(0.65f)
					[
						SNew(SEditableTextBox)
						.Text(FText::FromString(Value))
						.HintText(FText::FromString(Parameter.DefaultValue))
						.OnTextCommitted_Lambda([WeakWorkflow = TWeakPtr<FDataForgeRuleCreationWorkflow>(Workflow), Name = Parameter.Name](const FText& Text, ETextCommit::Type)
						{
							if (const TSharedPtr<FDataForgeRuleCreationWorkflow> Pinned = WeakWorkflow.Pin())
							{
								Pinned->SetAssetLayoutParameter(Name, Text.ToString());
							}
						})
					]
				];
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
			case EDataForgeWizardStep::AssetLayout:
				Workflow->MaterializeAssetLayout();
				DetailsView->ForceRefresh();
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
		TSharedPtr<SVerticalBox> LayoutParameterRows;
		TSharedPtr<SComboButton> BindingPresetPathButton;
		FString BindingPresetOutputFolder;
		FString AutomaticAssetRoot = TEXT("/Game");
		FString AutomaticGeneratedFolder;
		FString AutomaticDefinitionFolder;
		TWeakObjectPtr<UDataForgeNamingPolicy> AutomaticNamingPolicy;
		TWeakObjectPtr<UClass> AutomaticOutputClass;
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
