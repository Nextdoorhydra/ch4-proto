#include "NKMLocalizationSourceWriter.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FNKMLocalizationSourceWriter::SaveStructuredJson(
	const FString& Filename,
	const FNKMLocalizationDocument& Document,
	FNKMLocalizationResult& OutResult)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), Document.SchemaVersion);
	Root->SetStringField(TEXT("target"), Document.Target);
	Root->SetStringField(TEXT("nativeCulture"), Document.NativeCulture);

	TArray<const FNKMLocalizationTable*> SortedTables;
	for (const FNKMLocalizationTable& Table : Document.Tables)
	{
		SortedTables.Add(&Table);
	}
	SortedTables.Sort([](const FNKMLocalizationTable& Left, const FNKMLocalizationTable& Right) { return Left.Id < Right.Id; });

	TArray<TSharedPtr<FJsonValue>> Tables;
	for (const FNKMLocalizationTable* Table : SortedTables)
	{
		TSharedRef<FJsonObject> TableJson = MakeShared<FJsonObject>();
		TableJson->SetStringField(TEXT("id"), Table->Id);
		TableJson->SetStringField(TEXT("assetPath"), Table->AssetPath);

		TArray<const FNKMLocalizationEntry*> SortedEntries;
		for (const FNKMLocalizationEntry& Entry : Table->Entries)
		{
			SortedEntries.Add(&Entry);
		}
		SortedEntries.Sort([](const FNKMLocalizationEntry& Left, const FNKMLocalizationEntry& Right) { return Left.Key < Right.Key; });

		TArray<TSharedPtr<FJsonValue>> Entries;
		for (const FNKMLocalizationEntry* Entry : SortedEntries)
		{
			TSharedRef<FJsonObject> EntryJson = MakeShared<FJsonObject>();
			EntryJson->SetStringField(TEXT("key"), Entry->Key);
			EntryJson->SetStringField(TEXT("source"), Entry->SourceString);

			TSharedRef<FJsonObject> MetaDataJson = MakeShared<FJsonObject>();
			TArray<FName> MetaDataKeys;
			Entry->MetaData.GetKeys(MetaDataKeys);
			MetaDataKeys.Sort(FNameLexicalLess());
			for (const FName MetaDataKey : MetaDataKeys)
			{
				MetaDataJson->SetStringField(MetaDataKey.ToString(), Entry->MetaData.FindChecked(MetaDataKey));
			}
			if (!MetaDataJson->Values.IsEmpty())
			{
				EntryJson->SetObjectField(TEXT("metadata"), MetaDataJson);
			}
			Entries.Add(MakeShared<FJsonValueObject>(EntryJson));
		}
		TableJson->SetArrayField(TEXT("entries"), Entries);
		Tables.Add(MakeShared<FJsonValueObject>(TableJson));
	}
	Root->SetArrayField(TEXT("tables"), Tables);
	Root->SetArrayField(TEXT("redirects"), Document.Redirects);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutResult.AddError(TEXT("NKMLOC_SOURCE_SERIALIZE"), TEXT("Failed to serialize localization source JSON."));
		return false;
	}

	const FString Directory = FPaths::GetPath(Filename);
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		OutResult.AddError(TEXT("NKMLOC_SOURCE_DIRECTORY"), FString::Printf(TEXT("Failed to create source directory '%s'."), *Directory));
		return false;
	}

	const FString TemporaryPath = Filename + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(Json, *TemporaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		|| !IFileManager::Get().Move(*Filename, *TemporaryPath, true, true, false, true))
	{
		IFileManager::Get().Delete(*TemporaryPath, false, true);
		OutResult.AddError(TEXT("NKMLOC_SOURCE_WRITE"), FString::Printf(TEXT("Failed to atomically write source '%s'."), *Filename));
		return false;
	}

	return true;
}
