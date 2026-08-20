#include "Validators/RequirementCoverageValidator.h"

#if WITH_EDITOR // AsyncPDALoaderEditor is editor-only.

#include "IAsyncLoadScheduleProvider.h"
#include "IAsyncLoadRequirementSource.h"
#include "Misc/DataValidation.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "AsyncPDALoader"

// Schedule Provider를 구현한 에셋만 검증 대상으로 선택
bool URequirementCoverageValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	return InObject && InObject->GetClass()->ImplementsInterface(UAsyncLoadScheduleProvider::StaticClass());
}

// 시스템이 필수로 선언한 모든 PDA가 Schedule에 포함됐는지 검증
EDataValidationResult URequirementCoverageValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	IAsyncLoadScheduleProvider* Provider = Cast<IAsyncLoadScheduleProvider>(InAsset);
	if (!Provider)
	{
		return EDataValidationResult::NotValidated;
	}

	// provider에 등록된 자산을 수집한다.
	TSet<FPrimaryAssetId> RegisteredIds;
	for (const TPair<int32, FGameplayTag>& Pair : Provider->GetReachableScopeTimingPairs())
	{
		for (const FPrimaryAssetId& AssetId : Provider->ResolveAssetIds(Pair.Key, Pair.Value))
		{
			RegisteredIds.Add(AssetId);
		}
	}

	// requirement source CDO를 찾는다.
	bool bHasError = false;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (!Class->ImplementsInterface(UAsyncLoadRequirementSource::StaticClass()))
		{
			continue;
		}
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		const UObject* CDO = Class->GetDefaultObject();
		const IAsyncLoadRequirementSource* Source = Cast<IAsyncLoadRequirementSource>(CDO);
		if (!Source)
		{
			continue;
		}

		for (const FPrimaryAssetId& RequiredId : Source->GetRequiredAssetIds())
		{
			if (!RegisteredIds.Contains(RequiredId))
			{
				Context.AddError(FText::Format(
					LOCTEXT("MissingRequirement", "{0}이(가) 요구하는 자산이 스케줄에 등록돼 있지 않습니다: {1}"),
					FText::FromString(Class->GetName()), FText::FromString(RequiredId.ToString())));
				bHasError = true;
			}
		}
	}

	if (bHasError)
	{
		AssetFails(InAsset, LOCTEXT("ValidationFailed", "IAsyncLoadRequirementSource가 요구하는 자산 중 일부가 이 스케줄에 등록돼 있지 않습니다."));
		return EDataValidationResult::Invalid;
	}

	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
