#include "GameMode/Campaign/CMCampaignSubsystem.h"

#include "GameMode/Campaign/CMCampaignDefinition.h"
#include "AsyncLoad/CMStageLoadLog.h"

// 유효한 스테이지가 있는 캠페인을 서버 런타임 상태로 등록
bool UCMCampaignSubsystem::StartCampaign(UCMCampaignDefinition* Definition)
{
    if (!IsValid(Definition) || Definition->Stages.IsEmpty())
    {
        return false;
    }

    for (int32 StageIndex = 0; StageIndex < Definition->Stages.Num(); ++StageIndex)
    {
        if (!Definition->Stages[StageIndex].IsValid())
        {
            UE_LOG(LogChimeraStageLoad, Error,
                TEXT("Campaign contains an invalid stage. Campaign=%s StageIndex=%d StageId=%s"),
                *Definition->GetPathName(), StageIndex,
                *Definition->Stages[StageIndex].StageId.ToString());
            return false;
        }
    }

    CampaignDefinition = Definition;
    CurrentStageIndex = 0;
    PendingStageIndex = INDEX_NONE;
    return true;
}

// 캠페인 종료 또는 로비 복귀 시 진행 상태 제거
void UCMCampaignSubsystem::ResetCampaign()
{
    CampaignDefinition = nullptr;
    CurrentStageIndex = INDEX_NONE;
    PendingStageIndex = INDEX_NONE;
}

// 마지막 스테이지를 넘지 않는 경우에만 다음 인덱스로 진행
bool UCMCampaignSubsystem::PrepareNextStage()
{
    if (!IsCampaignActive() || IsLastStage() || PendingStageIndex != INDEX_NONE)
    {
        return false;
    }

    PendingStageIndex = CurrentStageIndex + 1;
    return true;
}

// 성공적으로 도착한 새 맵에서만 Pending 인덱스를 현재 진행도로 확정
bool UCMCampaignSubsystem::CommitPendingStage()
{
    if (!IsValid(CampaignDefinition)
        || !CampaignDefinition->Stages.IsValidIndex(PendingStageIndex))
    {
        return false;
    }

    CurrentStageIndex = PendingStageIndex;
    PendingStageIndex = INDEX_NONE;
    return true;
}

// 실패한 Travel 예약을 버리고 현재 스테이지 진행도 유지
void UCMCampaignSubsystem::CancelPendingStage()
{
    PendingStageIndex = INDEX_NONE;
}

// 캠페인 정의와 현재 인덱스가 모두 유효한지 확인
bool UCMCampaignSubsystem::IsCampaignActive() const
{
    return IsValid(CampaignDefinition)
        && CampaignDefinition->Stages.IsValidIndex(CurrentStageIndex);
}

// 현재 인덱스가 캠페인의 마지막 스테이지인지 확인
bool UCMCampaignSubsystem::IsLastStage() const
{
    return IsCampaignActive()
        && CurrentStageIndex + 1 >= CampaignDefinition->Stages.Num();
}

// 캠페인 전체 스테이지 수 반환
int32 UCMCampaignSubsystem::GetStageCount() const
{
    return IsValid(CampaignDefinition) ? CampaignDefinition->Stages.Num() : 0;
}

// 현재 인덱스에 해당하는 맵과 Schedule 설정 반환
const FCMCampaignStageDefinition* UCMCampaignSubsystem::GetCurrentStage() const
{
    return IsCampaignActive()
        ? CampaignDefinition->GetStage(CurrentStageIndex)
        : nullptr;
}

// Travel 대상으로 예약된 다음 스테이지 설정 반환
const FCMCampaignStageDefinition* UCMCampaignSubsystem::GetPendingStage() const
{
    return IsValid(CampaignDefinition)
        ? CampaignDefinition->GetStage(PendingStageIndex)
        : nullptr;
}
