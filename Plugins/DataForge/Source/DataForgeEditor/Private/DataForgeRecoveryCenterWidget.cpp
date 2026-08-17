#include "DataForgeRecoveryCenterWidget.h"

#include "DataForgeRenameRecovery.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "DataForgeRecoveryCenter"

namespace DataForgeRecoveryCenterWidget
{
	using FManifestPtr = TSharedPtr<FDataForgeRenameRecoveryManifest>;

	class SRecoveryCenter final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SRecoveryCenter) {}
			SLATE_ARGUMENT(TSharedPtr<SWindow>, OwnerWindow)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			OwnerWindow = Args._OwnerWindow;
			Refresh();
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 10.0f, 12.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title", "DataForge Recovery Center"))
					.Font(FAppStyle::GetFontStyle(TEXT("HeadingMedium")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 12.0f, 10.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Guidance", "Review recorded rename batches and restore assets to their original paths. Restore is enabled only when every recorded path is unambiguous and collision-free."))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(12.0f, 0.0f, 12.0f, 8.0f)
				[
					SNew(SSplitter)
					+ SSplitter::Slot().Value(0.42f)
					[
						SAssignNew(ListView, SListView<FManifestPtr>)
						.ListItemsSource(&Manifests)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SRecoveryCenter::GenerateRow)
						.OnSelectionChanged(this, &SRecoveryCenter::OnSelectionChanged)
					]
					+ SSplitter::Slot().Value(0.58f)
					[
						SNew(SBorder).Padding(10.0f)
						[
							SNew(STextBlock).Text(this, &SRecoveryCenter::GetDetailsText).AutoWrapText(true)
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(12.0f, 0.0f, 12.0f, 12.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton).Text(LOCTEXT("Refresh", "Refresh")).OnClicked(this, &SRecoveryCenter::OnRefresh)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Restore", "Restore Selected Batch"))
						.IsEnabled(this, &SRecoveryCenter::CanRestore)
						.OnClicked(this, &SRecoveryCenter::OnRestore)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton).Text(LOCTEXT("Close", "Close")).OnClicked(this, &SRecoveryCenter::OnClose)
					]
				]
			];
		}

	private:
		void Refresh()
		{
			Manifests.Reset();
			LoadDiagnostics.Reset();
			for (FDataForgeRenameRecoveryManifest& Manifest : FDataForgeRenameRecovery::LoadRecent(100, LoadDiagnostics))
			{
				Manifests.Add(MakeShared<FDataForgeRenameRecoveryManifest>(MoveTemp(Manifest)));
			}
			Selected.Reset();
			RestoreValidation = {};
			if (ListView.IsValid()) ListView->RequestListRefresh();
		}

		TSharedRef<ITableRow> GenerateRow(FManifestPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
		{
			return SNew(STableRow<FManifestPtr>, OwnerTable)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("[%s] %s (%d)"),
					*Item->State, *Item->CreatedUtc, Item->Candidates.Num())))
				.ToolTipText(FText::FromString(Item->Filename))
			];
		}

		void OnSelectionChanged(FManifestPtr Item, ESelectInfo::Type)
		{
			Selected = Item;
			RestoreValidation = Selected.IsValid()
				? FDataForgeRenameRecovery::ValidateRestore(*Selected)
				: FDataForgeResult{};
		}

		FText GetDetailsText() const
		{
			if (!Selected.IsValid())
			{
				return Manifests.IsEmpty()
					? LOCTEXT("Empty", "No DataForge rename recovery manifests were found.")
					: LOCTEXT("Select", "Select a recorded batch to inspect its paths and restore readiness.");
			}
			FString Text = FString::Printf(TEXT("State: %s\nCreated: %s\nUpdated: %s\nFile: %s\n\n%s"),
				*Selected->State, *Selected->CreatedUtc, *Selected->UpdatedUtc, *Selected->Filename, *Selected->Message);
			for (const FDataForgeRenameCandidate& Candidate : Selected->Candidates)
			{
				Text += TEXT("\n\n") + Candidate.AssetPath.ToString() + TEXT("\n  <- ") + Candidate.GetSuggestedObjectPath();
			}
			Text += TEXT("\n\nRestore validation: ") + RestoreValidation.Summary;
			for (const FDataForgeDiagnostic& Diagnostic : RestoreValidation.Diagnostics)
			{
				Text += FString::Printf(TEXT("\n%s: %s"), *Diagnostic.Code, *Diagnostic.Message);
			}
			return FText::FromString(Text);
		}

		bool CanRestore() const { return Selected.IsValid() && RestoreValidation.bSuccess; }

		FReply OnRefresh()
		{
			Refresh();
			return FReply::Handled();
		}

		FReply OnRestore()
		{
			if (!CanRestore()) return FReply::Handled();
			const FText Prompt = FText::Format(
				LOCTEXT("Confirm", "Restore this recorded batch to its original paths?\n\n{0}\n\nA separate restore manifest will be written before any asset moves."),
				FText::FromString(RestoreValidation.Summary));
			if (FMessageDialog::Open(EAppMsgType::YesNo, Prompt) != EAppReturnType::Yes) return FReply::Handled();
			const FDataForgeResult Result = FDataForgeRenameRecovery::Restore(*Selected);
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Result.Summary.IsEmpty()
				? TEXT("DataForge restore failed. Review the manifest diagnostics.") : Result.Summary));
			Refresh();
			return FReply::Handled();
		}

		FReply OnClose()
		{
			if (OwnerWindow.IsValid()) OwnerWindow.Pin()->RequestDestroyWindow();
			return FReply::Handled();
		}

		TWeakPtr<SWindow> OwnerWindow;
		TArray<FManifestPtr> Manifests;
		FManifestPtr Selected;
		TArray<FDataForgeDiagnostic> LoadDiagnostics;
		FDataForgeResult RestoreValidation;
		TSharedPtr<SListView<FManifestPtr>> ListView;
	};
}

void OpenDataForgeRecoveryCenter()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "DataForge Recovery Center"))
		.ClientSize(FVector2D(1000.0f, 620.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(false);
	Window->SetContent(SNew(DataForgeRecoveryCenterWidget::SRecoveryCenter).OwnerWindow(Window));
	FSlateApplication::Get().AddWindow(Window);
}

#undef LOCTEXT_NAMESPACE
