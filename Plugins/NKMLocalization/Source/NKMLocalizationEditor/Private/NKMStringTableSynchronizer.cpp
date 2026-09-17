#include "NKMStringTableSynchronizer.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "NKMLocalizationValidator.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	FString MakeObjectPath(const FString& PackageName)
	{
		return FString::Printf(TEXT("%s.%s"), *PackageName, *FPackageName::GetLongPackageAssetName(PackageName));
	}

	bool MetaDataEqual(
		const FStringTableConstRef& ExistingTable,
		const FNKMLocalizationEntry& DesiredEntry)
	{
		TMap<FName, FString> ExistingMetaData;
		ExistingTable->EnumerateMetaData(
			DesiredEntry.Key,
			[&ExistingMetaData](FName MetaDataId, const FString& MetaDataValue)
			{
				ExistingMetaData.Add(MetaDataId, MetaDataValue);
				return true;
			});

		if (ExistingMetaData.Num() != DesiredEntry.MetaData.Num())
		{
			return false;
		}

		for (const TPair<FName, FString>& Pair : DesiredEntry.MetaData)
		{
			const FString* ExistingValue = ExistingMetaData.Find(Pair.Key);
			if (!ExistingValue || !ExistingValue->Equals(Pair.Value, ESearchCase::CaseSensitive))
			{
				return false;
			}
		}

		return true;
	}

	UStringTable* LoadExistingAsset(const FString& PackageName)
	{
		if (!FPackageName::DoesPackageExist(PackageName))
		{
			return nullptr;
		}
		return LoadObject<UStringTable>(nullptr, *MakeObjectPath(PackageName));
	}

	bool CanWritePackage(const FString& PackageName, FNKMLocalizationResult& OutResult, const FString& TableId)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().FileExists(*Filename) && IFileManager::Get().IsReadOnly(*Filename))
		{
			OutResult.AddError(TEXT("NKMLOC_ASSET_READ_ONLY"), FString::Printf(TEXT("Asset package is read-only: %s"), *Filename), TableId);
			return false;
		}
		return true;
	}

	bool SaveTableAsset(UStringTable* Asset, FNKMLocalizationResult& OutResult, const FString& TableId)
	{
		UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
		if (!Package)
		{
			OutResult.AddError(TEXT("NKMLOC_PACKAGE"), TEXT("String Table has no package."), TableId);
			return false;
		}

		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
		{
			OutResult.AddError(TEXT("NKMLOC_DIRECTORY"), FString::Printf(TEXT("Unable to create asset directory: %s"), *FPaths::GetPath(Filename)), TableId);
			return false;
		}

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		SaveArgs.bSlowTask = false;
		if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
		{
			OutResult.AddError(TEXT("NKMLOC_SAVE"), FString::Printf(TEXT("Unable to save String Table asset: %s"), *Filename), TableId);
			return false;
		}

		// GatherTextFromAssets filters for on-disk Asset Registry data. A package created
		// earlier in this commandlet process is otherwise only visible on the next boot.
		FAssetRegistryModule::GetRegistry().ScanModifiedAssetFiles({Filename});

		return true;
	}
}

bool FNKMStringTableDiff::HasChanges() const
{
	return Added > 0 || Updated > 0 || Removed > 0 || bNamespaceChanged;
}

FNKMStringTableDiff FNKMStringTableSynchronizer::BuildDiff(
	const UStringTable* ExistingAsset,
	const FNKMLocalizationTable& DesiredTable)
{
	FNKMStringTableDiff Diff;
	Diff.TableId = DesiredTable.Id;
	Diff.AssetPath = DesiredTable.AssetPath;

	if (!ExistingAsset)
	{
		Diff.Added = DesiredTable.Entries.Num();
		Diff.bNamespaceChanged = true;
		return Diff;
	}

	const FStringTableConstRef ExistingTable = ExistingAsset->GetStringTable();
	Diff.bNamespaceChanged = !ExistingTable->GetNamespace().Equals(DesiredTable.Id, ESearchCase::CaseSensitive);

	TMap<FString, FString> ExistingStrings;
	ExistingTable->EnumerateSourceStrings(
		[&ExistingStrings](const FString& Key, const FString& SourceString)
		{
			ExistingStrings.Add(Key, SourceString);
			return true;
		});

	for (const FNKMLocalizationEntry& DesiredEntry : DesiredTable.Entries)
	{
		const FString* ExistingSource = ExistingStrings.Find(DesiredEntry.Key);
		if (!ExistingSource)
		{
			++Diff.Added;
		}
		else
		{
			if (!ExistingSource->Equals(DesiredEntry.SourceString, ESearchCase::CaseSensitive)
				|| !MetaDataEqual(ExistingTable, DesiredEntry))
			{
				++Diff.Updated;
			}
			ExistingStrings.Remove(DesiredEntry.Key);
		}
	}

	Diff.Removed = ExistingStrings.Num();
	return Diff;
}

void FNKMStringTableSynchronizer::ApplyToAsset(
	UStringTable& Asset,
	const FNKMLocalizationTable& DesiredTable)
{
	Asset.Modify();
	const FStringTableRef MutableTable = Asset.GetMutableStringTable();
	MutableTable->SetNamespace(DesiredTable.Id);
	MutableTable->ClearSourceStrings(DesiredTable.Entries.Num());

	TArray<const FNKMLocalizationEntry*> SortedEntries;
	SortedEntries.Reserve(DesiredTable.Entries.Num());
	for (const FNKMLocalizationEntry& Entry : DesiredTable.Entries)
	{
		SortedEntries.Add(&Entry);
	}
	SortedEntries.Sort(
		[](const FNKMLocalizationEntry& Left, const FNKMLocalizationEntry& Right)
		{
			return Left.Key < Right.Key;
		});

	for (const FNKMLocalizationEntry* Entry : SortedEntries)
	{
		MutableTable->SetSourceString(Entry->Key, Entry->SourceString);

		TArray<FName> MetaDataKeys;
		Entry->MetaData.GetKeys(MetaDataKeys);
		MetaDataKeys.Sort(FNameLexicalLess());
		for (const FName MetaDataKey : MetaDataKeys)
		{
			MutableTable->SetMetaData(Entry->Key, MetaDataKey, Entry->MetaData.FindChecked(MetaDataKey));
		}
	}
}

bool FNKMStringTableSynchronizer::Preview(
	const FNKMLocalizationDocument& Document,
	TArray<FNKMStringTableDiff>& OutDiffs,
	FNKMLocalizationResult& OutResult)
{
	OutDiffs.Reset();
	if (!FNKMLocalizationValidator::Validate(Document, OutResult))
	{
		return false;
	}

	for (const FNKMLocalizationTable& DesiredTable : Document.Tables)
	{
		UStringTable* ExistingAsset = LoadExistingAsset(DesiredTable.AssetPath);
		if (!ExistingAsset && FPackageName::DoesPackageExist(DesiredTable.AssetPath))
		{
			OutResult.AddError(TEXT("NKMLOC_ASSET_TYPE"), TEXT("Existing package does not contain a UStringTable with the expected name."), DesiredTable.Id);
		}
		OutDiffs.Add(BuildDiff(ExistingAsset, DesiredTable));
	}
	return !OutResult.HasErrors();
}

bool FNKMStringTableSynchronizer::Sync(
	const FNKMLocalizationDocument& Document,
	TArray<FNKMStringTableDiff>& OutDiffs,
	FNKMLocalizationResult& OutResult)
{
	if (!Preview(Document, OutDiffs, OutResult))
	{
		return false;
	}

	TArray<UStringTable*> ExistingAssets;
	ExistingAssets.Reserve(Document.Tables.Num());
	for (const FNKMLocalizationTable& DesiredTable : Document.Tables)
	{
		UStringTable* ExistingAsset = LoadExistingAsset(DesiredTable.AssetPath);
		CanWritePackage(DesiredTable.AssetPath, OutResult, DesiredTable.Id);
		ExistingAssets.Add(ExistingAsset);
	}

	if (OutResult.HasErrors())
	{
		return false;
	}

	for (int32 TableIndex = 0; TableIndex < Document.Tables.Num(); ++TableIndex)
	{
		const FNKMLocalizationTable& DesiredTable = Document.Tables[TableIndex];
		if (!OutDiffs[TableIndex].HasChanges())
		{
			continue;
		}

		UStringTable* Asset = ExistingAssets[TableIndex];
		bool bNewAsset = false;
		if (!Asset)
		{
			UPackage* Package = CreatePackage(*DesiredTable.AssetPath);
			const FName AssetName(*FPackageName::GetLongPackageAssetName(DesiredTable.AssetPath));
			Asset = NewObject<UStringTable>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
			bNewAsset = true;
		}

		ApplyToAsset(*Asset, DesiredTable);

		Asset->MarkPackageDirty();
		if (bNewAsset)
		{
			FAssetRegistryModule::AssetCreated(Asset);
		}

		if (!SaveTableAsset(Asset, OutResult, DesiredTable.Id))
		{
			return false;
		}
	}

	return true;
}
