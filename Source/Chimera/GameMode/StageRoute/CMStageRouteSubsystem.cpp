#include "GameMode/StageRoute/CMStageRouteSubsystem.h"

#include "GameMode/StageRoute/CMStageRouteDefinition.h"
#include "AsyncLoad/CMStageLoadLog.h"

// 유효한 스테이지 경로를 첫 번째 스테이지부터 시작
bool UCMStageRouteSubsystem::StartStageRoute(UCMStageRouteDefinition* Definition)
{
    return StartStageRouteAtIndex(Definition, 0);
}

// 유효한 스테이지 경로를 지정한 인덱스부터 서버 런타임 상태로 등록
bool UCMStageRouteSubsystem::StartStageRouteAtIndex(
    UCMStageRouteDefinition* Definition,
    int32 InitialStageIndex)
{
    if (!IsValid(Definition)
        || !Definition->Stages.IsValidIndex(InitialStageIndex))
    {
        return false;
    }

    for (int32 StageIndex = 0; StageIndex < Definition->Stages.Num(); ++StageIndex)
    {
        if (!Definition->Stages[StageIndex].IsValid())
        {
            UE_LOG(LogChimeraStageLoad, Error,
                TEXT("StageRoute에 유효하지 않은 스테이지가 있습니다. StageRoute=%s StageIndex=%d StageId=%s"),
                *Definition->GetPathName(), StageIndex,
                *Definition->Stages[StageIndex].StageId.ToString());
            return false;
        }
    }

    StageRouteDefinition = Definition;
    CurrentStageIndex = InitialStageIndex;
    PendingStageIndex = INDEX_NONE;
    return true;
}

// 스테이지 경로 종료 또는 로비 복귀 시 진행 상태 제거
void UCMStageRouteSubsystem::ResetStageRoute()
{
    StageRouteDefinition = nullptr;
    CurrentStageIndex = INDEX_NONE;
    PendingStageIndex = INDEX_NONE;
}

// RouteDefinition에 저장된 현재 스테이지 반복 정책 반환
bool UCMStageRouteSubsystem::ShouldLoopCurrentStage() const
{
    return IsStageRouteActive()
        && StageRouteDefinition->RouteMode == ECMStageRouteMode::LoopCurrentStage;
}

// 마지막 스테이지를 넘지 않는 경우에만 다음 인덱스로 진행
bool UCMStageRouteSubsystem::PrepareNextStage()
{
    if (!IsStageRouteActive() || IsLastStage() || PendingStageIndex != INDEX_NONE)
    {
        return false;
    }

    PendingStageIndex = CurrentStageIndex + 1;
    return true;
}

// 성공적으로 도착한 새 맵에서만 Pending 인덱스를 현재 진행도로 확정
bool UCMStageRouteSubsystem::CommitPendingStage()
{
    if (!IsValid(StageRouteDefinition)
        || !StageRouteDefinition->Stages.IsValidIndex(PendingStageIndex))
    {
        return false;
    }

    CurrentStageIndex = PendingStageIndex;
    PendingStageIndex = INDEX_NONE;
    return true;
}

// 실패한 Travel 예약을 버리고 현재 스테이지 진행도 유지
void UCMStageRouteSubsystem::CancelPendingStage()
{
    PendingStageIndex = INDEX_NONE;
}

// 스테이지 경로 정의와 현재 인덱스가 모두 유효한지 확인
bool UCMStageRouteSubsystem::IsStageRouteActive() const
{
    return IsValid(StageRouteDefinition)
        && StageRouteDefinition->Stages.IsValidIndex(CurrentStageIndex);
}

// 현재 인덱스가 스테이지 경로의 마지막 스테이지인지 확인
bool UCMStageRouteSubsystem::IsLastStage() const
{
    return IsStageRouteActive()
        && CurrentStageIndex + 1 >= StageRouteDefinition->Stages.Num();
}

// 스테이지 경로의 전체 스테이지 수 반환
int32 UCMStageRouteSubsystem::GetStageCount() const
{
    return IsValid(StageRouteDefinition) ? StageRouteDefinition->Stages.Num() : 0;
}

// 현재 인덱스에 해당하는 맵과 Schedule 설정 반환
const FCMStageRouteEntry* UCMStageRouteSubsystem::GetCurrentStage() const
{
    return IsStageRouteActive()
        ? StageRouteDefinition->GetStage(CurrentStageIndex)
        : nullptr;
}

// Travel 대상으로 예약된 다음 스테이지 설정 반환
const FCMStageRouteEntry* UCMStageRouteSubsystem::GetPendingStage() const
{
    return IsValid(StageRouteDefinition)
        ? StageRouteDefinition->GetStage(PendingStageIndex)
        : nullptr;
}
