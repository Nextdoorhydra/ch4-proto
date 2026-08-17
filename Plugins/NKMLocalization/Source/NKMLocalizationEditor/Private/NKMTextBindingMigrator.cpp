#include "NKMTextBindingMigrator.h"

#include "Misc/Paths.h"
#include "NKMLocalizationSettings.h"
#include "NKMLocalizationSourceWriter.h"
#include "NKMLocalizationValidator.h"
#include "NKMStringTableSynchronizer.h"
#include "UObject/UnrealType.h"

namespace
{
	bool ReadStringProperty(
		const UStruct* OwnerStruct,
		const void* Container,
		const FName PropertyName,
		FString& OutValue)
	{
		const FProperty* Property = FindFProperty<FProperty>(OwnerStruct, PropertyName);
		if (!Property)
		{
			return false;
		}

		const void* Value = Property->ContainerPtrToValuePtr<void>(Container);
		if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			OutValue = NameProperty->GetPropertyValue(Value).ToString();
			return true;
		}
		if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			OutValue = StringProperty->GetPropertyValue(Value);
			return true;
		}
		if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			OutValue = TextProperty->GetPropertyValue(Value).ToString();
			return true;
		}
		return false;
	}

	FString GetAuthoringFilename(
		const UNKMLocalizationSettings& Settings,
		const FNKMTextBindingProfile& Profile)
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir(),
			Settings.GetNormalizedAuthoringSourceRoot() / Profile.AuthoringSourceFile);
	}

	bool MigrateProfile(
		const UNKMLocalizationSettings& Settings,
		const FNKMTextBindingProfile& Profile,
		FNKMLocalizationResult& OutResult)
	{
		UObject* SourceAsset = Profile.LegacySourceAsset.TryLoad();
		if (!SourceAsset)
		{
			OutResult.AddError(TEXT("NKMLOC_BINDING_SOURCE_ASSET"), FString::Printf(TEXT("Unable to load legacy source asset for profile '%s': %s"), *Profile.ProfileName.ToString(), *Profile.LegacySourceAsset.ToString()));
			return false;
		}

		const FObjectPropertyBase* ParserProperty = FindFProperty<FObjectPropertyBase>(
			SourceAsset->GetClass(),
			Profile.LegacyParserProperty);
		UObject* Parser = ParserProperty ? ParserProperty->GetObjectPropertyValue_InContainer(SourceAsset) : nullptr;
		if (!Parser)
		{
			OutResult.AddError(TEXT("NKMLOC_BINDING_PARSER"), FString::Printf(TEXT("Profile '%s' cannot find parser property '%s'."), *Profile.ProfileName.ToString(), *Profile.LegacyParserProperty.ToString()));
			return false;
		}

		const FMapProperty* RowMapProperty = FindFProperty<FMapProperty>(Parser->GetClass(), Profile.LegacyRowMapProperty);
		const FStructProperty* RowValueProperty = RowMapProperty ? CastField<FStructProperty>(RowMapProperty->ValueProp) : nullptr;
		if (!RowMapProperty || !RowValueProperty)
		{
			OutResult.AddError(TEXT("NKMLOC_BINDING_ROW_MAP"), FString::Printf(TEXT("Profile '%s' requires a TMap with struct values at '%s'."), *Profile.ProfileName.ToString(), *Profile.LegacyRowMapProperty.ToString()));
			return false;
		}

		FNKMLocalizationDocument Document;
		Document.Target = Settings.LocalizationTargetName;
		Document.NativeCulture = Settings.NativeCulture;
		FNKMLocalizationTable& Table = Document.Tables.AddDefaulted_GetRef();
		Table.Id = Profile.TableId.ToString();
		Table.AssetPath = Settings.MakeConventionalTablePackagePath(Profile.TableId);

		TSet<FString> EntryKeys;
		FScriptMapHelper MapHelper(RowMapProperty, RowMapProperty->ContainerPtrToValuePtr<void>(Parser));
		for (int32 MapIndex = 0; MapIndex < MapHelper.GetMaxIndex(); ++MapIndex)
		{
			if (!MapHelper.IsValidIndex(MapIndex))
			{
				continue;
			}

			const void* Row = MapHelper.GetValuePtr(MapIndex);
			FString RecordId;
			if (!ReadStringProperty(RowValueProperty->Struct, Row, Profile.LegacyRecordIdProperty, RecordId)
				|| RecordId.IsEmpty())
			{
				OutResult.AddError(TEXT("NKMLOC_BINDING_RECORD_ID"), FString::Printf(TEXT("Profile '%s' contains a row without '%s'."), *Profile.ProfileName.ToString(), *Profile.LegacyRecordIdProperty.ToString()));
				continue;
			}

			for (const FNKMTextBindingField& Field : Profile.Fields)
			{
				FString SourceString;
				if (!ReadStringProperty(RowValueProperty->Struct, Row, Field.LegacySourceProperty, SourceString))
				{
					OutResult.AddError(TEXT("NKMLOC_BINDING_SOURCE_PROPERTY"), FString::Printf(TEXT("Profile '%s' row '%s' has no FString/FText property '%s'."), *Profile.ProfileName.ToString(), *RecordId, *Field.LegacySourceProperty.ToString()));
					continue;
				}
				SourceString.TrimStartAndEndInline();
				if (SourceString.IsEmpty() || SourceString == TEXT("-"))
				{
					continue;
				}

				FString Key = Profile.KeyPattern;
				Key.ReplaceInline(TEXT("{Id}"), *RecordId, ESearchCase::CaseSensitive);
				Key.ReplaceInline(TEXT("{Field}"), *Field.FieldName.ToString(), ESearchCase::CaseSensitive);
				if (EntryKeys.Contains(Key))
				{
					OutResult.AddError(TEXT("NKMLOC_BINDING_DUPLICATE_KEY"), FString::Printf(TEXT("Profile '%s' generated duplicate key '%s'."), *Profile.ProfileName.ToString(), *Key));
					continue;
				}
				EntryKeys.Add(Key);

				FNKMLocalizationEntry& Entry = Table.Entries.AddDefaulted_GetRef();
				Entry.Key = MoveTemp(Key);
				Entry.SourceString = MoveTemp(SourceString);
				Entry.MetaData.Add(TEXT("BindingProfile"), Profile.ProfileName.ToString());
				Entry.MetaData.Add(TEXT("RecordId"), RecordId);
				Entry.MetaData.Add(TEXT("Field"), Field.FieldName.ToString());
			}
		}

		if (OutResult.HasErrors())
		{
			return false;
		}
		if (!FNKMLocalizationValidator::Validate(Document, OutResult))
		{
			return false;
		}
		if (!FNKMLocalizationSourceWriter::SaveStructuredJson(
			GetAuthoringFilename(Settings, Profile),
			Document,
			OutResult))
		{
			return false;
		}

		TArray<FNKMStringTableDiff> Diffs;
		return FNKMStringTableSynchronizer::Sync(Document, Diffs, OutResult);
	}
}

bool FNKMTextBindingMigrator::MigrateConfiguredProfiles(
	const bool bOverwriteExisting,
	FNKMLocalizationResult& OutResult)
{
	const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
	for (const FNKMTextBindingProfile& Profile : Settings->TextBindingProfiles)
	{
		if (Profile.AuthoringSourceFile.IsEmpty() || Profile.LegacySourceAsset.IsNull())
		{
			continue;
		}
		const FString Filename = GetAuthoringFilename(*Settings, Profile);
		if (!bOverwriteExisting && FPaths::FileExists(Filename))
		{
			OutResult.AddError(TEXT("NKMLOC_BINDING_SOURCE_EXISTS"), FString::Printf(TEXT("Refusing to overwrite authoritative JSON '%s'. Use explicit force only for a deliberate re-migration."), *Filename));
		}
	}
	if (OutResult.HasErrors())
	{
		return false;
	}

	bool bMigratedAny = false;
	for (const FNKMTextBindingProfile& Profile : Settings->TextBindingProfiles)
	{
		if (Profile.AuthoringSourceFile.IsEmpty() || Profile.LegacySourceAsset.IsNull())
		{
			continue;
		}
		bMigratedAny = true;
		if (!MigrateProfile(*Settings, Profile, OutResult))
		{
			return false;
		}
	}
	if (!bMigratedAny)
	{
		OutResult.AddError(TEXT("NKMLOC_BINDING_NO_MIGRATIONS"), TEXT("No binding profile defines both Authoring Source File and Legacy Source Asset."));
		return false;
	}
	return true;
}
