#include "Validators/AssetReferenceValidator.h"

#if WITH_EDITOR // AsyncPDALoaderEditor is editor-only.

#include "IAsyncLoadScheduleProvider.h"
#include "AsyncLoadScheduleCatalogBase.h"
#include "PrimaryDataAssetBase.h"
#include "Engine/AssetManager.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "AsyncPDALoader"

// Schedule Provider를 구현한 에셋만 검증 대상으로 선택
bool UAssetReferenceValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	return InObject && InObject->GetClass()->ImplementsInterface(UAsyncLoadScheduleProvider::StaticClass());
}

// catalog의 PrimaryAssetId 존재 여부와 타입·기본 클래스 일치 검증
EDataValidationResult UAssetReferenceValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	IAsyncLoadScheduleProvider* Provider = Cast<IAsyncLoadScheduleProvider>(InAsset);
	if (!Provider)
	{
		return EDataValidationResult::NotValidated;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	TMap<FPrimaryAssetId, FString> AssetLocations;
	bool bHasError = false;

	if (const UAsyncLoadScheduleCatalogBase* Catalog = Cast<UAsyncLoadScheduleCatalogBase>(InAsset))
	{
		for (const FAsyncLoadScheduleCategory& Category : Catalog->Categories)
		{
			for (const FAsyncLoadScheduleEntry& Entry : Category.Entries)
			{
				AssetLocations.FindOrAdd(Entry.AssetId) = FString::Printf(
					TEXT("Category=%s"), *Category.PrimaryAssetType.ToString());

				if (!Entry.IsAssigned())
				{
					Context.AddError(FText::Format(
						LOCTEXT("UnassignedAsset", "PDA의 로드 Timing이 지정되지 않았습니다: {0}"),
						FText::FromString(Entry.AssetId.ToString())));
					bHasError = true;
				}
				if (Entry.AssetId.IsValid() && Entry.AssetId.PrimaryAssetType != FPrimaryAssetType(Category.PrimaryAssetType))
				{
					Context.AddError(FText::Format(
						LOCTEXT("CategoryTypeMismatch", "카테고리 타입과 PrimaryAssetId 타입이 일치하지 않습니다: Category={0}, Asset={1}"),
						FText::FromName(Category.PrimaryAssetType), FText::FromString(Entry.AssetId.ToString())));
					bHasError = true;
				}
			}
		}
	}

	for (const TPair<int32, FGameplayTag>& Pair : Provider->GetReachableScopeTimingPairs())
	{
		for (const FPrimaryAssetId& AssetId : Provider->ResolveAssetIds(Pair.Key, Pair.Value))
		{
			AssetLocations.FindOrAdd(AssetId) = FString::Printf(
				TEXT("Scope=%d, Timing=%s"), Pair.Key, *Pair.Value.ToString());
		}
	}

	for (const TPair<FPrimaryAssetId, FString>& AssetLocation : AssetLocations)
	{
		const FPrimaryAssetId& AssetId = AssetLocation.Key;
		if (!AssetId.IsValid())
		{
			Context.AddError(FText::Format(
				LOCTEXT("InvalidAssetId", "빈 FPrimaryAssetId가 등록돼 있습니다. ({0})"),
				FText::FromString(AssetLocation.Value)));
			bHasError = true;
			continue;
		}

		FAssetData FoundAssetData;
		if (!AssetManager.GetPrimaryAssetData(AssetId, FoundAssetData))
		{
			Context.AddError(FText::Format(
				LOCTEXT("MissingAsset", "등록된 PrimaryAssetId가 존재하지 않습니다: {0}"),
				FText::FromString(AssetId.ToString())));
			bHasError = true;
			continue;
		}

		// 강제 로드 없이 AssetRegistry의 클래스를 확인한다.
		const UClass* AssetClass = FoundAssetData.GetClass();
		if (!AssetClass || !AssetClass->IsChildOf(UPrimaryDataAssetBase::StaticClass()))
		{
			Context.AddError(FText::Format(
				LOCTEXT("WrongType", "PrimaryAssetId가 UPrimaryDataAssetBase 파생 클래스가 아닙니다: {0}"),
				FText::FromString(AssetId.ToString())));
			bHasError = true;
		}
	}

	if (bHasError)
	{
		AssetFails(InAsset, LOCTEXT("ValidationFailed", "AsyncPDALoader 스케줄에 등록된 자산 중 일부가 존재하지 않거나 타입이 맞지 않습니다."));
		return EDataValidationResult::Invalid;
	}

	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
