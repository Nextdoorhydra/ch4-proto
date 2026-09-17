#include "AsyncLoad/CMStageLoadBarrierComponent.h"

#include "AsyncLoad/CMStageLoadLog.h"
#include "GameFramework/GameModeBase.h"
#include "Player/CMPlayerController.h"
#include "TimerManager.h"

// 새 요청의 완료 집합과 선택적 제한 시간 시작
void UCMStageLoadBarrierComponent::BeginBarrier(
    FGuid RequestId,
    bool bBlocking,
    float TimeoutSeconds,
    int32 ExpectedPlayerCount)
{
    CancelBarrier();
    ActiveRequestId = RequestId;
    bBlockingRequest = bBlocking;
    ActiveTimeout = FMath::Max(0.0f, TimeoutSeconds);
    ExpectedTargetPlayerCount = FMath::Max(0, ExpectedPlayerCount);
    RefreshProgress();
    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("Stage load barrier started. Request=%s Ready=%d Target=%d Timeout=%.1fs"),
        *ActiveRequestId.ToString(), ReadyControllers.Num(),
        GetTargetPlayerCount(), ActiveTimeout);
    if (bBlockingRequest && ActiveTimeout > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            TimeoutHandle, this, &ThisClass::HandleTimeout, ActiveTimeout, false);
    }
}

// 현재 요청과 일치하는 플레이어 결과만 집계
void UCMStageLoadBarrierComponent::ReportPlayerResult(
    ACMPlayerController* Controller,
    FGuid RequestId,
    bool bSucceeded)
{
    if (!IsValid(Controller) || RequestId != ActiveRequestId)
    {
        return;
    }
    if (!bSucceeded)
    {
        FinishFailure(
            ECMStageLoadState::Failed,
            ECMStageLoadFailureReason::ClientReportedFailure);
        return;
    }

    ReadyControllers.Add(Controller);
    RefreshProgress();
    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("Stage load player reported. Request=%s Player=%s Succeeded=true Ready=%d Target=%d"),
        *ActiveRequestId.ToString(), *GetNameSafe(Controller),
        ReadyControllers.Num(), GetTargetPlayerCount());
    if (AreAllPlayersReady())
    {
        const FGuid CompletedId = ActiveRequestId;
        GetWorld()->GetTimerManager().ClearTimer(TimeoutHandle);
        ActiveRequestId.Invalidate();
        OnBarrierCompleted.Broadcast(CompletedId);
    }
}

// 합류한 플레이어를 대상 인원에 반영
void UCMStageLoadBarrierComponent::HandlePlayerJoined()
{
    RefreshProgress();
}

// 퇴장 플레이어를 제거하고 남은 인원 기준으로 완료 여부 재평가
void UCMStageLoadBarrierComponent::HandlePlayerLeft(ACMPlayerController* Controller)
{
    ReadyControllers.Remove(Controller);
    RefreshProgress();
    if (ActiveRequestId.IsValid() && AreAllPlayersReady())
    {
        const FGuid CompletedId = ActiveRequestId;
        GetWorld()->GetTimerManager().ClearTimer(TimeoutHandle);
        ActiveRequestId.Invalidate();
        OnBarrierCompleted.Broadcast(CompletedId);
    }
}

// 현재 배리어와 타이머 상태 초기화
void UCMStageLoadBarrierComponent::CancelBarrier()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimeoutHandle);
    }
    ActiveRequestId.Invalidate();
    ReadyControllers.Reset();
    bBlockingRequest = false;
    ActiveTimeout = 0.0f;
}

// 현재 월드의 Chimera PlayerController 수 계산
int32 UCMStageLoadBarrierComponent::GetTargetPlayerCount() const
{
    if (ExpectedTargetPlayerCount > 0)
    {
        return ExpectedTargetPlayerCount;
    }

    int32 Count = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        Count += IsValid(Cast<ACMPlayerController>(It->Get())) ? 1 : 0;
    }
    return Count;
}

// GameState의 UI용 집계 값 갱신
void UCMStageLoadBarrierComponent::RefreshProgress()
{
    if (!ActiveRequestId.IsValid())
    {
        return;
    }
    if (ACMPlayGameState* State = GetWorld()->GetGameState<ACMPlayGameState>())
    {
        State->SetStageLoadStatus(
            ECMStageLoadState::Loading,
            ECMStageLoadFailureReason::None,
            ReadyControllers.Num(),
            GetTargetPlayerCount(),
            bBlockingRequest ? ActiveTimeout : 0.0f);
    }
}

// 모든 현재 대상 Controller가 성공 집합에 포함됐는지 확인
bool UCMStageLoadBarrierComponent::AreAllPlayersReady() const
{
    const int32 TargetCount = GetTargetPlayerCount();
    return TargetCount > 0 && ReadyControllers.Num() >= TargetCount;
}

// Blocking 요청 제한 시간 초과 처리
void UCMStageLoadBarrierComponent::HandleTimeout()
{
    UE_LOG(LogChimeraStageLoad, Error,
        TEXT("Stage load barrier timed out. Request=%s Ready=%d Target=%d Timeout=%.1fs"),
        *ActiveRequestId.ToString(), ReadyControllers.Num(),
        GetTargetPlayerCount(), ActiveTimeout);
    FinishFailure(ECMStageLoadState::TimedOut, ECMStageLoadFailureReason::TimedOut);
}

// 실패 Snapshot을 갱신하고 GameMode 결정자에게 실패 전달
void UCMStageLoadBarrierComponent::FinishFailure(
    ECMStageLoadState State,
    ECMStageLoadFailureReason Reason)
{
    const FGuid FailedId = ActiveRequestId;
    if (ACMPlayGameState* GameState = GetWorld()->GetGameState<ACMPlayGameState>())
    {
        GameState->SetStageLoadStatus(
            State, Reason, ReadyControllers.Num(), GetTargetPlayerCount());
    }
    CancelBarrier();
    OnBarrierFailed.Broadcast(FailedId, State, Reason);
}
