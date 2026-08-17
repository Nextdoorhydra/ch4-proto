#include "NKMTextBindingReconciler.h"

#include "NKMLocalizationSettings.h"
#include "NKMTextRef.h"

namespace
{
	FNKMLocalizationTable* FindTable(FNKMLocalizationDocument& Document, const FName TableId)
	{
		return Document.Tables.FindByPredicate(
			[TableId](const FNKMLocalizationTable& Table) { return Table.Id == TableId.ToString(); });
	}

	bool SetMetadata(FNKMLocalizationEntry& Entry, const FName Name, const FString& Value)
	{
		if (Entry.MetaData.FindRef(Name) == Value)
		{
			return false;
		}
		Entry.MetaData.Add(Name, Value);
		return true;
	}
}

bool FNKMTextBindingReconciler::Reconcile(
	const FNKMTextBindingProfile& Profile,
	const INKMLocalizationRecordProvider& Provider,
	FNKMLocalizationDocument& InOutDocument,
	FNKMTextBindingReconcileReport& OutReport,
	FNKMLocalizationResult& OutResult)
{
	TArray<FNKMLocalizationRecord> Records;
	if (!Provider.EnumerateRecords(Records, OutResult))
	{
		return false;
	}
	OutReport.RecordCount = Records.Num();

	FNKMLocalizationTable* Table = FindTable(InOutDocument, Profile.TableId);
	if (!Table)
	{
		Table = &InOutDocument.Tables.AddDefaulted_GetRef();
		Table->Id = Profile.TableId.ToString();
		Table->AssetPath = GetDefault<UNKMLocalizationSettings>()->MakeConventionalTablePackagePath(Profile.TableId);
	}

	TSet<FString> ExpectedKeys;
	for (const FNKMLocalizationRecord& Record : Records)
	{
		for (const FNKMTextBindingField& Field : Profile.Fields)
		{
			FNKMTextRef Reference;
			if (!GetDefault<UNKMLocalizationSettings>()->TryMakeTextReference(Profile.ProfileName, Record.Id, Field.FieldName, Reference))
			{
				OutResult.AddError(TEXT("NKMLOC_BINDING_KEY"), FString::Printf(TEXT("Profile '%s' could not generate a key for record '%s', field '%s'."), *Profile.ProfileName.ToString(), *Record.Id.ToString(), *Field.FieldName.ToString()));
				continue;
			}
			ExpectedKeys.Add(Reference.Key);

			FNKMLocalizationEntry* Entry = Table->Entries.FindByPredicate(
				[&Reference](const FNKMLocalizationEntry& Candidate) { return Candidate.Key == Reference.Key; });
			if (!Entry)
			{
				Entry = &Table->Entries.AddDefaulted_GetRef();
				Entry->Key = Reference.Key;
				OutReport.AddedKeys.Add(Reference.Key);
			}

			const bool bMetadataChanged =
				SetMetadata(*Entry, TEXT("BindingProfile"), Profile.ProfileName.ToString())
				| SetMetadata(*Entry, TEXT("RecordId"), Record.Id.ToString())
				| SetMetadata(*Entry, TEXT("Field"), Field.FieldName.ToString());
			if (bMetadataChanged && !OutReport.AddedKeys.Contains(Reference.Key))
			{
				OutReport.UpdatedMetadataKeys.Add(Reference.Key);
			}
		}
	}

	for (const FNKMLocalizationEntry& Entry : Table->Entries)
	{
		if (Entry.MetaData.FindRef(TEXT("BindingProfile")) == Profile.ProfileName.ToString()
			&& !ExpectedKeys.Contains(Entry.Key))
		{
			OutReport.OrphanKeys.Add(Entry.Key);
		}
	}

	OutReport.AddedKeys.Sort();
	OutReport.UpdatedMetadataKeys.Sort();
	OutReport.OrphanKeys.Sort();
	if (!OutReport.AddedKeys.IsEmpty())
	{
		OutResult.AddWarning(TEXT("NKMLOC_BINDING_NATIVE_TEXT_REQUIRED"), FString::Printf(TEXT("Generated %d binding entries. Enter native source text before saving or gathering."), OutReport.AddedKeys.Num()), Profile.TableId.ToString());
	}
	if (!OutReport.OrphanKeys.IsEmpty())
	{
		OutResult.AddWarning(TEXT("NKMLOC_BINDING_ORPHANS"), FString::Printf(TEXT("Profile '%s' has %d orphaned JSON entries; they were preserved."), *Profile.ProfileName.ToString(), OutReport.OrphanKeys.Num()), Profile.TableId.ToString());
	}
	return !OutResult.HasErrors();
}
