#include "GameMode/StageRoute/CMStageRouteDefinition.h"

#if WITH_EDITOR
#include "Engine/AssetManager.h"
#include "Misc/DataValidation.h"
#endif

// 요청 인덱스에 해당하는 읽기 전용 스테이지 설정 반환
const FCMStageRouteEntry* UCMStageRouteDefinition::GetStage(
    int32 StageIndex) const
{
    return Stages.IsValidIndex(StageIndex) ? &Stages[StageIndex] : nullptr;
}

// 현재 월드의 전체 패키지 경로로 직접 실행할 스테이지를 검색
int32 UCMStageRouteDefinition::FindStageIndexByMap(
    const FString& MapPackageName) const
{
    if (MapPackageName.IsEmpty())
    {
        return INDEX_NONE;
    }

    for (int32 StageIndex = 0; StageIndex < Stages.Num(); ++StageIndex)
    {
        const FString StageMapPackageName =
            Stages[StageIndex].StageMap.ToSoftObjectPath().GetLongPackageName();
        if (StageMapPackageName == MapPackageName)
        {
            return StageIndex;
        }
    }

    return INDEX_NONE;
}

#if WITH_EDITOR
// 런타임 맵 전환 전에 스테이지 경로 데이터 오류를 콘텐츠 검증 단계에서 차단
EDataValidationResult UCMStageRouteDefinition::IsDataValid(
    FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    TSet<FName> StageIds;
    TSet<FSoftObjectPath> StageMaps;
    if (Stages.IsEmpty())
    {
        Context.AddError(FText::FromString(TEXT("StageRoute에는 스테이지가 하나 이상 필요합니다.")));
        return EDataValidationResult::Invalid;
    }

    for (int32 Index = 0; Index < Stages.Num(); ++Index)
    {
        const FCMStageRouteEntry& Stage = Stages[Index];
        if (Stage.StageId.IsNone() || StageIds.Contains(Stage.StageId))
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("스테이지 %d의 StageId가 비어 있거나 중복되었습니다: %s"),
                Index, *Stage.StageId.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        StageIds.Add(Stage.StageId);

        if (Stage.StageMap.IsNull())
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("스테이지 %d (%s)에 StageMap이 없습니다."), Index, *Stage.StageId.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        else
        {
            const FSoftObjectPath StageMapPath = Stage.StageMap.ToSoftObjectPath();
            if (StageMaps.Contains(StageMapPath))
            {
                Context.AddError(FText::FromString(FString::Printf(
                    TEXT("스테이지 %d (%s)의 StageMap이 중복되었습니다: %s"),
                    Index, *Stage.StageId.ToString(), *StageMapPath.ToString())));
                Result = EDataValidationResult::Invalid;
            }
            StageMaps.Add(StageMapPath);
        }
        if (!Stage.LoadScheduleId.IsValid()
            || !UAssetManager::Get().GetPrimaryAssetPath(Stage.LoadScheduleId).IsValid())
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("스테이지 %d (%s)의 LoadScheduleId가 등록되지 않았습니다: %s"),
                Index, *Stage.StageId.ToString(), *Stage.LoadScheduleId.ToString())));
            Result = EDataValidationResult::Invalid;
        }
    }

    return Result;
}
#endif
