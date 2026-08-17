#include "GameMode/Campaign/CMCampaignDefinition.h"

#if WITH_EDITOR
#include "Engine/AssetManager.h"
#include "Misc/DataValidation.h"
#endif

// 요청 인덱스에 해당하는 읽기 전용 스테이지 설정 반환
const FCMCampaignStageDefinition* UCMCampaignDefinition::GetStage(
    int32 StageIndex) const
{
    return Stages.IsValidIndex(StageIndex) ? &Stages[StageIndex] : nullptr;
}

#if WITH_EDITOR
// 런타임 Travel 전에 캠페인 데이터 오류를 Content Validation 단계에서 차단
EDataValidationResult UCMCampaignDefinition::IsDataValid(
    FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    TSet<FName> StageIds;
    if (Stages.IsEmpty())
    {
        Context.AddError(FText::FromString(TEXT("Campaign must contain at least one stage.")));
        return EDataValidationResult::Invalid;
    }

    for (int32 Index = 0; Index < Stages.Num(); ++Index)
    {
        const FCMCampaignStageDefinition& Stage = Stages[Index];
        if (Stage.StageId.IsNone() || StageIds.Contains(Stage.StageId))
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Stage %d has an empty or duplicate StageId: %s"),
                Index, *Stage.StageId.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        StageIds.Add(Stage.StageId);

        if (Stage.StageMap.IsNull())
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Stage %d (%s) has no StageMap."), Index, *Stage.StageId.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        if (!Stage.LoadScheduleId.IsValid()
            || !UAssetManager::Get().GetPrimaryAssetPath(Stage.LoadScheduleId).IsValid())
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Stage %d (%s) has an unregistered LoadScheduleId: %s"),
                Index, *Stage.StageId.ToString(), *Stage.LoadScheduleId.ToString())));
            Result = EDataValidationResult::Invalid;
        }
    }

    return Result;
}
#endif
