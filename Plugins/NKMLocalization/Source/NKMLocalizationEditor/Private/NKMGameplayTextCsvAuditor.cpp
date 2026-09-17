#include "NKMGameplayTextCsvAuditor.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NKMTextRef.h"
#include "Serialization/Csv/CsvParser.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const FNKMLocalizationTable* FindTable(const FNKMLocalizationDocument& Document, const FName TableId)
	{
		const FString TableValue = TableId.ToString();
		for (const FNKMLocalizationTable& Table : Document.Tables)
		{
			const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *Table.AssetPath, *FPaths::GetBaseFilename(Table.AssetPath));
			if (Table.Id == TableValue || ObjectPath == TableValue)
			{
				return &Table;
			}
		}
		return nullptr;
	}

	bool HasExactKey(const FNKMLocalizationTable& Table, const FString& Key)
	{
		return Table.Entries.ContainsByPredicate([&Key](const FNKMLocalizationEntry& Entry) { return Entry.Key == Key; });
	}
}

TArray<FString> FNKMGameplayTextCsvAuditor::DetectTextColumns(const TArray<FString>& Headers)
{
	TArray<FString> Result;
	for (const FString& Header : Headers)
	{
		if (Header.Equals(TEXT("DisplayName"), ESearchCase::IgnoreCase)
			|| Header.Equals(TEXT("Description"), ESearchCase::IgnoreCase)
			|| Header.EndsWith(TEXT("Text"), ESearchCase::IgnoreCase)
			|| Header.EndsWith(TEXT("TextKey"), ESearchCase::IgnoreCase)
			|| Header.EndsWith(TEXT("TextRef"), ESearchCase::IgnoreCase))
		{
			Result.Add(Header);
		}
	}
	return Result;
}

bool FNKMGameplayTextCsvAuditor::AuditFile(
	const FString& CsvFilename,
	const FNKMLocalizationDocument& SourceDocument,
	const FName DefaultTableId,
	const TArray<FString>& TextColumns,
	FNKMGameplayTextCsvAuditReport& OutReport,
	FNKMLocalizationResult& OutResult)
{
	OutReport = {};
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *CsvFilename))
	{
		OutResult.AddError(TEXT("NKMLOC_AUDIT_FILE"), FString::Printf(TEXT("Unable to read gameplay CSV '%s'."), *CsvFilename));
		return false;
	}

	const FCsvParser Parser(Csv);
	const FCsvParser::FRows& Rows = Parser.GetRows();
	if (Rows.IsEmpty())
	{
		OutResult.AddError(TEXT("NKMLOC_AUDIT_EMPTY"), TEXT("Gameplay CSV is empty."));
		return false;
	}

	TArray<FString> Headers;
	for (const TCHAR* Header : Rows[0])
	{
		Headers.Add(Header);
	}
	TSet<FString> FoldedHeaders;
	for (const FString& Header : Headers)
	{
		const FString Folded = Header.ToLower();
		if (FoldedHeaders.Contains(Folded))
		{
			OutResult.AddError(TEXT("NKMLOC_AUDIT_DUPLICATE_COLUMN"), FString::Printf(TEXT("Gameplay CSV contains duplicate column '%s'."), *Header));
		}
		FoldedHeaders.Add(Folded);
	}
	if (OutResult.HasErrors())
	{
		return false;
	}
	const TArray<FString> Columns = TextColumns.IsEmpty() ? DetectTextColumns(Headers) : TextColumns;
	if (Columns.IsEmpty())
	{
		OutResult.AddError(TEXT("NKMLOC_AUDIT_COLUMNS"), TEXT("No text columns were specified or detected."));
		return false;
	}

	TArray<int32> ColumnIndices;
	for (const FString& Column : Columns)
	{
		const int32 Index = Headers.IndexOfByPredicate([&Column](const FString& Header) { return Header.Equals(Column, ESearchCase::IgnoreCase); });
		if (Index == INDEX_NONE)
		{
			OutResult.AddError(TEXT("NKMLOC_AUDIT_COLUMN_MISSING"), FString::Printf(TEXT("Gameplay CSV column '%s' does not exist."), *Column));
			continue;
		}
		ColumnIndices.Add(Index);
	}
	if (OutResult.HasErrors())
	{
		return false;
	}

	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const TArray<const TCHAR*>& Row = Rows[RowIndex];
		for (const int32 ColumnIndex : ColumnIndices)
		{
			if (!Row.IsValidIndex(ColumnIndex))
			{
				OutReport.Issues.Add({RowIndex + 1, Headers[ColumnIndex], FString(), TEXT("NKMLOC_AUDIT_SHORT_ROW"), TEXT("CSV row does not contain the required text column.")});
				continue;
			}
			const FString Value = FString(Row[ColumnIndex]).TrimStartAndEnd();
			if (Value.IsEmpty())
			{
				OutReport.Issues.Add({RowIndex + 1, Headers[ColumnIndex], Value, TEXT("NKMLOC_AUDIT_EMPTY_REF"), TEXT("Required text reference is empty.")});
				continue;
			}

			++OutReport.CheckedReferenceCount;
			FNKMTextRef Reference;
			FString ParseError;
			if (!FNKMTextRef::Parse(Value, DefaultTableId, Reference, &ParseError))
			{
				OutReport.Issues.Add({RowIndex + 1, Headers[ColumnIndex], Value, TEXT("NKMLOC_AUDIT_REF"), ParseError});
				continue;
			}

			const FNKMLocalizationTable* Table = FindTable(SourceDocument, Reference.TableId);
			if (!Table)
			{
				OutReport.Issues.Add({RowIndex + 1, Headers[ColumnIndex], Value, TEXT("NKMLOC_AUDIT_TABLE"), TEXT("Referenced String Table is not present in the source document.")});
			}
			else if (!HasExactKey(*Table, Reference.Key))
			{
				OutReport.Issues.Add({RowIndex + 1, Headers[ColumnIndex], Value, TEXT("NKMLOC_AUDIT_KEY"), TEXT("Referenced key is missing from the source document.")});
			}
		}
	}
	if (OutReport.CheckedReferenceCount == 0)
	{
		OutResult.AddError(TEXT("NKMLOC_AUDIT_NO_REFERENCES"), TEXT("Gameplay CSV contains no non-empty text references."));
	}

	OutReport.bSucceeded = OutReport.Issues.IsEmpty() && !OutResult.HasErrors();
	return OutReport.Succeeded();
}

bool FNKMGameplayTextCsvAuditor::WriteReport(const FString& Filename, const FNKMGameplayTextCsvAuditReport& Report)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), 1);
	Root->SetBoolField(TEXT("succeeded"), Report.Succeeded());
	Root->SetNumberField(TEXT("checkedReferenceCount"), Report.CheckedReferenceCount);

	TArray<TSharedPtr<FJsonValue>> Issues;
	for (const FNKMGameplayTextCsvIssue& Issue : Report.Issues)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("row"), Issue.Row);
		Json->SetStringField(TEXT("column"), Issue.Column);
		Json->SetStringField(TEXT("value"), Issue.Value);
		Json->SetStringField(TEXT("code"), Issue.Code);
		Json->SetStringField(TEXT("message"), Issue.Message);
		Issues.Add(MakeShared<FJsonValueObject>(Json));
	}
	Root->SetArrayField(TEXT("issues"), Issues);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	return IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true)
		&& FJsonSerializer::Serialize(Root, Writer)
		&& FFileHelper::SaveStringToFile(Output, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
