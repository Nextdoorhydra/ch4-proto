#include "NKMLocalizationRecordProvider.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NKMLocalizationSettings.h"
#include "Serialization/Csv/CsvParser.h"
#include "UObject/UnrealType.h"

namespace
{
	bool AddUniqueRecord(
		const FString& RawId,
		const FString& SourceLocation,
		TSet<FString>& FoldedIds,
		TArray<FNKMLocalizationRecord>& OutRecords,
		FNKMLocalizationResult& OutResult)
	{
		const FString Id = RawId.TrimStartAndEnd();
		if (Id.IsEmpty())
		{
			OutResult.AddError(TEXT("NKMLOC_RECORD_ID_EMPTY"), FString::Printf(TEXT("Record ID is empty at %s."), *SourceLocation));
			return false;
		}

		const FString Folded = Id.ToLower();
		if (FoldedIds.Contains(Folded))
		{
			OutResult.AddError(TEXT("NKMLOC_RECORD_ID_DUPLICATE"), FString::Printf(TEXT("Duplicate or case-only duplicate record ID '%s' at %s."), *Id, *SourceLocation));
			return false;
		}

		FoldedIds.Add(Folded);
		OutRecords.Add({FName(*Id), SourceLocation});
		return true;
	}

	class FNKMCsvRecordProvider final : public INKMLocalizationRecordProvider
	{
	public:
		FNKMCsvRecordProvider(FString InFilename, const FName InIdColumn)
			: Filename(MoveTemp(InFilename)), IdColumn(InIdColumn)
		{
		}

		virtual bool EnumerateRecords(TArray<FNKMLocalizationRecord>& OutRecords, FNKMLocalizationResult& OutResult) const override
		{
			FString Csv;
			if (!FFileHelper::LoadFileToString(Csv, *Filename))
			{
				OutResult.AddError(TEXT("NKMLOC_RECORD_CSV_READ"), FString::Printf(TEXT("Unable to read record CSV '%s'."), *Filename));
				return false;
			}

			const FCsvParser Parser(Csv);
			const FCsvParser::FRows& Rows = Parser.GetRows();
			if (Rows.IsEmpty())
			{
				OutResult.AddError(TEXT("NKMLOC_RECORD_CSV_EMPTY"), FString::Printf(TEXT("Record CSV is empty: '%s'."), *Filename));
				return false;
			}

			const FString RequiredHeader = IdColumn.ToString();
			int32 IdColumnIndex = INDEX_NONE;
			TSet<FString> FoldedHeaders;
			for (int32 ColumnIndex = 0; ColumnIndex < Rows[0].Num(); ++ColumnIndex)
			{
				const FString Header = FString(Rows[0][ColumnIndex]).TrimStartAndEnd();
				const FString FoldedHeader = Header.ToLower();
				if (FoldedHeaders.Contains(FoldedHeader))
				{
					OutResult.AddError(TEXT("NKMLOC_RECORD_CSV_HEADER_DUPLICATE"), FString::Printf(TEXT("CSV contains a duplicate or case-only duplicate header '%s'."), *Header));
					return false;
				}
				FoldedHeaders.Add(FoldedHeader);
				if (Header.Equals(RequiredHeader, ESearchCase::CaseSensitive))
				{
					IdColumnIndex = ColumnIndex;
				}
			}
			if (IdColumnIndex == INDEX_NONE)
			{
				OutResult.AddError(TEXT("NKMLOC_RECORD_CSV_ID_COLUMN"), FString::Printf(TEXT("CSV requires exact record ID column '%s'."), *RequiredHeader));
				return false;
			}

			TSet<FString> FoldedIds;
			for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
			{
				const TArray<const TCHAR*>& Row = Rows[RowIndex];
				if (!Row.IsValidIndex(IdColumnIndex))
				{
					OutResult.AddError(TEXT("NKMLOC_RECORD_CSV_ROW"), FString::Printf(TEXT("CSV row %d has no '%s' cell."), RowIndex + 1, *RequiredHeader));
					continue;
				}
				AddUniqueRecord(Row[IdColumnIndex], FString::Printf(TEXT("%s:%d"), *Filename, RowIndex + 1), FoldedIds, OutRecords, OutResult);
			}
			return !OutResult.HasErrors();
		}

	private:
		FString Filename;
		FName IdColumn;
	};

	bool ReadRecordId(const UStruct* Struct, const void* Container, const FName PropertyName, FString& OutId)
	{
		const FProperty* Property = FindFProperty<FProperty>(Struct, PropertyName);
		if (!Property)
		{
			return false;
		}
		const void* Value = Property->ContainerPtrToValuePtr<void>(Container);
		if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			OutId = NameProperty->GetPropertyValue(Value).ToString();
			return true;
		}
		if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			OutId = StringProperty->GetPropertyValue(Value);
			return true;
		}
		return false;
	}

	class FNKMReflectedMapRecordProvider final : public INKMLocalizationRecordProvider
	{
	public:
		explicit FNKMReflectedMapRecordProvider(const FNKMTextBindingProfile& InProfile)
			: Profile(InProfile)
		{
		}

		virtual bool EnumerateRecords(TArray<FNKMLocalizationRecord>& OutRecords, FNKMLocalizationResult& OutResult) const override
		{
			UObject* SourceAsset = Profile.RecordSourceAsset.TryLoad();
			if (!SourceAsset)
			{
				OutResult.AddError(TEXT("NKMLOC_RECORD_SOURCE_ASSET"), FString::Printf(TEXT("Unable to load record source asset for profile '%s': %s"), *Profile.ProfileName.ToString(), *Profile.RecordSourceAsset.ToString()));
				return false;
			}

			UObject* RecordOwner = SourceAsset;
			if (!Profile.RecordSourceObjectProperty.IsNone())
			{
				const FObjectPropertyBase* ObjectProperty = FindFProperty<FObjectPropertyBase>(SourceAsset->GetClass(), Profile.RecordSourceObjectProperty);
				RecordOwner = ObjectProperty ? ObjectProperty->GetObjectPropertyValue_InContainer(SourceAsset) : nullptr;
			}
			if (!RecordOwner)
			{
				OutResult.AddError(TEXT("NKMLOC_RECORD_SOURCE_OBJECT"), FString::Printf(TEXT("Profile '%s' cannot resolve record source object property '%s'."), *Profile.ProfileName.ToString(), *Profile.RecordSourceObjectProperty.ToString()));
				return false;
			}

			const FMapProperty* MapProperty = FindFProperty<FMapProperty>(RecordOwner->GetClass(), Profile.RecordMapProperty);
			const FStructProperty* ValueProperty = MapProperty ? CastField<FStructProperty>(MapProperty->ValueProp) : nullptr;
			if (!MapProperty || !ValueProperty)
			{
				OutResult.AddError(TEXT("NKMLOC_RECORD_MAP"), FString::Printf(TEXT("Profile '%s' requires a TMap with struct values at '%s'."), *Profile.ProfileName.ToString(), *Profile.RecordMapProperty.ToString()));
				return false;
			}

			TSet<FString> FoldedIds;
			FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(RecordOwner));
			for (int32 MapIndex = 0; MapIndex < MapHelper.GetMaxIndex(); ++MapIndex)
			{
				if (!MapHelper.IsValidIndex(MapIndex))
				{
					continue;
				}
				FString RecordId;
				if (!ReadRecordId(ValueProperty->Struct, MapHelper.GetValuePtr(MapIndex), Profile.RecordIdProperty, RecordId))
				{
					OutResult.AddError(TEXT("NKMLOC_RECORD_ID_PROPERTY"), FString::Printf(TEXT("Profile '%s' cannot read FName/FString record ID property '%s'."), *Profile.ProfileName.ToString(), *Profile.RecordIdProperty.ToString()));
					continue;
				}
				AddUniqueRecord(RecordId, FString::Printf(TEXT("%s[%d]"), *Profile.RecordSourceAsset.ToString(), MapIndex), FoldedIds, OutRecords, OutResult);
			}
			return !OutResult.HasErrors();
		}

	private:
		FNKMTextBindingProfile Profile;
	};
}

TUniquePtr<INKMLocalizationRecordProvider> FNKMLocalizationRecordProviderFactory::Create(
	const FNKMTextBindingProfile& Profile,
	const FString& CsvOverride,
	FNKMLocalizationResult& OutResult)
{
	FString CsvPath = CsvOverride.TrimStartAndEnd();
	if (CsvPath.IsEmpty())
	{
		CsvPath = Profile.RecordSourceFile.FilePath.TrimStartAndEnd();
	}
	if (!CsvPath.IsEmpty())
	{
		if (Profile.RecordIdColumn.IsNone())
		{
			OutResult.AddError(TEXT("NKMLOC_RECORD_ID_COLUMN"), FString::Printf(TEXT("Profile '%s' must define Record Id Column for CSV reconciliation."), *Profile.ProfileName.ToString()));
			return nullptr;
		}
		if (FPaths::IsRelative(CsvPath))
		{
			CsvPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), CsvPath);
		}
		return MakeUnique<FNKMCsvRecordProvider>(MoveTemp(CsvPath), Profile.RecordIdColumn);
	}

	if (!Profile.RecordSourceAsset.IsNull() && !Profile.RecordMapProperty.IsNone() && !Profile.RecordIdProperty.IsNone())
	{
		return MakeUnique<FNKMReflectedMapRecordProvider>(Profile);
	}

	OutResult.AddError(TEXT("NKMLOC_RECORD_PROVIDER"), FString::Printf(TEXT("Profile '%s' has no CSV or reflected record provider configuration."), *Profile.ProfileName.ToString()));
	return nullptr;
}
