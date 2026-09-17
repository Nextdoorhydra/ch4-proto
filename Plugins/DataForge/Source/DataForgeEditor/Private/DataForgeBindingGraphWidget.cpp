#include "DataForgeBindingGraphWidget.h"

#include "DataForgeBindingGraph.h"
#include "DataForgeEditorService.h"
#include "DataForgeRuleSet.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "DataForgeBindingGraphWidget"

namespace DataForgeBindingGraphWidget
{
	class FSourceDragDropOp final : public FDecoratedDragDropOp
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FSourceDragDropOp, FDecoratedDragDropOp)

		FDataForgeBindingGraphSource Source;

		static TSharedRef<FSourceDragDropOp> New(const FDataForgeBindingGraphSource& InSource)
		{
			TSharedRef<FSourceDragDropOp> Operation = MakeShared<FSourceDragDropOp>();
			Operation->Source = InSource;
			Operation->DefaultHoverText = FText::FromString(InSource.DisplayName);
			Operation->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
			Operation->Construct();
			return Operation;
		}
	};

	DECLARE_DELEGATE_TwoParams(FOnBindingDropped, const FDataForgeBindingGraphSource&, const FDataForgeBindingGraphTarget&)

	class SSourceRow final : public STableRow<TSharedPtr<FDataForgeBindingGraphSource>>
	{
	public:
		SLATE_BEGIN_ARGS(SSourceRow) {}
			SLATE_ARGUMENT(TSharedPtr<FDataForgeBindingGraphSource>, Source)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& Owner)
		{
			Source = Args._Source;
			STableRow::Construct(
				STableRow::FArguments().Padding(5.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Source.IsValid() ? Source->DisplayName : FString()))
				],
				Owner);
		}

		virtual FReply OnDragDetected(const FGeometry& Geometry, const FPointerEvent& PointerEvent) override
		{
			if (PointerEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && Source.IsValid())
			{
				return FReply::Handled().BeginDragDrop(FSourceDragDropOp::New(*Source));
			}
			return FReply::Unhandled();
		}

	private:
		TSharedPtr<FDataForgeBindingGraphSource> Source;
	};

	class STargetRow final : public STableRow<TSharedPtr<FDataForgeBindingGraphTarget>>
	{
	public:
		SLATE_BEGIN_ARGS(STargetRow) {}
			SLATE_ARGUMENT(TSharedPtr<FDataForgeBindingGraphTarget>, Target)
			SLATE_EVENT(FOnBindingDropped, OnBindingDropped)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& Owner)
		{
			Target = Args._Target;
			OnBindingDropped = Args._OnBindingDropped;
			const FString OwnerName = Target.IsValid() && Target->Target == EDataForgeBindingTarget::GeneratedOutput
				? Target->TargetOutput.ToString()
				: TEXT("row");
			const FString Label = Target.IsValid()
				? FString::Printf(TEXT("%s.%s  :  %s"), *OwnerName, *Target->PropertyPath, *Target->PropertyType)
				: FString();
			STableRow::Construct(
				STableRow::FArguments().Padding(5.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Label))
				],
				Owner);
		}

		virtual FReply OnDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent) override
		{
			const TSharedPtr<FSourceDragDropOp> Operation = DragDropEvent.GetOperationAs<FSourceDragDropOp>();
			if (!Operation.IsValid() || !Target.IsValid() || !OnBindingDropped.IsBound())
			{
				return FReply::Unhandled();
			}
			OnBindingDropped.Execute(Operation->Source, *Target);
			return FReply::Handled();
		}

	private:
		TSharedPtr<FDataForgeBindingGraphTarget> Target;
		FOnBindingDropped OnBindingDropped;
	};

	class SGraph final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SGraph) {}
			SLATE_ARGUMENT(UDataForgeRuleSet*, RuleSet)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			RuleSet = Args._RuleSet;
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton).Text(LOCTEXT("ProbeRefresh", "Probe + Refresh")).OnClicked(this, &SGraph::ProbeAndRefresh)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(12.0f, 4.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("Instructions", "Drag a source field/output onto a target property. Only Direct or Convertible connections are created."))
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f)
				[
					SNew(SSplitter)
					+ SSplitter::Slot().Value(0.35f)
					[
						SNew(SBorder).Padding(6.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
							[
								SNew(STextBlock).Text(LOCTEXT("Sources", "Sources")).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)
							[
								SAssignNew(SourceList, SListView<TSharedPtr<FDataForgeBindingGraphSource>>)
								.ListItemsSource(&Sources)
								.OnGenerateRow_Lambda([](TSharedPtr<FDataForgeBindingGraphSource> Source, const TSharedRef<STableViewBase>& Owner)
								{
									return SNew(SSourceRow, Owner).Source(Source);
								})
							]
						]
					]
					+ SSplitter::Slot().Value(0.65f)
					[
						SNew(SBorder).Padding(6.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
							[
								SNew(STextBlock).Text(LOCTEXT("Targets", "Targets")).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)
							[
								SAssignNew(TargetList, SListView<TSharedPtr<FDataForgeBindingGraphTarget>>)
								.ListItemsSource(&Targets)
								.OnGenerateRow_Lambda([this](TSharedPtr<FDataForgeBindingGraphTarget> Target, const TSharedRef<STableViewBase>& Owner)
								{
									return SNew(STargetRow, Owner)
										.Target(Target)
										.OnBindingDropped(FOnBindingDropped::CreateSP(this, &SGraph::Connect));
								})
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 4.0f, 12.0f, 10.0f)
				[
					SNew(STextBlock).Text(this, &SGraph::GetStatusText).AutoWrapText(true)
				]
			];
			RefreshItems();
		}

	private:
		FReply ProbeAndRefresh()
		{
			if (UDataForgeRuleSet* EditedRuleSet = RuleSet.Get())
			{
				Status = FDataForgeEditorService::Probe(*EditedRuleSet, &DataSet).Summary;
				RefreshItems();
			}
			return FReply::Handled();
		}

		void Connect(const FDataForgeBindingGraphSource& Source, const FDataForgeBindingGraphTarget& Target)
		{
			if (UDataForgeRuleSet* EditedRuleSet = RuleSet.Get())
			{
				FDataForgeBindingGraphModel::Connect(*EditedRuleSet, DataSet, Source, Target, Status);
			}
		}

		void RefreshItems()
		{
			Sources.Reset();
			Targets.Reset();
			if (const UDataForgeRuleSet* EditedRuleSet = RuleSet.Get())
			{
				for (const FDataForgeBindingGraphSource& Source : FDataForgeBindingGraphModel::BuildSources(*EditedRuleSet, DataSet))
				{
					Sources.Add(MakeShared<FDataForgeBindingGraphSource>(Source));
				}
				for (const FDataForgeBindingGraphTarget& Target : FDataForgeBindingGraphModel::BuildTargets(*EditedRuleSet))
				{
					Targets.Add(MakeShared<FDataForgeBindingGraphTarget>(Target));
				}
			}
			if (SourceList.IsValid()) SourceList->RequestListRefresh();
			if (TargetList.IsValid()) TargetList->RequestListRefresh();
		}

		FText GetStatusText() const
		{
			return FText::FromString(Status.IsEmpty() ? TEXT("Run Probe + Refresh to populate canonical source fields.") : Status);
		}

		TWeakObjectPtr<UDataForgeRuleSet> RuleSet;
		FDataForgeDataSet DataSet;
		FString Status;
		TArray<TSharedPtr<FDataForgeBindingGraphSource>> Sources;
		TArray<TSharedPtr<FDataForgeBindingGraphTarget>> Targets;
		TSharedPtr<SListView<TSharedPtr<FDataForgeBindingGraphSource>>> SourceList;
		TSharedPtr<SListView<TSharedPtr<FDataForgeBindingGraphTarget>>> TargetList;
	};
}

TSharedRef<SWidget> CreateDataForgeBindingGraphWidget(UDataForgeRuleSet& RuleSet)
{
	return SNew(DataForgeBindingGraphWidget::SGraph).RuleSet(&RuleSet);
}

#undef LOCTEXT_NAMESPACE
