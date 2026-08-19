#include "AsyncLoadScheduleCatalogBase.h"
#include "Engine/AssetManager.h"

// catalog에서 Scope와 Timing이 일치하는 PrimaryAssetId 검색
TArray<FPrimaryAssetId> UAsyncLoadScheduleCatalogBase::ResolveAssetIds(int32 Scope, FGameplayTag Timing) const
{
	TArray<FPrimaryAssetId> Result;

	for (const FAsyncLoadScheduleCategory& Category : Categories)
	{
		for (const FAsyncLoadScheduleEntry& Entry : Category.Entries)
		{
			if (Entry.IsAssigned() && Entry.Scope == Scope && Entry.Timing.MatchesTag(Timing))
			{
				Result.AddUnique(Entry.AssetId);
			}
		}
	}

	return Result;
}

// 데이터에 실제 등록된 고유 Scope·Timing 조합 반환
TArray<TPair<int32, FGameplayTag>> UAsyncLoadScheduleCatalogBase::GetRegisteredScopeTimingPairs() const
{
	TArray<TPair<int32, FGameplayTag>> Pairs;

	for (const FAsyncLoadScheduleCategory& Category : Categories)
	{
		for (const FAsyncLoadScheduleEntry& Entry : Category.Entries)
		{
			if (Entry.IsAssigned())
			{
				Pairs.AddUnique(TPair<int32, FGameplayTag>(Entry.Scope, Entry.Timing));
			}
		}
	}

	return Pairs;
}

#if WITH_EDITOR

// AssetManager 등록 PDA를 다시 스캔하면서 기존 Scope·Timing 배정 보존
void UAsyncLoadScheduleCatalogBase::RefreshPDACatalog()
{
	const TSet<FName> Excluded = GetExcludedPrimaryAssetTypes();
	UAssetManager& AssetManager = UAssetManager::Get();

	// 기존 Scope와 Timing을 보존한다.
	TMap<FPrimaryAssetId, FAsyncLoadScheduleEntry> ExistingByAssetId;
	for (const FAsyncLoadScheduleCategory& Category : Categories)
	{
		for (const FAsyncLoadScheduleEntry& Entry : Category.Entries)
		{
			ExistingByAssetId.Add(Entry.AssetId, Entry);
		}
	}

	TArray<FPrimaryAssetTypeInfo> TypeInfos;
	AssetManager.GetPrimaryAssetTypeInfoList(TypeInfos);

	TArray<FAsyncLoadScheduleCategory> NewCategories;
	for (const FPrimaryAssetTypeInfo& TypeInfo : TypeInfos)
	{
		const FName TypeName = TypeInfo.PrimaryAssetType;
		if (Excluded.Contains(TypeName))
		{
			continue;
		}

		const UClass* BaseClass = TypeInfo.GetAssetBaseClass().LoadSynchronous();
		if (!BaseClass || !BaseClass->IsChildOf(UPrimaryDataAssetBase::StaticClass()))
		{
			continue;
		}

		TArray<FPrimaryAssetId> IdList;
		AssetManager.GetPrimaryAssetIdList(TypeInfo.PrimaryAssetType, IdList);
		if (IdList.IsEmpty())
		{
			continue;
		}

		FAsyncLoadScheduleCategory Category;
		Category.PrimaryAssetType = TypeName;

		for (const FPrimaryAssetId& Id : IdList)
		{
			if (const FAsyncLoadScheduleEntry* Existing = ExistingByAssetId.Find(Id))
			{
				Category.Entries.Add(*Existing);
			}
			else
			{
				FAsyncLoadScheduleEntry NewEntry;
				NewEntry.AssetId = Id;
				// 새 항목은 미할당 상태로 추가한다.
				Category.Entries.Add(NewEntry);
			}
		}

		Category.Entries.Sort([](const FAsyncLoadScheduleEntry& A, const FAsyncLoadScheduleEntry& B)
		{
			return A.AssetId.ToString() < B.AssetId.ToString();
		});

		NewCategories.Add(MoveTemp(Category));
	}

	NewCategories.Sort([](const FAsyncLoadScheduleCategory& A, const FAsyncLoadScheduleCategory& B)
	{
		return A.PrimaryAssetType.LexicalLess(B.PrimaryAssetType);
	});

	Categories = MoveTemp(NewCategories);
	MarkPackageDirty();
}

#endif // WITH_EDITOR
