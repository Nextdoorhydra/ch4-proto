#include "NKMLocalizationValidator.h"

#include "Internationalization/Text.h"
#include "Misc/PackageName.h"
#include "NKMLocalizationSettings.h"
#include "NKMTextRef.h"

namespace
{
	bool IsAsciiIdentifierCharacter(const TCHAR Character)
	{
		return (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('a') && Character <= TEXT('z'))
			|| (Character >= TEXT('0') && Character <= TEXT('9'))
			|| Character == TEXT('_');
	}

	bool IsValidSegmentedIdentifier(const FString& Value)
	{
		if (Value.IsEmpty() || Value.StartsWith(TEXT(".")) || Value.EndsWith(TEXT(".")) || Value.Contains(TEXT("..")))
		{
			return false;
		}

		TArray<FString> Segments;
		Value.ParseIntoArray(Segments, TEXT("."), false);
		for (const FString& Segment : Segments)
		{
			if (Segment.IsEmpty() || !((Segment[0] >= TEXT('A') && Segment[0] <= TEXT('Z'))
				|| (Segment[0] >= TEXT('a') && Segment[0] <= TEXT('z'))
				|| Segment[0] == TEXT('_')))
			{
				return false;
			}

			for (const TCHAR Character : Segment)
			{
				if (!IsAsciiIdentifierCharacter(Character))
				{
					return false;
				}
			}
		}

		return true;
	}
}

bool FNKMLocalizationValidator::Validate(const FNKMLocalizationDocument& Document, FNKMLocalizationResult& OutResult)
{
	const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
	if (Document.SchemaVersion != 1)
	{
		OutResult.AddError(TEXT("NKMLOC_SCHEMA_VERSION"), TEXT("Only schemaVersion 1 is supported."));
	}
	if (!Document.Target.Equals(Settings->LocalizationTargetName, ESearchCase::CaseSensitive))
	{
		OutResult.AddError(TEXT("NKMLOC_TARGET"), FString::Printf(TEXT("Target must match Project Settings (%s)."), *Settings->LocalizationTargetName));
	}
	const FString& ConfiguredNativeCulture = Settings->NativeCulture;
	if (!Document.NativeCulture.Equals(ConfiguredNativeCulture, ESearchCase::CaseSensitive))
	{
		OutResult.AddError(TEXT("NKMLOC_NATIVE_CULTURE"), FString::Printf(TEXT("Native culture must match Project Settings (%s)."), *ConfiguredNativeCulture));
	}
	if (Document.Tables.IsEmpty())
	{
		OutResult.AddError(TEXT("NKMLOC_NO_TABLES"), TEXT("At least one String Table is required."));
	}

	TSet<FString> TableIds;
	TSet<FString> AssetPaths;
	const FString StringTableRoot = Settings->GetNormalizedStringTableAssetRoot();
	if (!FPackageName::IsValidLongPackageName(StringTableRoot) || !StringTableRoot.StartsWith(TEXT("/Game/")))
	{
		OutResult.AddError(TEXT("NKMLOC_STRING_TABLE_ROOT"), TEXT("Project Settings > NKM Localization > String Table Asset Root must be a valid /Game package path."));
	}
	for (const FNKMLocalizationTable& Table : Document.Tables)
	{
		const FString FoldedTableId = Table.Id.ToLower();
		const FString FoldedAssetPath = Table.AssetPath.ToLower();
		if (!IsValidSegmentedIdentifier(Table.Id) || !Settings->IsAllowedTableId(Table.Id))
		{
			OutResult.AddError(TEXT("NKMLOC_TABLE_ID"), FString::Printf(TEXT("Table id must be an ASCII segmented identifier allowed by prefix '%s'."), *Settings->TableIdPrefix), Table.Id);
		}
		if (TableIds.Contains(FoldedTableId))
		{
			OutResult.AddError(TEXT("NKMLOC_DUPLICATE_TABLE"), TEXT("Duplicate or case-only duplicate table id."), Table.Id);
		}
		TableIds.Add(FoldedTableId);

		if (!FPackageName::IsValidLongPackageName(Table.AssetPath)
			|| !Table.AssetPath.StartsWith(StringTableRoot + TEXT("/"), ESearchCase::CaseSensitive))
		{
			OutResult.AddError(
				TEXT("NKMLOC_ASSET_PATH"),
				FString::Printf(TEXT("Asset path must be a valid package beneath %s."), *StringTableRoot),
				Table.Id);
		}
		else
		{
			FName RuntimeTableId;
			const FString AssetName = FPackageName::GetLongPackageAssetName(Table.AssetPath);
			const FString ExpectedObjectPath = FString::Printf(TEXT("%s.%s"), *Table.AssetPath, *AssetName);
			if (!FNKMTextRef::ResolveTableId(FName(*Table.Id), RuntimeTableId)
				|| RuntimeTableId.ToString() != ExpectedObjectPath)
			{
				OutResult.AddError(
					TEXT("NKMLOC_ALIAS_ASSET_MISMATCH"),
					FString::Printf(
						TEXT("Table alias '%s' resolves to '%s', not source asset '%s'. Follow the configured asset naming convention, or add an alias override."),
						*Table.Id,
						*RuntimeTableId.ToString(),
						*ExpectedObjectPath),
					Table.Id);
			}
		}
		if (AssetPaths.Contains(FoldedAssetPath))
		{
			OutResult.AddError(TEXT("NKMLOC_DUPLICATE_ASSET"), TEXT("Multiple tables target the same asset path."), Table.Id);
		}
		AssetPaths.Add(FoldedAssetPath);

		if (Table.Entries.IsEmpty())
		{
			OutResult.AddError(TEXT("NKMLOC_NO_ENTRIES"), TEXT("String Table must contain at least one entry."), Table.Id);
		}

		TSet<FString> Keys;
		for (const FNKMLocalizationEntry& Entry : Table.Entries)
		{
			const FString FoldedKey = Entry.Key.ToLower();
			if (!IsValidSegmentedIdentifier(Entry.Key))
			{
				OutResult.AddError(TEXT("NKMLOC_KEY"), TEXT("Key must be an ASCII segmented identifier."), Table.Id, Entry.Key);
			}
			if (Keys.Contains(FoldedKey))
			{
				OutResult.AddError(TEXT("NKMLOC_DUPLICATE_KEY"), TEXT("Duplicate or case-only duplicate key."), Table.Id, Entry.Key);
			}
			Keys.Add(FoldedKey);

			if (Entry.SourceString.TrimStartAndEnd().IsEmpty())
			{
				OutResult.AddError(TEXT("NKMLOC_SOURCE_EMPTY"), TEXT("Source string cannot be empty or whitespace-only."), Table.Id, Entry.Key);
			}
			else if (!FTextFormat::FromString(Entry.SourceString).IsValid())
			{
				OutResult.AddError(TEXT("NKMLOC_SOURCE_FORMAT"), TEXT("Source string contains an invalid FText format pattern."), Table.Id, Entry.Key);
			}
		}
	}

	return !OutResult.HasErrors();
}
