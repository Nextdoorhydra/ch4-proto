#include "NKMTextBindingCoverageValidator.h"

#include "Misc/Paths.h"
#include "NKMLocalizationSettings.h"
#include "NKMLocalizationSourceReader.h"
#include "NKMLocalizationValidator.h"
#include "NKMTextRef.h"

bool FNKMTextBindingCoverageValidator::Validate(
	const FNKMTextBindingProfile& Profile,
	const INKMLocalizationRecordProvider& Provider,
	const FNKMLocalizationDocument& Document,
	FNKMTextBindingCoverageReport& OutReport,
	FNKMLocalizationResult& OutResult)
{
	TArray<FNKMLocalizationRecord> Records;
	if (!Provider.EnumerateRecords(Records, OutResult))
	{
		return false;
	}
	OutReport.RecordCount = Records.Num();

	const FNKMLocalizationTable* Table = Document.Tables.FindByPredicate(
		[&Profile](const FNKMLocalizationTable& Candidate) { return Candidate.Id == Profile.TableId.ToString(); });
	if (!Table)
	{
		OutResult.AddError(TEXT("NKMLOC_COVERAGE_TABLE"), FString::Printf(TEXT("Profile '%s' table '%s' is absent from its authoring source."), *Profile.ProfileName.ToString(), *Profile.TableId.ToString()));
		return false;
	}

	TSet<FString> ExpectedKeys;
	for (const FNKMLocalizationRecord& Record : Records)
	{
		for (const FNKMTextBindingField& Field : Profile.Fields)
		{
			FNKMTextRef Reference;
			if (!GetDefault<UNKMLocalizationSettings>()->TryMakeTextReference(Profile.ProfileName, Record.Id, Field.FieldName, Reference))
			{
				OutResult.AddError(TEXT("NKMLOC_COVERAGE_KEY"), FString::Printf(TEXT("Unable to generate binding for profile '%s', record '%s', field '%s'."), *Profile.ProfileName.ToString(), *Record.Id.ToString(), *Field.FieldName.ToString()));
				continue;
			}
			++OutReport.ExpectedKeyCount;
			ExpectedKeys.Add(Reference.Key);
			const FNKMLocalizationEntry* Entry = Table->Entries.FindByPredicate(
				[&Reference](const FNKMLocalizationEntry& Candidate) { return Candidate.Key == Reference.Key; });
			if (!Entry)
			{
				OutReport.MissingKeys.Add(Reference.Key);
				OutResult.AddError(TEXT("NKMLOC_COVERAGE_MISSING"), TEXT("Gameplay record has no generated localization entry."), Table->Id, Reference.Key);
				continue;
			}
			if (Entry->SourceString.TrimStartAndEnd().IsEmpty())
			{
				OutResult.AddError(TEXT("NKMLOC_COVERAGE_SOURCE"), TEXT("Generated localization entry requires native source text."), Table->Id, Reference.Key);
			}

			if (Entry->MetaData.FindRef(TEXT("BindingProfile")) != Profile.ProfileName.ToString()
				|| Entry->MetaData.FindRef(TEXT("RecordId")) != Record.Id.ToString()
				|| Entry->MetaData.FindRef(TEXT("Field")) != Field.FieldName.ToString())
			{
				OutReport.InvalidMetadataKeys.Add(Reference.Key);
				OutResult.AddError(TEXT("NKMLOC_COVERAGE_METADATA"), TEXT("Binding metadata does not match Profile + RecordId + Field."), Table->Id, Reference.Key);
			}
		}
	}

	for (const FNKMLocalizationEntry& Entry : Table->Entries)
	{
		if (Entry.MetaData.FindRef(TEXT("BindingProfile")) == Profile.ProfileName.ToString()
			&& !ExpectedKeys.Contains(Entry.Key))
		{
			OutReport.OrphanKeys.Add(Entry.Key);
			OutResult.AddWarning(TEXT("NKMLOC_COVERAGE_ORPHAN"), TEXT("Localization entry has no corresponding gameplay record and was preserved."), Table->Id, Entry.Key);
		}
	}

	OutReport.MissingKeys.Sort();
	OutReport.InvalidMetadataKeys.Sort();
	OutReport.OrphanKeys.Sort();
	return !OutResult.HasErrors();
}

bool FNKMTextBindingCoverageValidator::ValidateConfiguredProfiles(FNKMLocalizationResult& OutResult)
{
	const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
	bool bValidatedAny = false;
	for (const FNKMTextBindingProfile& Profile : Settings->TextBindingProfiles)
	{
		if (Profile.AuthoringSourceFile.IsEmpty())
		{
			continue;
		}

		TUniquePtr<INKMLocalizationRecordProvider> Provider = FNKMLocalizationRecordProviderFactory::Create(Profile, FString(), OutResult);
		if (!Provider)
		{
			return false;
		}
		bValidatedAny = true;

		const FString SourcePath = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir(),
			Settings->GetNormalizedAuthoringSourceRoot() / Profile.AuthoringSourceFile);
		FNKMLocalizationSourceContext Context;
		Context.Target = Settings->LocalizationTargetName;
		Context.NativeCulture = Settings->NativeCulture;
		FNKMLocalizationDocument Document;
		if (!FNKMLocalizationSourceReader::LoadFile(SourcePath, Context, Document, OutResult)
			|| !FNKMLocalizationValidator::Validate(Document, OutResult))
		{
			return false;
		}

		FNKMTextBindingCoverageReport Report;
		if (!Validate(Profile, *Provider, Document, Report, OutResult))
		{
			return false;
		}
	}

	if (!bValidatedAny)
	{
		OutResult.AddWarning(TEXT("NKMLOC_COVERAGE_NO_PROFILES"), TEXT("No configured binding profile has an authoring source and record provider."));
	}
	return !OutResult.HasErrors();
}
