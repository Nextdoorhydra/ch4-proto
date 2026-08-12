#include "DataForgeDependencyGraphWidget.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeDependencyGraph.h"
#include "DataForgeRuleSet.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "DataForgeDependencyGraphWidget"

namespace DataForgeDependencyGraphWidget
{
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
						SNew(SButton).Text(LOCTEXT("Refresh", "Refresh Project Overview")).OnClicked(this, &SGraph::Refresh)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(12.0f, 4.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("Meaning", "Current execution order is followed by every project RuleSet with its source, output, and dependencies."))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(10.0f, 4.0f)
				[
					SNew(STextBlock).Text(this, &SGraph::GetSummaryText).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
				]
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f)
				[
					SAssignNew(GraphList, SListView<TSharedPtr<FString>>)
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
		FReply Refresh()
		{
			RefreshRows();
			return FReply::Handled();
		}

		void RefreshRows()
		{
			Rows.Reset();
			Diagnostics.Reset();
			ProjectRuleCount = 0;
			const UDataForgeRuleSet* Current = RuleSet.Get();
			if (Current)
			{
				const TArray<const UDataForgeRuleSet*> Roots = { Current };
				TArray<const UDataForgeRuleSet*> Order;
				bValid = FDataForgeDependencyGraph::BuildExecutionOrder(Roots, Order, Diagnostics);
				if (bValid)
				{
					Rows.Add(MakeShared<FString>(TEXT("CURRENT EXECUTION ORDER")));
					for (int32 Index = 0; Index < Order.Num(); ++Index)
					{
						Rows.Add(MakeShared<FString>(FString::Printf(TEXT("%d. %s"), Index + 1, *Order[Index]->GetPathName())));
					}
				}
				else
				{
					for (const FDataForgeDiagnostic& Diagnostic : Diagnostics)
					{
						Rows.Add(MakeShared<FString>(FString::Printf(TEXT("%s: %s"), *Diagnostic.Code, *Diagnostic.Message)));
					}
				}
			}

			Rows.Add(MakeShared<FString>(TEXT("\nALL PROJECT RULESETS")));
			TArray<FAssetData> RuleSetAssets;
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get()
				.GetAssetsByClass(UDataForgeRuleSet::StaticClass()->GetClassPathName(), RuleSetAssets, true);
			RuleSetAssets.Sort([](const FAssetData& Left, const FAssetData& Right)
			{
				return Left.GetSoftObjectPath().ToString() < Right.GetSoftObjectPath().ToString();
			});
			for (const FAssetData& AssetData : RuleSetAssets)
			{
				const UDataForgeRuleSet* ProjectRule = Cast<UDataForgeRuleSet>(AssetData.GetAsset());
				if (!ProjectRule) continue;
				++ProjectRuleCount;
				FString Source = ProjectRule->Source.SourceAsset.IsNull()
					? ProjectRule->Source.File.FilePath
					: ProjectRule->Source.SourceAsset.ToSoftObjectPath().ToString();
				if (ProjectRule->Source.AdapterId == TEXT("MultiSource"))
				{
					TArray<FString> InputDescriptions;
					for (const FDataForgeSourceInput& Input : ProjectRule->Source.Inputs)
					{
						const FString Location = Input.SourceAsset.IsNull() ? Input.File.FilePath : Input.SourceAsset.ToSoftObjectPath().ToString();
						InputDescriptions.Add(FString::Printf(TEXT("%s:%s [%s]"), *Input.AdapterId.ToString(), *Location, *Input.JoinColumn.ToString()));
					}
					Source = FString::Join(InputDescriptions, TEXT(" | "));
				}
				if (Source.IsEmpty()) Source = TEXT("<not configured>");
				TArray<FString> DependencyPaths;
				for (const FDataForgeDependencyRule& Dependency : ProjectRule->Dependencies)
				{
					DependencyPaths.Add(Dependency.RuleSet.ToSoftObjectPath().ToString());
				}
				DependencyPaths.Sort();
				Rows.Add(MakeShared<FString>(FString::Printf(
					TEXT("%s\n  Adapter: %s\n  Source: %s\n  Output: %s\n  Dependencies: %s"),
					*ProjectRule->GetPathName(), *ProjectRule->Source.AdapterId.ToString(), *Source,
					ProjectRule->Output.AssetPath.IsEmpty() ? TEXT("<not configured>") : *ProjectRule->Output.AssetPath,
					DependencyPaths.IsEmpty() ? TEXT("None") : *FString::Join(DependencyPaths, TEXT(", ")))));
			}
			if (GraphList.IsValid()) GraphList->RequestListRefresh();
		}

		FText GetSummaryText() const
		{
			return FText::FromString(bValid
				? FString::Printf(TEXT("Valid current dependency graph. Tracking %d project RuleSet(s)."), ProjectRuleCount)
				: FString::Printf(TEXT("Invalid dependency graph: %d error(s)."), Diagnostics.Num()));
		}

		TWeakObjectPtr<UDataForgeRuleSet> RuleSet;
		bool bValid = false;
		int32 ProjectRuleCount = 0;
		TArray<FDataForgeDiagnostic> Diagnostics;
		TArray<TSharedPtr<FString>> Rows;
		TSharedPtr<SListView<TSharedPtr<FString>>> GraphList;
	};
}

TSharedRef<SWidget> CreateDataForgeDependencyGraphWidget(UDataForgeRuleSet& RuleSet)
{
	return SNew(DataForgeDependencyGraphWidget::SGraph).RuleSet(&RuleSet);
}

#undef LOCTEXT_NAMESPACE
