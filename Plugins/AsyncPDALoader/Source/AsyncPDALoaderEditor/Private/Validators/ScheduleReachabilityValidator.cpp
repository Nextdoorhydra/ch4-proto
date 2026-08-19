#include "Validators/ScheduleReachabilityValidator.h"

#if WITH_EDITOR // AsyncPDALoaderEditor is editor-only.

#include "IAsyncLoadScheduleProvider.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "AsyncPDALoader"

// Schedule Provider를 구현한 에셋만 검증 대상으로 선택
bool UScheduleReachabilityValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	return InObject && InObject->GetClass()->ImplementsInterface(UAsyncLoadScheduleProvider::StaticClass());
}

// 실제 요청 가능한 Scope·Timing과 등록 slot을 비교해 누락과 dead slot 검증
EDataValidationResult UScheduleReachabilityValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	IAsyncLoadScheduleProvider* Provider = Cast<IAsyncLoadScheduleProvider>(InAsset);
	if (!Provider)
	{
		return EDataValidationResult::NotValidated;
	}

	const TArray<TPair<int32, FGameplayTag>> ReachablePairs = Provider->GetReachableScopeTimingPairs();
	if (ReachablePairs.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoReachablePairs", "스케줄 Provider가 도달 가능한 (Scope, Timing) 조합을 하나도 선언하지 않았습니다."));
		AssetFails(InAsset, LOCTEXT("ValidationFailedNoPairs", "AsyncPDALoader 스케줄에 도달 가능한 요청 조합이 없습니다."));
		return EDataValidationResult::Invalid;
	}

	for (const TPair<int32, FGameplayTag>& Registered : Provider->GetRegisteredScopeTimingPairs())
	{
		// 로더와 같은 GameplayTag 매칭 규칙을 사용한다.
		bool bCovered = false;
		for (const TPair<int32, FGameplayTag>& Reachable : ReachablePairs)
		{
			if (Registered.Key == Reachable.Key && Registered.Value.MatchesTag(Reachable.Value))
			{
				bCovered = true;
				break;
			}
		}

		if (!bCovered)
		{
			AssetWarning(InAsset, FText::Format(
				LOCTEXT("DeadSlot", "이 조합에 등록된 자산은 절대 로드되지 않습니다(죽은 슬롯): Scope={0}, Timing={1}"),
				FText::AsNumber(Registered.Key), FText::FromString(Registered.Value.ToString())));
		}
	}

	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
