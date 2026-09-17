#include "SNKMLocalizationDashboard.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "IDesktopPlatform.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Internationalization/StringTableCoreFwd.h"
#include "Internationalization/Text.h"
#include "Internationalization/TextLocalizationManager.h"
#include "ITranslationEditor.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "NKMLocalizationPipelineRunner.h"
#include "NKMLocalizationPathConfigurator.h"
#include "NKMLocalizationSettings.h"
#include "NKMLocalizationSourceReader.h"
#include "NKMLocalizationSourceWriter.h"
#include "NKMLocalizationValidator.h"
#include "NKMGameplayTextCsvAuditor.h"
#include "NKMStringTableSynchronizer.h"
#include "NKMTextRef.h"
#include "NKMTextBindingMigrator.h"
#include "NKMTextBindingCoverageValidator.h"
#include "NKMTextBindingReconciler.h"
#include "NKMTranslationStatusAnalyzer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SNKMLocalizationDashboard"

namespace
{
	FString DiagnosticsToString(const FNKMLocalizationResult& Result)
	{
		TArray<FString> Lines;
		for (const FNKMLocalizationDiagnostic& Diagnostic : Result.Diagnostics)
		{
			Lines.Add(Diagnostic.ToString());
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	TSharedRef<SWidget> HeaderCell(const FText& Text, float Width)
	{
		return SNew(SBox).WidthOverride(Width).Padding(4.0f)[SNew(STextBlock).Text(Text)];
	}
}

void SNKMLocalizationDashboard::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(LOCTEXT("SourceTab", "Source")).OnClicked(this, &SNKMLocalizationDashboard::SelectTab, 0)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("TranslationTab", "Translation")).OnClicked(this, &SNKMLocalizationDashboard::SelectTab, 1)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f, 0.0f)
		[
			SAssignNew(TabSwitcher, SWidgetSwitcher)
			.WidgetIndex(ActiveTabIndex)
			+ SWidgetSwitcher::Slot()[BuildSourceTab()]
			+ SWidgetSwitcher::Slot()[BuildTranslationTab()]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(SBorder)
			.Padding(6.0f)
			[
				SAssignNew(StatusText, STextBlock)
				.Text(LOCTEXT("Ready", "Ready."))
				.AutoWrapText(true)
			]
		]
	];
	PreviousPreviewLanguage = FTextLocalizationManager::Get().GetConfiguredGameLocalizationPreviewLanguage();
	RefreshTranslationRows();
}

TSharedRef<SWidget> SNKMLocalizationDashboard::BuildSourceTab()
{
	const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
	const FString ExampleTableId = Settings->TableIdPrefix.IsEmpty() ? TEXT("Game.Items") : Settings->TableIdPrefix + TEXT(".Items");
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[SNew(STextBlock).Text(LOCTEXT("SourcePath", "Source"))]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[SAssignNew(SourcePathTextBox, SEditableTextBox).HintText(FText::FromString(Settings->GetNormalizedAuthoringSourceRoot() / TEXT("MainMenu.json")))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[SNew(SButton).Text(LOCTEXT("Browse", "Browse...")).OnClicked(this, &SNKMLocalizationDashboard::BrowseSource)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)[SNew(SButton).Text(LOCTEXT("Load", "Load")).OnClicked(this, &SNKMLocalizationDashboard::LoadSource)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[SNew(STextBlock).Text(LOCTEXT("DefaultTable", "CSV/Simple JSON Table"))]
			+ SHorizontalBox::Slot().FillWidth(0.35f)[SAssignNew(DefaultTableIdTextBox, SEditableTextBox).HintText(FText::FromString(ExampleTableId))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 6, 0)[SNew(STextBlock).Text(LOCTEXT("DefaultAsset", "Asset"))]
			+ SHorizontalBox::Slot().FillWidth(0.65f)[SAssignNew(DefaultAssetPathTextBox, SEditableTextBox).HintText(FText::FromString(Settings->MakeConventionalTablePackagePath(FName(*ExampleTableId))))]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[SNew(STextBlock).Text(LOCTEXT("BindingProfile", "Binding Profile"))]
			+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(BindingProfileTextBox, SEditableTextBox).MinDesiredWidth(130.0f).HintText(LOCTEXT("BindingProfileHint", "Items"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 6, 0)[SNew(STextBlock).Text(LOCTEXT("RecordCsv", "Record CSV (optional)"))]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[SAssignNew(RecordCsvPathTextBox, SEditableTextBox).HintText(LOCTEXT("RecordCsvHint", "Empty uses the profile's configured provider"))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SNew(SButton).Text(LOCTEXT("BrowseRecordCsv", "Browse...")) .OnClicked(this, &SNKMLocalizationDashboard::BrowseRecordCsv)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SNew(SButton).Text(LOCTEXT("ReconcileRecords", "Reconcile")) .OnClicked(this, &SNKMLocalizationDashboard::ReconcileBoundRecords)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SNew(SButton).Text(LOCTEXT("ValidateCoverage", "Coverage")) .OnClicked(this, &SNKMLocalizationDashboard::ValidateBindingCoverage)]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[HeaderCell(LOCTEXT("TableHeader", "Table"), 170.0f)]
			+ SHorizontalBox::Slot().AutoWidth()[HeaderCell(LOCTEXT("KeyHeader", "Key"), 300.0f)]
			+ SHorizontalBox::Slot().FillWidth(0.65f)[SNew(SBox).Padding(4.0f)[SNew(STextBlock).Text(LOCTEXT("SourceHeader", "Native Source"))]]
			+ SHorizontalBox::Slot().FillWidth(0.35f)[SNew(SBox).Padding(4.0f)[SNew(STextBlock).Text(LOCTEXT("CommentHeader", "Comment"))]]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SAssignNew(SourceList, SListView<TSharedPtr<FNKMSourceEditorRow>>)
			.ListItemsSource(&SourceRows)
			.SelectionMode(ESelectionMode::Multi)
			.OnGenerateRow(this, &SNKMLocalizationDashboard::GenerateSourceRow)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("AddEntry", "Add Entry")).OnClicked(this, &SNKMLocalizationDashboard::AddEntry)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SNew(SButton).Text(LOCTEXT("RemoveEntry", "Remove Selected")).OnClicked(this, &SNKMLocalizationDashboard::RemoveSelectedEntries)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(12, 0, 0, 0)[SNew(SButton).Text(LOCTEXT("Validate", "Validate")).OnClicked(this, &SNKMLocalizationDashboard::ValidateSource)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SNew(SButton).Text(LOCTEXT("PreviewDiff", "Preview Diff")).OnClicked(this, &SNKMLocalizationDashboard::PreviewDiff)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SNew(SButton).Text(LOCTEXT("Save", "Save JSON")).OnClicked(this, &SNKMLocalizationDashboard::SaveSource)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SNew(SButton).Text(LOCTEXT("Sync", "Save + Sync + Gather/Export")).OnClicked(this, &SNKMLocalizationDashboard::SyncSource)]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)[SNew(SButton).Text(LOCTEXT("MigrateBindings", "Migrate Bound Texts")).OnClicked(this, &SNKMLocalizationDashboard::MigrateBindingTexts)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[SNew(SButton).Text(LOCTEXT("ApplyPaths", "Apply Project Settings")).OnClicked(this, &SNKMLocalizationDashboard::ApplyPathSettings)]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("AuditCsv", "Audit Gameplay CSV...")).OnClicked(this, &SNKMLocalizationDashboard::AuditGameplayCsv)]
		];
}

TSharedRef<SWidget> SNKMLocalizationDashboard::BuildTranslationTab()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.14f)[SNew(SBox).Padding(4.0f)[SNew(STextBlock).Text(LOCTEXT("Culture", "Culture"))]]
			+ SHorizontalBox::Slot().FillWidth(0.22f)[SNew(SBox).Padding(4.0f)[SNew(STextBlock).Text(LOCTEXT("Progress", "Translation"))]]
			+ SHorizontalBox::Slot().FillWidth(0.25f)[SNew(SBox).Padding(4.0f)[SNew(STextBlock).Text(LOCTEXT("POStatus", "PO Issues"))]]
			+ SHorizontalBox::Slot().FillWidth(0.23f)[SNew(SBox).Padding(4.0f)[SNew(STextBlock).Text(LOCTEXT("LocResStatus", "LocRes"))]]
			+ SHorizontalBox::Slot().FillWidth(0.09f)[SNew(SBox).Padding(4.0f)[SNew(STextBlock).Text(LOCTEXT("EditorHeader", "Editor"))]]
			+ SHorizontalBox::Slot().FillWidth(0.07f)[SNew(SBox).Padding(4.0f)[SNew(STextBlock).Text(LOCTEXT("PreviewHeader", "Preview"))]]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SAssignNew(TranslationList, SListView<TSharedPtr<FNKMTranslationStatusRow>>)
			.ListItemsSource(&TranslationRows)
			.OnGenerateRow(this, &SNKMLocalizationDashboard::GenerateTranslationRow)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Refresh", "Refresh")).OnClicked_Lambda([this]() { RefreshTranslationRows(); return FReply::Handled(); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)[SNew(SButton).Text(LOCTEXT("GatherExport", "Gather + Export PO")).OnClicked(this, &SNKMLocalizationDashboard::RunPipeline, FString(TEXT("GatherExport")))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SNew(SButton).Text(LOCTEXT("ImportCompile", "Import + Compile")).OnClicked(this, &SNKMLocalizationDashboard::RunPipeline, FString(TEXT("ImportCompile")))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SNew(SButton).Text(LOCTEXT("Verify", "Verify")).OnClicked(this, &SNKMLocalizationDashboard::RunPipeline, FString(TEXT("Verify")))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(12, 0, 0, 0)[SNew(SButton).Text(LOCTEXT("RestoreGamePreview", "Restore Game Preview")).OnClicked(this, &SNKMLocalizationDashboard::RestoreCulture)]
		];
}

TSharedRef<ITableRow> SNKMLocalizationDashboard::GenerateSourceRow(
	TSharedPtr<FNKMSourceEditorRow> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FNKMSourceEditorRow>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(170.0f).Padding(4)[SNew(STextBlock).Text(FText::FromString(Item->TableId))]]
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(300.0f).Padding(2)[SNew(SEditableTextBox).Text(FText::FromString(Item->Key)).OnTextChanged_Lambda([this](const FText&) { bIsDirty = true; }).OnTextCommitted_Lambda([Item](const FText& Text, ETextCommit::Type) { Item->Key = Text.ToString(); })]]
		+ SHorizontalBox::Slot().FillWidth(0.65f)[SNew(SBox).Padding(2)[SNew(SEditableTextBox).Text(FText::FromString(Item->SourceString)).OnTextChanged_Lambda([this](const FText&) { bIsDirty = true; }).OnTextCommitted_Lambda([Item](const FText& Text, ETextCommit::Type) { Item->SourceString = Text.ToString(); })]]
		+ SHorizontalBox::Slot().FillWidth(0.35f)[SNew(SBox).Padding(2)[SNew(SEditableTextBox)
			.Text(FText::FromString(Item->MetaData.FindRef(TEXT("Comment"))))
			.OnTextChanged_Lambda([this](const FText&) { bIsDirty = true; })
			.OnTextCommitted_Lambda([Item](const FText& Text, ETextCommit::Type)
			{
				const FString Comment = Text.ToString();
				if (Comment.IsEmpty()) Item->MetaData.Remove(TEXT("Comment"));
				else Item->MetaData.Add(TEXT("Comment"), Comment);
			})]]
	];
}

TSharedRef<ITableRow> SNKMLocalizationDashboard::GenerateTranslationRow(
	TSharedPtr<FNKMTranslationStatusRow> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FNKMTranslationStatusRow>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.14f)[SNew(SBox).Padding(4)[SNew(STextBlock).Text(FText::FromString(Item->Culture))]]
		+ SHorizontalBox::Slot().FillWidth(0.22f)[SNew(SBox).Padding(4)[SNew(STextBlock).Text(FText::FromString(Item->ProgressStatus))]]
		+ SHorizontalBox::Slot().FillWidth(0.25f)[SNew(SBox).Padding(4)[SNew(STextBlock).Text(FText::FromString(Item->POStatus))]]
		+ SHorizontalBox::Slot().FillWidth(0.23f)[SNew(SBox).Padding(4)[SNew(STextBlock).Text(FText::FromString(Item->LocResStatus))]]
		+ SHorizontalBox::Slot().FillWidth(0.09f)[SNew(SBox).Padding(2)[SNew(SButton).Text(LOCTEXT("OpenTranslationEditor", "Edit")).OnClicked(this, &SNKMLocalizationDashboard::OpenTranslationEditor, Item->Culture)]]
		+ SHorizontalBox::Slot().FillWidth(0.07f)[SNew(SBox).Padding(2)[SNew(SButton).Text(LOCTEXT("PreviewCulture", "Preview")).OnClicked(this, &SNKMLocalizationDashboard::PreviewCulture, Item->Culture)]]
	];
}

FReply SNKMLocalizationDashboard::OpenTranslationEditor(FString Culture)
{
	const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
	const FString TargetRoot = FPaths::ProjectDir() / Settings->GetNormalizedLocalizationTargetRoot();
	const FString ManifestPath = TargetRoot / (Settings->LocalizationTargetName + TEXT(".manifest"));
	const FString NativeArchivePath = TargetRoot / Settings->NativeCulture / (Settings->LocalizationTargetName + TEXT(".archive"));
	const FString CultureArchivePath = TargetRoot / Culture / (Settings->LocalizationTargetName + TEXT(".archive"));
	if (IFileManager::Get().FileSize(*ManifestPath) <= 0
		|| IFileManager::Get().FileSize(*NativeArchivePath) <= 0
		|| IFileManager::Get().FileSize(*CultureArchivePath) <= 0)
	{
		SetStatus(FString::Printf(TEXT("Cannot edit '%s': run Gather + Export PO, then Import + Compile first."), *Culture));
		return FReply::Handled();
	}

	ITranslationEditor::OpenTranslationEditor(ManifestPath, NativeArchivePath, CultureArchivePath);
	SetStatus(FString::Printf(TEXT("Opened Translation Editor for %s culture '%s'."), *Settings->LocalizationTargetName, *Culture));
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::PreviewCulture(FString Culture)
{
	const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
	const FString LocResPath = FPaths::ProjectDir() / Settings->GetNormalizedLocalizationTargetRoot() / Culture / (Settings->LocalizationTargetName + TEXT(".locres"));
	if (IFileManager::Get().FileSize(*LocResPath) <= 0)
	{
		SetStatus(FString::Printf(TEXT("Cannot preview '%s': compile its %s LocRes first."), *Culture, *Settings->LocalizationTargetName));
		return FReply::Handled();
	}

	FTextLocalizationManager::Get().ConfigureGameLocalizationPreviewLanguage(Culture);
	FTextLocalizationManager::Get().EnableGameLocalizationPreview(Culture);

	FString Detail = FString::Printf(TEXT("Game localization preview: %s (Editor language unchanged)"), *Culture);
	if (!Document.Tables.IsEmpty() && !Document.Tables[0].Entries.IsEmpty())
	{
		const FNKMLocalizationTable& Table = Document.Tables[0];
		const FNKMLocalizationEntry& Entry = Table.Entries[0];
		FName RuntimeTableId;
		if (FNKMTextRef::ResolveTableId(FName(*Table.Id), RuntimeTableId))
		{
			const FText Preview = FText::FromStringTable(RuntimeTableId, Entry.Key, EStringTableLoadingPolicy::FindOrFullyLoad);
			Detail += FString::Printf(TEXT("\n%s::%s = %s"), *Table.Id, *Entry.Key, *Preview.ToString());
		}
	}
	SetStatus(Detail);
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::RestoreCulture()
{
	FTextLocalizationManager::Get().ConfigureGameLocalizationPreviewLanguage(PreviousPreviewLanguage);
	if (PreviousPreviewLanguage.IsEmpty())
	{
		FTextLocalizationManager::Get().DisableGameLocalizationPreview();
	}
	else
	{
		FTextLocalizationManager::Get().EnableGameLocalizationPreview(PreviousPreviewLanguage);
	}
	SetStatus(PreviousPreviewLanguage.IsEmpty()
		? TEXT("Game localization preview disabled. Editor language was not changed.")
		: FString::Printf(TEXT("Game localization preview restored: %s"), *PreviousPreviewLanguage));
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::SelectTab(const int32 TabIndex)
{
	ActiveTabIndex = TabIndex;
	TabSwitcher->SetActiveWidgetIndex(TabIndex);
	if (TabIndex == 1) RefreshTranslationRows();
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::BrowseSource()
{
	if (!ConfirmDiscardChanges()) return FReply::Handled();
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop) return FReply::Handled();
	TArray<FString> Files;
	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	if (Desktop->OpenFileDialog(ParentWindow, TEXT("Select NKM localization source"), FPaths::ProjectDir(), TEXT(""), TEXT("Localization source (*.json;*.csv)|*.json;*.csv"), EFileDialogFlags::None, Files) && !Files.IsEmpty())
	{
		SourcePathTextBox->SetText(FText::FromString(Files[0]));
		LoadCurrentSource(true);
	}
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::LoadSource()
{
	if (ConfirmDiscardChanges()) LoadCurrentSource(true);
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::ValidateSource()
{
	ApplyRowsToDocument();
	FNKMLocalizationResult Result;
	const bool bValid = FNKMLocalizationValidator::Validate(Document, Result);
	SetResultStatus(TEXT("Validate"), Result, bValid);
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::PreviewDiff()
{
	ApplyRowsToDocument();
	FNKMLocalizationResult Result;
	TArray<FNKMStringTableDiff> Diffs;
	const bool bSucceeded = FNKMStringTableSynchronizer::Preview(Document, Diffs, Result);
	FString Detail = bSucceeded ? TEXT("Preview Diff: Succeeded") : TEXT("Preview Diff: Failed");
	for (const FNKMStringTableDiff& Diff : Diffs)
	{
		Detail += FString::Printf(
			TEXT("\n%s: +%d ~%d -%d%s"),
			*Diff.TableId,
			Diff.Added,
			Diff.Updated,
			Diff.Removed,
			Diff.bNamespaceChanged ? TEXT(" namespace-change") : TEXT(""));
	}
	const FString Diagnostics = DiagnosticsToString(Result);
	if (!Diagnostics.IsEmpty()) Detail += TEXT("\n") + Diagnostics;
	SetStatus(Detail);
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::ApplyPathSettings()
{
	FNKMLocalizationResult Result;
	const bool bSucceeded = FNKMLocalizationPathConfigurator::Apply(Result);
	SetResultStatus(TEXT("Apply Path Settings"), Result, bSucceeded);
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::MigrateBindingTexts()
{
	FNKMLocalizationResult Result;
	const bool bSucceeded = FNKMTextBindingMigrator::MigrateConfiguredProfiles(false, Result);
	SetResultStatus(TEXT("Migrate Bound Texts"), Result, bSucceeded);
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::BrowseRecordCsv()
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop) return FReply::Handled();
	TArray<FString> Files;
	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	if (Desktop->OpenFileDialog(ParentWindow, TEXT("Select gameplay record CSV"), FPaths::ProjectDir(), TEXT(""), TEXT("CSV files (*.csv)|*.csv"), EFileDialogFlags::None, Files)
		&& !Files.IsEmpty())
	{
		RecordCsvPathTextBox->SetText(FText::FromString(Files[0]));
	}
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::ReconcileBoundRecords()
{
	ApplyRowsToDocument();
	FNKMLocalizationResult Result;
	if (Document.Target.IsEmpty() || Document.NativeCulture.IsEmpty() || Document.Tables.IsEmpty())
	{
		Result.AddError(TEXT("NKMLOC_BINDING_SOURCE_NOT_LOADED"), TEXT("Load the binding profile's authoritative JSON before reconciling records."));
		SetResultStatus(TEXT("Reconcile Bound Records"), Result, false);
		return FReply::Handled();
	}
	const FName ProfileName(*BindingProfileTextBox->GetText().ToString().TrimStartAndEnd());
	const FNKMTextBindingProfile* Profile = GetDefault<UNKMLocalizationSettings>()->FindTextBindingProfile(ProfileName);
	if (!Profile)
	{
		Result.AddError(TEXT("NKMLOC_BINDING_PROFILE"), FString::Printf(TEXT("Unknown binding profile '%s'."), *ProfileName.ToString()));
		SetResultStatus(TEXT("Reconcile Bound Records"), Result, false);
		return FReply::Handled();
	}

	TUniquePtr<INKMLocalizationRecordProvider> Provider = FNKMLocalizationRecordProviderFactory::Create(
		*Profile,
		RecordCsvPathTextBox->GetText().ToString(),
		Result);
	FNKMTextBindingReconcileReport Report;
	const bool bSucceeded = Provider
		&& FNKMTextBindingReconciler::Reconcile(*Profile, *Provider, Document, Report, Result);
	if (bSucceeded)
	{
		RebuildSourceRows();
		bIsDirty = bIsDirty || !Report.AddedKeys.IsEmpty() || !Report.UpdatedMetadataKeys.IsEmpty();
		Result.AddWarning(
			TEXT("NKMLOC_BINDING_SUMMARY"),
			FString::Printf(TEXT("records=%d added=%d metadata=%d orphans=%d"), Report.RecordCount, Report.AddedKeys.Num(), Report.UpdatedMetadataKeys.Num(), Report.OrphanKeys.Num()),
			Profile->TableId.ToString());
	}
	SetResultStatus(TEXT("Reconcile Bound Records"), Result, bSucceeded);
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::ValidateBindingCoverage()
{
	ApplyRowsToDocument();
	FNKMLocalizationResult Result;
	const FName ProfileName(*BindingProfileTextBox->GetText().ToString().TrimStartAndEnd());
	const FNKMTextBindingProfile* Profile = GetDefault<UNKMLocalizationSettings>()->FindTextBindingProfile(ProfileName);
	if (!Profile)
	{
		Result.AddError(TEXT("NKMLOC_BINDING_PROFILE"), FString::Printf(TEXT("Unknown binding profile '%s'."), *ProfileName.ToString()));
		SetResultStatus(TEXT("Binding Coverage"), Result, false);
		return FReply::Handled();
	}

	TUniquePtr<INKMLocalizationRecordProvider> Provider = FNKMLocalizationRecordProviderFactory::Create(
		*Profile,
		RecordCsvPathTextBox->GetText().ToString(),
		Result);
	FNKMTextBindingCoverageReport Report;
	const bool bSucceeded = Provider
		&& FNKMTextBindingCoverageValidator::Validate(*Profile, *Provider, Document, Report, Result);
	SetResultStatus(TEXT("Binding Coverage"), Result, bSucceeded);
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::SaveSource() { SaveCurrentSource(); return FReply::Handled(); }

FReply SNKMLocalizationDashboard::SyncSource()
{
	FNKMLocalizationResult Result;
	if (!FNKMLocalizationPathConfigurator::Apply(Result))
	{
		SetResultStatus(TEXT("Apply Path Settings"), Result, false);
		return FReply::Handled();
	}
	TArray<FNKMStringTableDiff> Diffs;
	ApplyRowsToDocument();
	if (!FNKMStringTableSynchronizer::Preview(Document, Diffs, Result))
	{
		SetResultStatus(TEXT("Preview before Sync"), Result, false);
		return FReply::Handled();
	}

	int32 RemovedCount = 0;
	for (const FNKMStringTableDiff& Diff : Diffs) RemovedCount += Diff.Removed;
	if (RemovedCount > 0
		&& FMessageDialog::Open(
			EAppMsgType::YesNo,
			FText::Format(LOCTEXT("ConfirmRemovedKeys", "This sync removes {0} String Table key(s). Continue?"), RemovedCount)) != EAppReturnType::Yes)
	{
		SetStatus(TEXT("Sync cancelled before saving or changing assets."));
		return FReply::Handled();
	}
	if (bPipelineRunning)
	{
		SetStatus(TEXT("A localization pipeline operation is already running."));
		return FReply::Handled();
	}
	TGuardValue<bool> RunningGuard(bPipelineRunning, true);
	FScopedSlowTask SlowTask(3.0f, LOCTEXT("SyncPipelineProgress", "Saving source, syncing String Tables, and gathering localization..."));
	SlowTask.MakeDialog(false);

	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SaveSourceProgress", "Saving canonical source..."));
	if (!SaveCurrentSource()) return FReply::Handled();
	Diffs.Reset();
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SyncAssetProgress", "Synchronizing String Table Assets..."));
	bool bSucceeded = FNKMTextBindingCoverageValidator::ValidateConfiguredProfiles(Result)
		&& FNKMStringTableSynchronizer::Sync(Document, Diffs, Result);
	if (bSucceeded)
	{
		SlowTask.EnterProgressFrame(1.0f, LOCTEXT("GatherProgress", "Gathering and exporting PO files..."));
		FNKMLocalizationPipelineReport Report;
		bSucceeded = FNKMLocalizationPipelineRunner::Run(TEXT("GatherExport"), false, Result, Report);
	}
	SetResultStatus(TEXT("Save + Sync + Gather/Export"), Result, bSucceeded);
	RefreshTranslationRows();
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::AddEntry()
{
	if (Document.Tables.IsEmpty())
	{
		SetStatus(TEXT("Load a valid source document before adding entries."));
		return FReply::Handled();
	}
	const FName TableId = GetExplicitDefaultTableId();
	if (TableId.IsNone())
	{
		SetStatus(TEXT("Choose a valid CSV/Simple JSON Table before adding an entry to a multi-table document."));
		return FReply::Handled();
	}
	SourceRows.Add(MakeShared<FNKMSourceEditorRow>(FNKMSourceEditorRow{TableId.ToString(), TEXT("New.Entry"), TEXT("새 텍스트"), {}}));
	bIsDirty = true;
	SourceList->RequestListRefresh();
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::RemoveSelectedEntries()
{
	if (!SourceList->GetSelectedItems().IsEmpty()) bIsDirty = true;
	for (const TSharedPtr<FNKMSourceEditorRow>& Selected : SourceList->GetSelectedItems()) SourceRows.Remove(Selected);
	SourceList->ClearSelection();
	SourceList->RequestListRefresh();
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::AuditGameplayCsv()
{
	if (Document.Tables.IsEmpty())
	{
		SetStatus(TEXT("Load the localization source before auditing gameplay CSV."));
		return FReply::Handled();
	}
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop) return FReply::Handled();
	TArray<FString> Files;
	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	if (!Desktop->OpenFileDialog(ParentWindow, TEXT("Select gameplay CSV"), FPaths::ProjectDir(), TEXT(""), TEXT("CSV (*.csv)|*.csv"), EFileDialogFlags::None, Files) || Files.IsEmpty()) return FReply::Handled();

	ApplyRowsToDocument();
	FNKMLocalizationResult Result;
	FNKMGameplayTextCsvAuditReport Audit;
	const FName DefaultTableId = GetExplicitDefaultTableId();
	if (Document.Tables.Num() > 1 && DefaultTableId.IsNone())
	{
		SetStatus(TEXT("Gameplay CSV audit requires an explicit default table for key-only references in a multi-table document."));
		return FReply::Handled();
	}
	const bool bSucceeded = FNKMGameplayTextCsvAuditor::AuditFile(Files[0], Document, DefaultTableId, {}, Audit, Result);
	FString Detail = FString::Printf(TEXT("Audit %s; checked %d reference(s); %d issue(s)."), bSucceeded ? TEXT("succeeded") : TEXT("failed"), Audit.CheckedReferenceCount, Audit.Issues.Num());
	for (const FNKMGameplayTextCsvIssue& Issue : Audit.Issues)
	{
		Detail += FString::Printf(TEXT("\n%s row %d, %s='%s': %s"), *Issue.Code, Issue.Row, *Issue.Column, *Issue.Value, *Issue.Message);
	}
	if (!DiagnosticsToString(Result).IsEmpty()) Detail += TEXT("\n") + DiagnosticsToString(Result);
	SetStatus(Detail);
	return FReply::Handled();
}

FReply SNKMLocalizationDashboard::RunPipeline(FString Mode)
{
	if (bPipelineRunning)
	{
		SetStatus(TEXT("A localization pipeline operation is already running."));
		return FReply::Handled();
	}
	TGuardValue<bool> RunningGuard(bPipelineRunning, true);
	FScopedSlowTask SlowTask(1.0f, FText::Format(LOCTEXT("PipelineProgress", "Running NKM localization: {0}"), FText::FromString(Mode)));
	SlowTask.MakeDialog(false);
	SlowTask.EnterProgressFrame(1.0f);
	FNKMLocalizationResult Result;
	FNKMLocalizationPipelineReport Report;
	const bool bNeedsCoverage = Mode.Equals(TEXT("GatherExport"), ESearchCase::IgnoreCase)
		|| Mode.Equals(TEXT("Verify"), ESearchCase::IgnoreCase);
	const bool bSucceeded = FNKMLocalizationPathConfigurator::Apply(Result)
		&& (!bNeedsCoverage || FNKMTextBindingCoverageValidator::ValidateConfiguredProfiles(Result))
		&& FNKMLocalizationPipelineRunner::Run(Mode, false, Result, Report);
	SetResultStatus(Mode, Result, bSucceeded);
	RefreshTranslationRows();
	return FReply::Handled();
}

bool SNKMLocalizationDashboard::LoadCurrentSource(const bool bRequireValid)
{
	SelectedSourcePath = GetSourcePath();
	if (SelectedSourcePath.IsEmpty())
	{
		SetStatus(TEXT("Choose a source JSON or CSV file."));
		return false;
	}
	FNKMLocalizationSourceContext Context;
	Context.NativeCulture = GetDefault<UNKMLocalizationSettings>()->NativeCulture;
	Context.TableId = DefaultTableIdTextBox->GetText().ToString();
	Context.AssetPath = DefaultAssetPathTextBox->GetText().ToString();
	FNKMLocalizationResult Result;
	bool bSucceeded = FNKMLocalizationSourceReader::LoadFile(SelectedSourcePath, Context, Document, Result);
	if (bSucceeded && bRequireValid) bSucceeded = FNKMLocalizationValidator::Validate(Document, Result);
	if (bSucceeded) RebuildSourceRows();
	if (bSucceeded) bIsDirty = false;
	SetResultStatus(TEXT("Load"), Result, bSucceeded);
	return bSucceeded;
}

bool SNKMLocalizationDashboard::SaveCurrentSource()
{
	SelectedSourcePath = GetSourcePath();
	if (!FPaths::GetExtension(SelectedSourcePath).Equals(TEXT("json"), ESearchCase::IgnoreCase))
	{
		SetStatus(TEXT("Grid saving is supported for structured JSON only. CSV remains an import format."));
		return false;
	}
	ApplyRowsToDocument();
	FNKMLocalizationResult Result;
	const bool bValid = FNKMLocalizationValidator::Validate(Document, Result);
	const bool bSaved = bValid && FNKMLocalizationSourceWriter::SaveStructuredJson(SelectedSourcePath, Document, Result);
	if (bSaved) bIsDirty = false;
	SetResultStatus(TEXT("Save"), Result, bSaved);
	return bSaved;
}

void SNKMLocalizationDashboard::RebuildSourceRows()
{
	SourceRows.Reset();
	for (const FNKMLocalizationTable& Table : Document.Tables)
	{
		for (const FNKMLocalizationEntry& Entry : Table.Entries)
		{
			SourceRows.Add(MakeShared<FNKMSourceEditorRow>(FNKMSourceEditorRow{Table.Id, Entry.Key, Entry.SourceString, Entry.MetaData}));
		}
	}
	SourceRows.Sort([](const TSharedPtr<FNKMSourceEditorRow>& Left, const TSharedPtr<FNKMSourceEditorRow>& Right)
	{
		return Left->TableId == Right->TableId ? Left->Key < Right->Key : Left->TableId < Right->TableId;
	});
	SourceList->RequestListRefresh();
}

void SNKMLocalizationDashboard::ApplyRowsToDocument()
{
	for (FNKMLocalizationTable& Table : Document.Tables) Table.Entries.Reset();
	for (const TSharedPtr<FNKMSourceEditorRow>& Row : SourceRows)
	{
		if (FNKMLocalizationTable* Table = Document.Tables.FindByPredicate([&Row](const FNKMLocalizationTable& Candidate) { return Candidate.Id == Row->TableId; }))
		{
			Table->Entries.Add({Row->Key, Row->SourceString, Row->MetaData});
		}
	}
}

void SNKMLocalizationDashboard::RefreshTranslationRows()
{
	TranslationRows.Reset();
	const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
	for (const FString& Culture : FNKMLocalizationPipelineRunner::GetCultures())
	{
		const FString BasePath = FPaths::ProjectDir() / Settings->GetNormalizedLocalizationTargetRoot() / Culture;
		const FString POPath = BasePath / (Settings->LocalizationTargetName + TEXT(".po"));
		const int64 POSize = IFileManager::Get().FileSize(*POPath);
		const int64 LocResSize = IFileManager::Get().FileSize(*(BasePath / (Settings->LocalizationTargetName + TEXT(".locres"))));
		FNKMTranslationStatus TranslationStatus;
		FString AnalyzeError;
		const bool bAnalyzed = POSize > 0 && FNKMTranslationStatusAnalyzer::AnalyzeFile(POPath, TranslationStatus, AnalyzeError);
		const FString ProgressStatus = POSize <= 0
			? TEXT("No PO")
			: bAnalyzed
				? FString::Printf(
					TEXT("%d / %d (%.1f%%)"),
					TranslationStatus.Translated,
					TranslationStatus.Total,
					TranslationStatus.Total > 0 ? (100.0 * TranslationStatus.Translated / TranslationStatus.Total) : 0.0)
				: TEXT("Invalid PO");
		const FString POStatus = POSize <= 0
			? TEXT("Missing")
			: bAnalyzed
				? FString::Printf(
					TEXT("Stale:%d Missing:%d Invalid:%d"),
					TranslationStatus.Stale,
					TranslationStatus.Missing,
					TranslationStatus.Invalid)
				: FString::Printf(TEXT("Invalid PO: %s"), *AnalyzeError);
		TranslationRows.Add(MakeShared<FNKMTranslationStatusRow>(FNKMTranslationStatusRow{
			Culture,
			ProgressStatus,
			POStatus,
			LocResSize > 0 ? FString::Printf(TEXT("Compiled (%lld bytes)"), LocResSize) : TEXT("Missing"),
			TranslationStatus.Total,
			TranslationStatus.Translated,
			TranslationStatus.Stale,
			TranslationStatus.Missing,
			TranslationStatus.Invalid}));
	}
	if (TranslationList.IsValid()) TranslationList->RequestListRefresh();
}

void SNKMLocalizationDashboard::SetResultStatus(
	const FString& Operation,
	const FNKMLocalizationResult& Result,
	const bool bSucceeded)
{
	FString Status = FString::Printf(TEXT("%s: %s"), *Operation, bSucceeded ? TEXT("Succeeded") : TEXT("Failed"));
	const FString Diagnostics = DiagnosticsToString(Result);
	if (!Diagnostics.IsEmpty()) Status += TEXT("\n") + Diagnostics;
	SetStatus(Status);
}

void SNKMLocalizationDashboard::SetStatus(const FString& Status)
{
	if (StatusText.IsValid()) StatusText->SetText(FText::FromString(Status));
}

FString SNKMLocalizationDashboard::GetSourcePath() const
{
	FString Path = SourcePathTextBox.IsValid() ? SourcePathTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	return Path.IsEmpty() || !FPaths::IsRelative(Path) ? Path : FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Path);
}

FName SNKMLocalizationDashboard::GetExplicitDefaultTableId() const
{
	const FString Requested = DefaultTableIdTextBox.IsValid()
		? DefaultTableIdTextBox->GetText().ToString().TrimStartAndEnd()
		: FString();
	if (!Requested.IsEmpty()
		&& Document.Tables.ContainsByPredicate([&Requested](const FNKMLocalizationTable& Table) { return Table.Id == Requested; }))
	{
		return FName(*Requested);
	}
	return Document.Tables.Num() == 1 ? FName(*Document.Tables[0].Id) : NAME_None;
}

bool SNKMLocalizationDashboard::ConfirmDiscardChanges() const
{
	return !bIsDirty
		|| FMessageDialog::Open(
			EAppMsgType::YesNo,
			LOCTEXT("DiscardUnsavedSource", "Discard unsaved localization source changes?")) == EAppReturnType::Yes;
}

#undef LOCTEXT_NAMESPACE
