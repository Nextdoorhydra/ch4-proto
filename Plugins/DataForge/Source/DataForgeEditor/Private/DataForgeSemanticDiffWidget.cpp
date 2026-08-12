#include "DataForgeSemanticDiffWidget.h"

#include "DataForgeRuleSet.h"
#include "DataForgeRuleSetSemanticDiff.h"
#include "Styling/AppStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "DataForgeSemanticDiffWidget"

namespace DataForgeSemanticDiffWidget
{
	class SDiff final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDiff) {}
			SLATE_ARGUMENT(UDataForgeRuleSet*, RuleSet)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			RuleSet = Args._RuleSet;
			CaptureBaseline();
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton).Text(LOCTEXT("Refresh", "Refresh Diff")).OnClicked(this, &SDiff::Refresh)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton).Text(LOCTEXT("Capture", "Use Current as Baseline")).OnClicked(this, &SDiff::ResetBaseline)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(12.0f, 4.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("Meaning", "Semantic comparison is keyed by RuleId, OutputName, and binding target; array reorder alone is ignored."))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(10.0f, 4.0f)
				[
					SNew(STextBlock).Text(this, &SDiff::GetSummaryText).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
				]
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f)
				[
					SAssignNew(DiffList, SListView<TSharedPtr<FString>>)
					.ListItemsSource(&Rows)
					.OnGenerateRow_Lambda([](TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner)
					{
						return SNew(STableRow<TSharedPtr<FString>>, Owner)
						[
							SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString())).AutoWrapText(true)
						];
					})
				]
			];
			RefreshRows();
		}

	private:
		void CaptureBaseline()
		{
			if (const UDataForgeRuleSet* Current = RuleSet.Get())
			{
				const FName Name = MakeUniqueObjectName(GetTransientPackage(), UDataForgeRuleSet::StaticClass(), TEXT("DataForgeSemanticDiffBaseline"));
				Baseline.Reset(DuplicateObject<UDataForgeRuleSet>(Current, GetTransientPackage(), Name));
			}
		}

		FReply Refresh()
		{
			RefreshRows();
			return FReply::Handled();
		}

		FReply ResetBaseline()
		{
			CaptureBaseline();
			RefreshRows();
			return FReply::Handled();
		}

		void RefreshRows()
		{
			Rows.Reset();
			const UDataForgeRuleSet* Current = RuleSet.Get();
			if (Current && Baseline.IsValid())
			{
				for (const FDataForgeSemanticDiffEntry& Entry : FDataForgeRuleSetSemanticDiff::Compare(*Baseline.Get(), *Current))
				{
					Rows.Add(MakeShared<FString>(Entry.ToDisplayString()));
				}
			}
			if (DiffList.IsValid()) DiffList->RequestListRefresh();
		}

		FText GetSummaryText() const
		{
			return FText::FromString(Rows.IsEmpty()
				? TEXT("No semantic changes from the captured baseline.")
				: FString::Printf(TEXT("%d semantic change(s) from the captured baseline."), Rows.Num()));
		}

		TWeakObjectPtr<UDataForgeRuleSet> RuleSet;
		TStrongObjectPtr<UDataForgeRuleSet> Baseline;
		TArray<TSharedPtr<FString>> Rows;
		TSharedPtr<SListView<TSharedPtr<FString>>> DiffList;
	};
}

TSharedRef<SWidget> CreateDataForgeSemanticDiffWidget(UDataForgeRuleSet& RuleSet)
{
	return SNew(DataForgeSemanticDiffWidget::SDiff).RuleSet(&RuleSet);
}

#undef LOCTEXT_NAMESPACE
