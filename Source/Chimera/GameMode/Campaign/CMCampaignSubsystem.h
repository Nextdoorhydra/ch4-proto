#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "CMCampaignSubsystem.generated.h"

class UCMCampaignDefinition;
struct FCMCampaignStageDefinition;

UCLASS()
// 맵 전환에도 유지되어야 하는 서버 캠페인 진행 인덱스 관리
class CHIMERA_API UCMCampaignSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 새 캠페인을 첫 스테이지부터 시작
    bool StartCampaign(UCMCampaignDefinition* Definition);

    // 현재 캠페인 진행 상태 초기화
    void ResetCampaign();

    // 다음 스테이지를 Pending으로 예약하되 현재 인덱스는 유지
    bool PrepareNextStage();

    // 새 맵 도착 후 Pending 인덱스를 현재 스테이지로 확정
    bool CommitPendingStage();

    // Travel 실패 시 Pending 전환 취소
    void CancelPendingStage();

    bool IsCampaignActive() const;
    bool IsLastStage() const;
    int32 GetCurrentStageIndex() const { return CurrentStageIndex; }
    int32 GetPendingStageIndex() const { return PendingStageIndex; }
    int32 GetStageCount() const;
    const FCMCampaignStageDefinition* GetCurrentStage() const;
    const FCMCampaignStageDefinition* GetPendingStage() const;
    UCMCampaignDefinition* GetCampaignDefinition() const { return CampaignDefinition; }

private:
    UPROPERTY(Transient)
    TObjectPtr<UCMCampaignDefinition> CampaignDefinition;

    int32 CurrentStageIndex = INDEX_NONE;
    int32 PendingStageIndex = INDEX_NONE;
};
