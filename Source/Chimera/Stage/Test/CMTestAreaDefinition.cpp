#include "Stage/Test/CMTestAreaDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// AreaId가 일치하는 구역 설정 검색
const FCMTestAreaEntry* UCMTestAreaDefinition::FindArea(FName AreaId) const
{
    return Areas.FindByPredicate([AreaId](const FCMTestAreaEntry& Area)
    {
        return Area.AreaId == AreaId;
    });
}

// Definition 배열 순서에 따라 다음 구역을 반환하고 마지막이면 첫 구역으로 순환
const FCMTestAreaEntry* UCMTestAreaDefinition::FindNextArea(FName CompletedAreaId) const
{
    if (Areas.IsEmpty())
    {
        return nullptr;
    }

    const int32 CompletedIndex = Areas.IndexOfByPredicate(
        [CompletedAreaId](const FCMTestAreaEntry& Area)
        {
            return Area.AreaId == CompletedAreaId;
        });
    const int32 NextIndex = CompletedIndex == INDEX_NONE
        ? 0 : (CompletedIndex + 1) % Areas.Num();
    return &Areas[NextIndex];
}

#if WITH_EDITOR
// 런타임 진입 전에 Test Area Definition의 식별자와 시작 태그 오류 차단
EDataValidationResult UCMTestAreaDefinition::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    if (Areas.IsEmpty())
    {
        Context.AddError(FText::FromString(TEXT("TestAreaDefinition에는 구역이 하나 이상 필요합니다.")));
        return EDataValidationResult::Invalid;
    }

    TSet<FName> AreaIds;
    TSet<FName> StartTags;
    TSet<FSoftObjectPath> AreaLevels;
    for (int32 Index = 0; Index < Areas.Num(); ++Index)
    {
        const FCMTestAreaEntry& Area = Areas[Index];
        if (Area.AreaId.IsNone() || AreaIds.Contains(Area.AreaId))
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("구역 %d의 AreaId가 비어 있거나 중복되었습니다: %s"),
                Index, *Area.AreaId.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        if (Area.StartTag.IsNone() || StartTags.Contains(Area.StartTag))
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("구역 %d (%s)의 StartTag가 비어 있거나 중복되었습니다: %s"),
                Index, *Area.AreaId.ToString(), *Area.StartTag.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        const FSoftObjectPath AreaLevelPath = Area.AreaLevel.ToSoftObjectPath();
        if (Area.AreaLevel.IsNull() || AreaLevels.Contains(AreaLevelPath))
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("구역 %d (%s)의 AreaLevel이 비어 있거나 중복되었습니다: %s"),
                Index, *Area.AreaId.ToString(), *AreaLevelPath.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        AreaIds.Add(Area.AreaId);
        StartTags.Add(Area.StartTag);
        AreaLevels.Add(AreaLevelPath);
    }
    return Result;
}
#endif
