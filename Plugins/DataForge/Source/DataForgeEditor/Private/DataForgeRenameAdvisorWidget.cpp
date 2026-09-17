#include "DataForgeRenameAdvisorWidget.h"

#include "DataForgeRenameAdvisor.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DataForgeRenameAdvisor"

namespace DataForgeRenameAdvisorWidget
{
	using FCandidatePtr = TSharedPtr<FDataForgeRenameCandidate>;

	class SAdvisor final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SAdvisor) {}
			SLATE_ARGUMENT(TArray<FAssetData>, Assets)
			SLATE_ARGUMENT(TSharedPtr<SWindow>, OwnerWindow)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Assets = Args._Assets;
			OwnerWindow = Args._OwnerWindow;
			RefreshCandidates();
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 10.0f, 12.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title", "DataForge Rename Advisor"))
					.Font(FAppStyle::GetFontStyle(TEXT("HeadingMedium")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 8.0f)
				[
					SNew(STextBlock)
					.Text(this, &SAdvisor::GetScopeText)
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 10.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Guidance", "Candidates are inferred from source records, Binding Preset slots, folder recipes, and Naming Policy. DataForge never applies a candidate without confirmation."))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(12.0f, 0.0f, 12.0f, 8.0f)
				[
					SNew(SSplitter)
					+ SSplitter::Slot().Value(0.52f)
					[
						SAssignNew(ListView, SListView<FCandidatePtr>)
						.ListItemsSource(&Candidates)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SAdvisor::GenerateRow)
						.OnSelectionChanged(this, &SAdvisor::OnSelectionChanged)
					]
					+ SSplitter::Slot().Value(0.48f)
					[
						SNew(SBorder)
						.Padding(10.0f)
						[
							SNew(STextBlock)
							.Text(this, &SAdvisor::GetDetailsText)
							.AutoWrapText(true)
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(12.0f, 0.0f, 12.0f, 12.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Refresh", "Refresh"))
						.OnClicked(this, &SAdvisor::OnRefresh)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Apply", "Rename / Move Checked"))
						.IsEnabled(this, &SAdvisor::CanApply)
						.OnClicked(this, &SAdvisor::OnApply)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("Close", "Close"))
						.OnClicked(this, &SAdvisor::OnClose)
					]
				]
			];
		}

	private:
		void RefreshCandidates()
		{
			Candidates.Reset();
			CheckedCandidates.Reset();
			for (FDataForgeRenameCandidate& Candidate : FDataForgeRenameAdvisor::BuildCandidates(Assets))
			{
				Candidates.Add(MakeShared<FDataForgeRenameCandidate>(MoveTemp(Candidate)));
			}
			Selected.Reset();
			if (ListView.IsValid()) ListView->RequestListRefresh();
		}

		TSharedRef<ITableRow> GenerateRow(FCandidatePtr Item, const TSharedRef<STableViewBase>& OwnerTable)
		{
			const FString Prefix = Item->HasErrors() ? TEXT("[Blocked] ")
				: !Item->IsChange() ? TEXT("[Compliant] ")
				: Item->bRecommended ? TEXT("[Recommended] ") : TEXT("");
			return SNew(STableRow<FCandidatePtr>, OwnerTable)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SCheckBox)
					.IsEnabled(!Item->HasErrors() && Item->IsChange())
					.IsChecked(this, &SAdvisor::GetCandidateCheckState, Item)
					.OnCheckStateChanged(this, &SAdvisor::OnCandidateCheckStateChanged, Item)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Prefix + Item->GetSuggestedObjectPath()))
					.ToolTipText(FText::FromString(Item->Reason))
				]
			];
		}

		FString GetCandidateKey(const FDataForgeRenameCandidate& Candidate) const
		{
			return Candidate.AssetPath.ToString() + TEXT("|") + Candidate.GetSuggestedObjectPath()
				+ TEXT("|") + Candidate.RuleSetPath.ToString() + TEXT("|") + Candidate.SlotId.ToString()
				+ TEXT("|") + Candidate.RecordId.ToString();
		}

		ECheckBoxState GetCandidateCheckState(FCandidatePtr Item) const
		{
			return CheckedCandidates.Contains(GetCandidateKey(*Item)) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}

		void OnCandidateCheckStateChanged(ECheckBoxState State, FCandidatePtr Item)
		{
			const FString Key = GetCandidateKey(*Item);
			if (State == ECheckBoxState::Checked) CheckedCandidates.Add(Key);
			else CheckedCandidates.Remove(Key);
		}

		void OnSelectionChanged(FCandidatePtr Item, ESelectInfo::Type)
		{
			Selected = Item;
		}

		FText GetDetailsText() const
		{
			if (!Selected.IsValid())
			{
				return Candidates.IsEmpty()
					? LOCTEXT("NoCandidates", "No candidates were found. The asset must be inside a configured folder inventory and compatible with a Binding Preset slot.")
					: LOCTEXT("SelectCandidate", "Select a candidate to inspect its provenance and validation results.");
			}
			FString Text = Selected->Reason + TEXT("\n\nFrom:\n") + Selected->AssetPath.ToString()
				+ TEXT("\n\nTo:\n") + Selected->GetSuggestedObjectPath()
				+ FString::Printf(TEXT("\n\nMatch score: %d\nEvidence: %s"),
					Selected->MatchScore, *FString::Join(Selected->MatchEvidence, TEXT(", ")));
			for (const FDataForgeDiagnostic& Diagnostic : Selected->Diagnostics)
			{
				Text += FString::Printf(TEXT("\n\n%s: %s"), *Diagnostic.Code, *Diagnostic.Message);
			}
			return FText::FromString(Text);
		}

		bool CanApply() const
		{
			return !CheckedCandidates.IsEmpty();
		}

		FText GetScopeText() const
		{
			TSet<FSoftObjectPath> AssetsWithCandidates;
			for (const FCandidatePtr& Candidate : Candidates) AssetsWithCandidates.Add(Candidate->AssetPath);
			return FText::Format(
				LOCTEXT("Scope", "Audited assets: {0}  |  Candidates: {1}  |  No candidate: {2}"),
				FText::AsNumber(Assets.Num()),
				FText::AsNumber(Candidates.Num()),
				FText::AsNumber(FMath::Max(0, Assets.Num() - AssetsWithCandidates.Num())));
		}

		FReply OnRefresh()
		{
			RefreshCandidates();
			return FReply::Handled();
		}

		FReply OnApply()
		{
			if (!CanApply()) return FReply::Handled();
			TArray<FDataForgeRenameCandidate> Checked;
			for (const FCandidatePtr& Candidate : Candidates)
			{
				if (CheckedCandidates.Contains(GetCandidateKey(*Candidate))) Checked.Add(*Candidate);
			}
			const FText Prompt = FText::Format(
				LOCTEXT("Confirm", "Rename and move {0} checked asset(s)?\n\nEvery candidate will be revalidated before the batch starts. DataForge will reconcile dependent managed outputs after the rename."),
				FText::AsNumber(Checked.Num()));
			if (FMessageDialog::Open(EAppMsgType::YesNo, Prompt) != EAppReturnType::Yes) return FReply::Handled();

			const FDataForgeResult Result = FDataForgeRenameAdvisor::ApplyBatch(Checked);
			if (!Result.bSuccess)
			{
				FString Message = TEXT("DataForge could not apply the checked candidates.");
				if (!Result.Summary.IsEmpty()) Message += TEXT("\n\n") + Result.Summary;
				for (const FDataForgeDiagnostic& Diagnostic : Result.Diagnostics)
				{
					if (Diagnostic.Severity == EDataForgeSeverity::Error) Message += TEXT("\n") + Diagnostic.Code + TEXT(": ") + Diagnostic.Message;
				}
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
				RefreshCandidates();
				return FReply::Handled();
			}
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Result.Summary + TEXT(" DataForge reconciliation has been queued.")));
			if (OwnerWindow.IsValid()) OwnerWindow.Pin()->RequestDestroyWindow();
			return FReply::Handled();
		}

		FReply OnClose()
		{
			if (OwnerWindow.IsValid()) OwnerWindow.Pin()->RequestDestroyWindow();
			return FReply::Handled();
		}

		TArray<FAssetData> Assets;
		TWeakPtr<SWindow> OwnerWindow;
		TArray<FCandidatePtr> Candidates;
		TSet<FString> CheckedCandidates;
		FCandidatePtr Selected;
		TSharedPtr<SListView<FCandidatePtr>> ListView;
	};
}

void OpenDataForgeRenameAdvisor(const FAssetData& AssetData)
{
	OpenDataForgeRenameAdvisor(TArray<FAssetData>{ AssetData });
}

void OpenDataForgeRenameAdvisor(const TArray<FAssetData>& Assets)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "DataForge Rename Advisor"))
		.ClientSize(FVector2D(940.0f, 560.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(false);
	Window->SetContent(SNew(DataForgeRenameAdvisorWidget::SAdvisor).Assets(Assets).OwnerWindow(Window));
	FSlateApplication::Get().AddWindow(Window);
}

#undef LOCTEXT_NAMESPACE
