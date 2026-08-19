#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CMStageRouteDefinition.generated.h"

class UWorld;

UENUM(BlueprintType)
enum class ECMStageRouteMode : uint8
{
    Normal,             // 클리어하면 다음 스테이지 또는 최종 승리로 진행
    LoopCurrentStage    // 클리어·실패·전체 패배 후 현재 스테이지를 다시 시작
};

USTRUCT(BlueprintType)
struct FCMStageRouteEntry
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

    // 스테이지 진행에 필요한 식별자와 맵, 로드 Schedule이 모두 설정됐는지 확인
    bool IsValid() const
    {
        return !StageId.IsNone()
            && !StageMap.IsNull()
            && LoadScheduleId.IsValid();
    }
};

UCLASS(BlueprintType)
// 고정된 스테이지 진행 순서와 각 맵, 로드 Schedule 설정 보관
class CHIMERA_API UCMStageRouteDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 정식 진행과 반복 테스트 중 이 경로가 사용할 완료 정책
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StageRoute")
    ECMStageRouteMode RouteMode = ECMStageRouteMode::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StageRoute", meta = (TitleProperty = "StageId"))
    TArray<FCMStageRouteEntry> Stages;

    const FCMStageRouteEntry* GetStage(int32 StageIndex) const;

    // 전체 패키지 경로가 일치하는 스테이지 인덱스를 반환
    int32 FindStageIndexByMap(const FString& MapPackageName) const;

#if WITH_EDITOR
    // 저장 시 중복 ID와 Map, Schedule 설정 오류를 에디터 Validation에 보고
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
