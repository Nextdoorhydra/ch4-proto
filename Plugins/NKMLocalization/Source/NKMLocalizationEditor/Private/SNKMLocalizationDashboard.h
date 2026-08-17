#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationTypes.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class STextBlock;
template<typename ItemType> class SListView;

struct FNKMSourceEditorRow
{
	FString TableId;
	FString Key;
	FString SourceString;
	TMap<FName, FString> MetaData;
};

struct FNKMTranslationStatusRow
{
	FString Culture;
	FString ProgressStatus;
	FString POStatus;
	FString LocResStatus;
	int32 Total = 0;
	int32 Translated = 0;
	int32 Stale = 0;
	int32 Missing = 0;
	int32 Invalid = 0;
};

class SNKMLocalizationDashboard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNKMLocalizationDashboard) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	bool CanCloseTab() const { return ConfirmDiscardChanges(); }

private:
	TSharedRef<SWidget> BuildSourceTab();
	TSharedRef<SWidget> BuildTranslationTab();
	TSharedRef<ITableRow> GenerateSourceRow(TSharedPtr<FNKMSourceEditorRow> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> GenerateTranslationRow(TSharedPtr<FNKMTranslationStatusRow> Item, const TSharedRef<STableViewBase>& OwnerTable);

	FReply SelectTab(int32 TabIndex);
	FReply BrowseSource();
	FReply LoadSource();
	FReply ValidateSource();
	FReply PreviewDiff();
	FReply ApplyPathSettings();
	FReply MigrateBindingTexts();
	FReply BrowseRecordCsv();
	FReply ReconcileBoundRecords();
	FReply ValidateBindingCoverage();
	FReply SaveSource();
	FReply SyncSource();
	FReply AddEntry();
	FReply RemoveSelectedEntries();
	FReply AuditGameplayCsv();
	FReply RunPipeline(FString Mode);
	FReply OpenTranslationEditor(FString Culture);
	FReply PreviewCulture(FString Culture);
	FReply RestoreCulture();

	bool LoadCurrentSource(bool bRequireValid);
	bool SaveCurrentSource();
	void RebuildSourceRows();
	void ApplyRowsToDocument();
	void RefreshTranslationRows();
	void SetResultStatus(const FString& Operation, const FNKMLocalizationResult& Result, bool bSucceeded);
	void SetStatus(const FString& Status);
	FString GetSourcePath() const;
	FName GetExplicitDefaultTableId() const;
	bool ConfirmDiscardChanges() const;

	int32 ActiveTabIndex = 0;
	FString SelectedSourcePath;
	FString PreviousPreviewLanguage;
	bool bIsDirty = false;
	bool bPipelineRunning = false;
	FNKMLocalizationDocument Document;
	TArray<TSharedPtr<FNKMSourceEditorRow>> SourceRows;
	TArray<TSharedPtr<FNKMTranslationStatusRow>> TranslationRows;

	TSharedPtr<SEditableTextBox> SourcePathTextBox;
	TSharedPtr<SEditableTextBox> DefaultTableIdTextBox;
	TSharedPtr<SEditableTextBox> DefaultAssetPathTextBox;
	TSharedPtr<SEditableTextBox> BindingProfileTextBox;
	TSharedPtr<SEditableTextBox> RecordCsvPathTextBox;
	TSharedPtr<SListView<TSharedPtr<FNKMSourceEditorRow>>> SourceList;
	TSharedPtr<SListView<TSharedPtr<FNKMTranslationStatusRow>>> TranslationList;
	TSharedPtr<class SWidgetSwitcher> TabSwitcher;
	TSharedPtr<STextBlock> StatusText;
};
