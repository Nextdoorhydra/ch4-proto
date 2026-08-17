#include "NKMLocalizationSourceReader.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NKMLocalizationSettings.h"
#include "Serialization/Csv/CsvParser.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	void AddConvenienceMetaData(const TSharedPtr<FJsonObject>& EntryObject, FNKMLocalizationEntry& OutEntry)
	{
		FString StringValue;
		if (EntryObject->TryGetStringField(TEXT("comment"), StringValue))
		{
			OutEntry.MetaData.Add(TEXT("Comment"), StringValue);
		}
		if (EntryObject->TryGetStringField(TEXT("context"), StringValue))
		{
			OutEntry.MetaData.Add(TEXT("Context"), StringValue);
		}

		double NumberValue = 0.0;
		if (EntryObject->TryGetNumberField(TEXT("maxLength"), NumberValue))
		{
			OutEntry.MetaData.Add(TEXT("MaxLength"), FString::FromInt(static_cast<int32>(NumberValue)));
		}

		const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
		if (EntryObject->TryGetArrayField(TEXT("tags"), Tags))
		{
			TArray<FString> TagStrings;
			for (const TSharedPtr<FJsonValue>& Tag : *Tags)
			{
				FString TagString;
				if (Tag.IsValid() && Tag->TryGetString(TagString))
				{
					TagStrings.Add(TagString);
				}
			}
			OutEntry.MetaData.Add(TEXT("Tags"), FString::Join(TagStrings, TEXT("|")));
		}

		const TSharedPtr<FJsonObject>* MetaDataObject = nullptr;
		if (EntryObject->TryGetObjectField(TEXT("metadata"), MetaDataObject))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*MetaDataObject)->Values)
			{
				FString MetaDataValue;
				if (Pair.Value.IsValid() && Pair.Value->TryGetString(MetaDataValue))
				{
					OutEntry.MetaData.Add(FName(*Pair.Key), MetaDataValue);
				}
			}
		}
	}

	bool ParseStructuredJson(
		const TSharedPtr<FJsonObject>& Root,
		FNKMLocalizationDocument& OutDocument,
		FNKMLocalizationResult& OutResult)
	{
		double SchemaVersion = 0.0;
		if (!Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion))
		{
			OutResult.AddError(TEXT("NKMLOC_PARSE_SCHEMA"), TEXT("Structured JSON requires a numeric schemaVersion."));
			return false;
		}

		OutDocument.SchemaVersion = static_cast<int32>(SchemaVersion);
		if (!Root->TryGetStringField(TEXT("target"), OutDocument.Target))
		{
			OutResult.AddError(TEXT("NKMLOC_PARSE_TARGET"), TEXT("Structured JSON requires target."));
		}
		if (!Root->TryGetStringField(TEXT("nativeCulture"), OutDocument.NativeCulture))
		{
			OutResult.AddError(TEXT("NKMLOC_PARSE_CULTURE"), TEXT("Structured JSON requires nativeCulture."));
		}

		const TArray<TSharedPtr<FJsonValue>>* Tables = nullptr;
		if (!Root->TryGetArrayField(TEXT("tables"), Tables))
		{
			OutResult.AddError(TEXT("NKMLOC_PARSE_TABLES"), TEXT("Structured JSON requires a tables array."));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Redirects = nullptr;
		if (Root->TryGetArrayField(TEXT("redirects"), Redirects))
		{
			OutDocument.Redirects = *Redirects;
		}

		for (const TSharedPtr<FJsonValue>& TableValue : *Tables)
		{
			const TSharedPtr<FJsonObject> TableObject = TableValue.IsValid() ? TableValue->AsObject() : nullptr;
			if (!TableObject.IsValid())
			{
				OutResult.AddError(TEXT("NKMLOC_PARSE_TABLE"), TEXT("Each tables element must be an object."));
				continue;
			}

			FNKMLocalizationTable& Table = OutDocument.Tables.AddDefaulted_GetRef();
			TableObject->TryGetStringField(TEXT("id"), Table.Id);
			TableObject->TryGetStringField(TEXT("assetPath"), Table.AssetPath);

			const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
			if (!TableObject->TryGetArrayField(TEXT("entries"), Entries))
			{
				OutResult.AddError(TEXT("NKMLOC_PARSE_ENTRIES"), TEXT("Table requires an entries array."), Table.Id);
				continue;
			}

			for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
			{
				const TSharedPtr<FJsonObject> EntryObject = EntryValue.IsValid() ? EntryValue->AsObject() : nullptr;
				if (!EntryObject.IsValid())
				{
					OutResult.AddError(TEXT("NKMLOC_PARSE_ENTRY"), TEXT("Each entries element must be an object."), Table.Id);
					continue;
				}

				FNKMLocalizationEntry& Entry = Table.Entries.AddDefaulted_GetRef();
				EntryObject->TryGetStringField(TEXT("key"), Entry.Key);
				EntryObject->TryGetStringField(TEXT("source"), Entry.SourceString);
				AddConvenienceMetaData(EntryObject, Entry);
			}
		}

		return !OutResult.HasErrors();
	}

	bool ParseKeyValueJson(
		const TSharedPtr<FJsonObject>& Root,
		const FNKMLocalizationSourceContext& Context,
		FNKMLocalizationDocument& OutDocument,
		FNKMLocalizationResult& OutResult)
	{
		const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
		OutDocument.Target = Context.Target.IsEmpty() ? Settings->LocalizationTargetName : Context.Target;
		OutDocument.NativeCulture = Context.NativeCulture.IsEmpty() ? Settings->NativeCulture : Context.NativeCulture;
		FNKMLocalizationTable& Table = OutDocument.Tables.AddDefaulted_GetRef();
		Table.Id = Context.TableId;
		Table.AssetPath = Context.AssetPath;

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
		{
			FString SourceString;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(SourceString))
			{
				OutResult.AddError(
					TEXT("NKMLOC_PARSE_VALUE"),
					TEXT("Simple key-value JSON values must be strings."),
					Table.Id,
					Pair.Key);
				continue;
			}

			Table.Entries.Add({Pair.Key, SourceString, {}});
		}

		return !OutResult.HasErrors();
	}
}

bool FNKMLocalizationSourceReader::LoadFile(
	const FString& Filename,
	const FNKMLocalizationSourceContext& Context,
	FNKMLocalizationDocument& OutDocument,
	FNKMLocalizationResult& OutResult)
{
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *Filename))
	{
		OutResult.AddError(TEXT("NKMLOC_FILE_READ"), FString::Printf(TEXT("Unable to read source file: %s"), *Filename));
		return false;
	}

	const FString Extension = FPaths::GetExtension(Filename, false);
	if (Extension.Equals(TEXT("json"), ESearchCase::IgnoreCase))
	{
		return ParseJson(Source, Context, OutDocument, OutResult);
	}
	if (Extension.Equals(TEXT("csv"), ESearchCase::IgnoreCase))
	{
		return ParseCsv(Source, Context, OutDocument, OutResult);
	}

	OutResult.AddError(TEXT("NKMLOC_FILE_TYPE"), TEXT("Only .json and .csv source files are supported."));
	return false;
}

bool FNKMLocalizationSourceReader::ParseJson(
	const FString& Source,
	const FNKMLocalizationSourceContext& Context,
	FNKMLocalizationDocument& OutDocument,
	FNKMLocalizationResult& OutResult)
{
	OutDocument = FNKMLocalizationDocument();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Source);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutResult.AddError(TEXT("NKMLOC_JSON_INVALID"), TEXT("Source is not a valid JSON object."));
		return false;
	}

	return Root->HasField(TEXT("tables"))
		? ParseStructuredJson(Root, OutDocument, OutResult)
		: ParseKeyValueJson(Root, Context, OutDocument, OutResult);
}

bool FNKMLocalizationSourceReader::ParseCsv(
	const FString& Source,
	const FNKMLocalizationSourceContext& Context,
	FNKMLocalizationDocument& OutDocument,
	FNKMLocalizationResult& OutResult)
{
	OutDocument = FNKMLocalizationDocument();
	const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
	OutDocument.Target = Context.Target.IsEmpty() ? Settings->LocalizationTargetName : Context.Target;
	OutDocument.NativeCulture = Context.NativeCulture.IsEmpty() ? Settings->NativeCulture : Context.NativeCulture;

	const FCsvParser Parser(Source);
	const FCsvParser::FRows& Rows = Parser.GetRows();
	if (Rows.IsEmpty())
	{
		OutResult.AddError(TEXT("NKMLOC_CSV_EMPTY"), TEXT("CSV source is empty."));
		return false;
	}

	const TArray<const TCHAR*>& Headers = Rows[0];
	int32 KeyColumn = INDEX_NONE;
	int32 SourceColumn = INDEX_NONE;
	for (int32 Column = 0; Column < Headers.Num(); ++Column)
	{
		const FString Header = Headers[Column];
		if (Header.Equals(TEXT("Key"), ESearchCase::CaseSensitive))
		{
			KeyColumn = Column;
		}
		else if (Header.Equals(TEXT("SourceString"), ESearchCase::CaseSensitive))
		{
			SourceColumn = Column;
		}
	}

	if (KeyColumn == INDEX_NONE || SourceColumn == INDEX_NONE)
	{
		OutResult.AddError(TEXT("NKMLOC_CSV_HEADERS"), TEXT("CSV requires exact Key and SourceString headers."));
		return false;
	}

	FNKMLocalizationTable& Table = OutDocument.Tables.AddDefaulted_GetRef();
	Table.Id = Context.TableId;
	Table.AssetPath = Context.AssetPath;

	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const TArray<const TCHAR*>& Row = Rows[RowIndex];
		if (!Row.IsValidIndex(KeyColumn) || !Row.IsValidIndex(SourceColumn))
		{
			OutResult.AddError(TEXT("NKMLOC_CSV_ROW"), FString::Printf(TEXT("CSV row %d has too few columns."), RowIndex + 1), Table.Id);
			continue;
		}

		FNKMLocalizationEntry& Entry = Table.Entries.AddDefaulted_GetRef();
		Entry.Key = Row[KeyColumn];
		Entry.SourceString = Row[SourceColumn];

		for (int32 Column = 0; Column < Headers.Num(); ++Column)
		{
			if (Column != KeyColumn && Column != SourceColumn && Row.IsValidIndex(Column) && Headers[Column][0] != TEXT('\0'))
			{
				Entry.MetaData.Add(FName(Headers[Column]), Row[Column]);
			}
		}
	}

	return !OutResult.HasErrors();
}
