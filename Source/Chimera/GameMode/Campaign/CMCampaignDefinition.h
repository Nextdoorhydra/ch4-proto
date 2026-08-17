#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CMCampaignDefinition.generated.h"

class UWorld;

USTRUCT(BlueprintType)
struct FCMCampaignStageDefinition
{
    GENERATED_BODY()

    // 로그와 데이터 검증에 사용하는 안정적인 스테이지 식별자
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
    FName StageId;

    // 서버가 해당 스테이지로 전환할 월드
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
    TSoftObjectPtr<UWorld> StageMap;

    // 해당 스테이지의 Entry, Background, Result 그룹을 제공하는 Schedule PDA
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
    FPrimaryAssetId LoadScheduleId;

    bool IsValid() const
    {
        return !StageId.IsNone()
            && !StageMap.IsNull()
            && LoadScheduleId.IsValid();
    }
};

UCLASS(BlueprintType)
// 캠페인의 고정된 스테이지 순서와 각 맵, 로드 Schedule 설정 보관
class CHIMERA_API UCMCampaignDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign", meta = (TitleProperty = "StageId"))
    TArray<FCMCampaignStageDefinition> Stages;

    const FCMCampaignStageDefinition* GetStage(int32 StageIndex) const;

#if WITH_EDITOR
    // 저장 시 중복 ID와 Map, Schedule 설정 오류를 에디터 Validation에 보고
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
