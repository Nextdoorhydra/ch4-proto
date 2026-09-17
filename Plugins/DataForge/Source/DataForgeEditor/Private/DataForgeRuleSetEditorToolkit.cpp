#include "DataForgeRuleSetEditorToolkit.h"

#include "DataForgeEditorService.h"
#include "DataForgeBindingGraphWidget.h"
#include "DataForgeDependencyGraphWidget.h"
#include "DataForgeRuleCreationWizard.h"
#include "DataForgeRuleSet.h"
#include "DataForgeRuleSetSnapshot.h"
#include "DataForgeSemanticDiffWidget.h"
#include "Engine/DataTable.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "Misc/MessageDialog.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "DataForgeRuleSetEditor"

const FName FDataForgeRuleSetEditorToolkit::MainTabId(TEXT("DataForgeRuleSetEditor.Main"));
const FName FDataForgeRuleSetEditorToolkit::BindingGraphTabId(TEXT("DataForgeRuleSetEditor.BindingGraph"));
const FName FDataForgeRuleSetEditorToolkit::DependencyGraphTabId(TEXT("DataForgeRuleSetEditor.DependencyGraph"));
const FName FDataForgeRuleSetEditorToolkit::SemanticDiffTabId(TEXT("DataForgeRuleSetEditor.SemanticDiff"));

namespace DataForgeRuleSetEditor
{
	class SSourceRow final : public SMultiColumnTableRow<TSharedPtr<FDataForgeRow>>
	{
	public:
		SLATE_BEGIN_ARGS(SSourceRow) {}
			SLATE_ARGUMENT(TSharedPtr<FDataForgeRow>, Row)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTable)
		{
			Row = Args._Row;
			SMultiColumnTableRow<TSharedPtr<FDataForgeRow>>::Construct(
				FSuperRowType::FArguments().Padding(2.0f),
				OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == TEXT("__SourceRow"))
			{
				return SNew(STextBlock).Text(FText::AsNumber(Row.IsValid() ? Row->SourceRow : INDEX_NONE));
			}
			return SNew(STextBlock).Text(FText::FromString(Row.IsValid() ? Row->Values.FindRef(ColumnName) : FString()));
		}

	private:
		TSharedPtr<FDataForgeRow> Row;
	};
}

void FDataForgeRuleSetEditorToolkit::InitEditor(
	UDataForgeRuleSet* InRuleSet,
	EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	RuleSet = InRuleSet;

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(DetailsArgs);
	DetailsView->SetObject(InRuleSet);
	DetailsView->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent& Event)
	{
		if (UDataForgeRuleSet* EditedRuleSet = RuleSet.Get())
		{
			if (Event.MemberProperty && Event.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDataForgeRuleSet, Source))
			{
				FDataForgeEditorService::InvalidateProbeCache(*EditedRuleSet);
			}
#if WITH_EDITORONLY_DATA
			EditedRuleSet->LastStatus = TEXT("Draft");
			EditedRuleSet->LastSummary = TEXT("Rules changed. Run Preview before Apply.");
#endif
		}
	});

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("DataForgeRuleSetEditorLayout_v2"))
		->AddArea(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split(
				FTabManager::NewStack()
					->SetHideTabWell(false)
					->AddTab(MainTabId, ETabState::OpenedTab)
					->AddTab(BindingGraphTabId, ETabState::ClosedTab)
					->AddTab(DependencyGraphTabId, ETabState::ClosedTab)
					->AddTab(SemanticDiffTabId, ETabState::ClosedTab)));

	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		TEXT("DataForgeRuleSetEditorApp"),
		Layout,
		true,
		true,
		InRuleSet);
}

void FDataForgeRuleSetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceCategory", "DataForge RuleSet Editor"));
	InTabManager->RegisterTabSpawner(MainTabId, FOnSpawnTab::CreateSP(this, &FDataForgeRuleSetEditorToolkit::SpawnMainTab))
		.SetDisplayName(LOCTEXT("MainTab", "RuleSet"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	InTabManager->RegisterTabSpawner(BindingGraphTabId, FOnSpawnTab::CreateSP(this, &FDataForgeRuleSetEditorToolkit::SpawnBindingGraphTab))
		.SetDisplayName(LOCTEXT("BindingGraphTab", "Binding Graph"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	InTabManager->RegisterTabSpawner(DependencyGraphTabId, FOnSpawnTab::CreateSP(this, &FDataForgeRuleSetEditorToolkit::SpawnDependencyGraphTab))
		.SetDisplayName(LOCTEXT("DependencyGraphTab", "Project Overview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	InTabManager->RegisterTabSpawner(SemanticDiffTabId, FOnSpawnTab::CreateSP(this, &FDataForgeRuleSetEditorToolkit::SpawnSemanticDiffTab))
		.SetDisplayName(LOCTEXT("SemanticDiffTab", "Semantic Diff"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FDataForgeRuleSetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(MainTabId);
	InTabManager->UnregisterTabSpawner(BindingGraphTabId);
	InTabManager->UnregisterTabSpawner(DependencyGraphTabId);
	InTabManager->UnregisterTabSpawner(SemanticDiffTabId);
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

FName FDataForgeRuleSetEditorToolkit::GetToolkitFName() const
{
	return TEXT("DataForgeRuleSetEditor");
}

FText FDataForgeRuleSetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "DataForge RuleSet Editor");
}

FString FDataForgeRuleSetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("DataForge ");
}

FLinearColor FDataForgeRuleSetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.15f, 0.55f, 1.0f, 1.0f);
}

TSharedRef<SDockTab> FDataForgeRuleSetEditorToolkit::SpawnMainTab(const FSpawnTabArgs& Args)
{
	SourceHeader = SNew(SHeaderRow);
	SourceList = SNew(SListView<TSharedPtr<FDataForgeRow>>)
		.ListItemsSource(&SourceRows)
		.HeaderRow(SourceHeader)
		.OnGenerateRow_Lambda([](TSharedPtr<FDataForgeRow> Row, const TSharedRef<STableViewBase>& Owner)
		{
			return SNew(DataForgeRuleSetEditor::SSourceRow, Owner).Row(Row);
		});

	PlanList = SNew(SListView<TSharedPtr<FString>>)
		.ListItemsSource(&PlanRows)
		.OnGenerateRow_Lambda([](TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner)
		{
			return SNew(STableRow<TSharedPtr<FString>>, Owner)
			[
				SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()))
			];
		});

	RefreshSourceRows();
	RefreshPlanRows();

	return SNew(SDockTab)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("Wizard", "Creation Wizard")).OnClicked(this, &FDataForgeRuleSetEditorToolkit::OpenCreationWizard)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("BindingGraph", "Binding Graph")).OnClicked(this, &FDataForgeRuleSetEditorToolkit::OpenBindingGraph)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("DependencyGraph", "Project Overview")).OnClicked(this, &FDataForgeRuleSetEditorToolkit::OpenDependencyGraph)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("SemanticDiff", "Semantic Diff")).OnClicked(this, &FDataForgeRuleSetEditorToolkit::OpenSemanticDiff)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("Probe", "Probe Source")).OnClicked(this, &FDataForgeRuleSetEditorToolkit::ProbeSource)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("AutoMap", "Auto Map Exact Names")).OnClicked(this, &FDataForgeRuleSetEditorToolkit::AutoMapExactNames)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("Preview", "Preview Dependency Graph")).OnClicked(this, &FDataForgeRuleSetEditorToolkit::Preview)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("Apply", "Apply Dependency Graph")).OnClicked(this, &FDataForgeRuleSetEditorToolkit::Apply)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("CleanupOrphans", "Cleanup Root Orphans")).OnClicked(this, &FDataForgeRuleSetEditorToolkit::CleanupOrphans)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(LOCTEXT("ExportSnapshots", "Export Snapshots")).OnClicked(this, &FDataForgeRuleSetEditorToolkit::ExportSnapshots)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f, 8.0f, 6.0f)
		[
			SNew(SBorder)
			.Padding(6.0f)
			[
				SNew(STextBlock).Text(this, &FDataForgeRuleSetEditorToolkit::GetStatusText).AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(4.0f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.45f)
			[
				DetailsView.ToSharedRef()
			]
			+ SSplitter::Slot().Value(0.55f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("SourcePreview", "Source Preview")).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
				]
				+ SVerticalBox::Slot().FillHeight(0.65f).Padding(4.0f)
				[
					SourceList.ToSharedRef()
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("PlanPreview", "Managed Asset Layout / Apply Plan")).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
				]
				+ SVerticalBox::Slot().FillHeight(0.35f).Padding(4.0f)
				[
					PlanList.ToSharedRef()
				]
			]
		]
	];
}

TSharedRef<SDockTab> FDataForgeRuleSetEditorToolkit::SpawnBindingGraphTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		RuleSet.IsValid()
			? CreateDataForgeBindingGraphWidget(*RuleSet.Get())
			: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(LOCTEXT("GraphUnavailable", "RuleSet unavailable")))
	];
}

TSharedRef<SDockTab> FDataForgeRuleSetEditorToolkit::SpawnSemanticDiffTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		RuleSet.IsValid()
			? CreateDataForgeSemanticDiffWidget(*RuleSet.Get())
			: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(LOCTEXT("DiffUnavailable", "RuleSet unavailable")))
	];
}

TSharedRef<SDockTab> FDataForgeRuleSetEditorToolkit::SpawnDependencyGraphTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		RuleSet.IsValid()
			? CreateDataForgeDependencyGraphWidget(*RuleSet.Get())
			: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(LOCTEXT("DependencyGraphUnavailable", "RuleSet unavailable")))
	];
}

FReply FDataForgeRuleSetEditorToolkit::OpenCreationWizard()
{
	if (UDataForgeRuleSet* EditedRuleSet = RuleSet.Get())
	{
		OpenDataForgeRuleCreationWizard(*EditedRuleSet);
	}
	return FReply::Handled();
}

FReply FDataForgeRuleSetEditorToolkit::OpenBindingGraph()
{
	GetTabManager()->TryInvokeTab(BindingGraphTabId);
	return FReply::Handled();
}

FReply FDataForgeRuleSetEditorToolkit::OpenDependencyGraph()
{
	GetTabManager()->TryInvokeTab(DependencyGraphTabId);
	return FReply::Handled();
}

FReply FDataForgeRuleSetEditorToolkit::OpenSemanticDiff()
{
	GetTabManager()->TryInvokeTab(SemanticDiffTabId);
	return FReply::Handled();
}

FReply FDataForgeRuleSetEditorToolkit::ProbeSource()
{
	if (UDataForgeRuleSet* EditedRuleSet = RuleSet.Get())
	{
		FDataForgeEditorService::Probe(*EditedRuleSet, &ProbedDataSet);
		RefreshSourceRows();
		DetailsView->ForceRefresh();
	}
	return FReply::Handled();
}

FReply FDataForgeRuleSetEditorToolkit::AutoMapExactNames()
{
	UDataForgeRuleSet* EditedRuleSet = RuleSet.Get();
	if (!EditedRuleSet || !EditedRuleSet->Output.RowStruct)
	{
		return FReply::Handled();
	}
	if (ProbedDataSet.Columns.IsEmpty())
	{
		FDataForgeEditorService::Probe(*EditedRuleSet, &ProbedDataSet);
		RefreshSourceRows();
	}

	FDataForgeEditorService::AutoMapExactNames(*EditedRuleSet, ProbedDataSet.Columns);
	DetailsView->ForceRefresh();
	return FReply::Handled();
}

FReply FDataForgeRuleSetEditorToolkit::Preview()
{
	if (UDataForgeRuleSet* EditedRuleSet = RuleSet.Get())
	{
		PreviewPlan = FDataForgeApplyPlan();
		FDataForgeEditorService::PreviewDependencyGraph(*EditedRuleSet, &PreviewPlan);
		RefreshPlanRows();
		DetailsView->ForceRefresh();
	}
	return FReply::Handled();
}

FReply FDataForgeRuleSetEditorToolkit::Apply()
{
	if (UDataForgeRuleSet* EditedRuleSet = RuleSet.Get())
	{
		FDataForgeEditorService::ApplyDependencyGraph(*EditedRuleSet);
		PreviewPlan = FDataForgeApplyPlan();
		RefreshPlanRows();
		DetailsView->ForceRefresh();
	}
	return FReply::Handled();
}

FReply FDataForgeRuleSetEditorToolkit::CleanupOrphans()
{
	UDataForgeRuleSet* EditedRuleSet = RuleSet.Get();
	if (!EditedRuleSet)
	{
		return FReply::Handled();
	}
	if (PreviewPlan.AssetOrphanCount <= 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoOrphans", "The current Preview contains no managed orphan assets."));
		return FReply::Handled();
	}

	const FText Confirmation = FText::Format(
		LOCTEXT("ConfirmCleanup", "Permanently delete {0} DataForge-managed orphan asset(s) from the root RuleSet?\n\nOwnership and Preview drift will be revalidated, and a recovery manifest will be written first. This operation does not delete external assets."),
		FText::AsNumber(PreviewPlan.AssetOrphanCount));
	if (FMessageDialog::Open(EAppMsgType::YesNo, Confirmation) == EAppReturnType::Yes)
	{
		FDataForgeEditorService::CleanupOrphans(*EditedRuleSet);
		PreviewPlan = FDataForgeApplyPlan();
		RefreshPlanRows();
		DetailsView->ForceRefresh();
	}
	return FReply::Handled();
}

FReply FDataForgeRuleSetEditorToolkit::ExportSnapshots()
{
	if (UDataForgeRuleSet* EditedRuleSet = RuleSet.Get())
	{
		const FDataForgeResult Result = FDataForgeRuleSetSnapshot::Export(*EditedRuleSet);
		FDataForgeEditorService::LogResult(*EditedRuleSet, Result, !Result.bSuccess);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Result.Summary));
	}
	return FReply::Handled();
}

void FDataForgeRuleSetEditorToolkit::RefreshSourceRows()
{
	SourceRows.Reset();
	for (const FDataForgeRow& Row : ProbedDataSet.Rows)
	{
		SourceRows.Add(MakeShared<FDataForgeRow>(Row));
	}
	if (SourceHeader.IsValid())
	{
		SourceHeader->ClearColumns();
		SourceHeader->AddColumn(SHeaderRow::Column(TEXT("__SourceRow")).DefaultLabel(LOCTEXT("SourceRow", "Source Row")).FixedWidth(70.0f));
		for (const FName Column : ProbedDataSet.Columns)
		{
			SourceHeader->AddColumn(SHeaderRow::Column(Column).DefaultLabel(FText::FromName(Column)).FillWidth(1.0f));
		}
	}
	if (SourceList.IsValid())
	{
		SourceList->RequestListRefresh();
	}
}

void FDataForgeRuleSetEditorToolkit::RefreshPlanRows()
{
	PlanRows.Reset();
	for (const FDataForgePlannedAsset& Asset : PreviewPlan.ManagedAssets)
	{
		const FString Change = StaticEnum<EDataForgeManagedAssetChange>()->GetNameStringByValue(static_cast<int64>(Asset.Change));
		const FString Path = Asset.Change == EDataForgeManagedAssetChange::Move
			? Asset.PreviousObjectPath + TEXT(" -> ") + Asset.ObjectPath
			: Asset.ObjectPath;
		PlanRows.Add(MakeShared<FString>(FString::Printf(TEXT("%-9s %s [%s / %s]"), *Change, *Path, *Asset.RecordId.ToString(), *Asset.OutputName.ToString())));
	}
	if (PreviewPlan.ManagedAssets.IsEmpty() && !PreviewPlan.Rows.IsEmpty())
	{
		PlanRows.Add(MakeShared<FString>(PreviewPlan.MakeSummary()));
	}
	if (PlanList.IsValid())
	{
		PlanList->RequestListRefresh();
	}
}

FText FDataForgeRuleSetEditorToolkit::GetStatusText() const
{
	const UDataForgeRuleSet* EditedRuleSet = RuleSet.Get();
	if (!EditedRuleSet)
	{
		return LOCTEXT("Unavailable", "RuleSet unavailable");
	}
#if WITH_EDITORONLY_DATA
	return FText::FromString(EditedRuleSet->LastStatus + TEXT(" — ") + EditedRuleSet->LastSummary);
#else
	return FText::GetEmpty();
#endif
}

#undef LOCTEXT_NAMESPACE
