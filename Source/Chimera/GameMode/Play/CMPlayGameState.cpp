#include "GameMode/Play/CMPlayGameState.h"

#include "Net/UnrealNetwork.h"

// 플레이 흐름과 로드 Snapshot을 네트워크 복제 대상으로 등록
void ACMPlayGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACMPlayGameState, PlayPhase);
    DOREPLIFETIME(ACMPlayGameState, CurrentStageIndex);
    DOREPLIFETIME(ACMPlayGameState, TotalStageCount);
    DOREPLIFETIME(ACMPlayGameState, PhaseStartServerTime);
    DOREPLIFETIME(ACMPlayGameState, PhaseDuration);
    DOREPLIFETIME(ACMPlayGameState, CompletedStageTime);
    DOREPLIFETIME(ACMPlayGameState, StageLoadSnapshot);
    DOREPLIFETIME(ACMPlayGameState, StagePresentationState);
}

// 서버에서 모든 머신이 재생할 스테이지 연출 상태 갱신
void ACMPlayGameState::SetStagePresentationState(ECMStagePresentationState NewState)
{
    if (!HasAuthority() || StagePresentationState == NewState)
    {
        return;
    }
    StagePresentationState = NewState;
    OnRep_StagePresentationState();
    ForceNetUpdate();
}

// 새 로드 요청과 최초 진행 상태를 하나의 복제 Snapshot으로 시작
void ACMPlayGameState::BeginStageLoadRequest(
    const FCMStageLoadRequest& NewRequest,
    int32 NewTargetPlayerCount,
    float NewTimeoutDuration)
{
    if (!HasAuthority() || !NewRequest.IsValid())
    {
        return;
    }

    StageLoadSnapshot.Request = NewRequest;
    StageLoadSnapshot.State = ECMStageLoadState::Loading;
    StageLoadSnapshot.FailureReason = ECMStageLoadFailureReason::None;
    StageLoadSnapshot.ReadyPlayerCount = 0;
    StageLoadSnapshot.TargetPlayerCount = FMath::Max(0, NewTargetPlayerCount);
    StageLoadSnapshot.StartServerTime = GetServerWorldTimeSeconds();
    StageLoadSnapshot.TimeoutDuration = FMath::Max(0.0f, NewTimeoutDuration);
    ++StageLoadSnapshot.Revision;
    OnRep_StageLoadSnapshot();
    ForceNetUpdate();
}

// 같은 요청의 결과와 UI용 진행도를 Snapshot 단위로 갱신
void ACMPlayGameState::SetStageLoadStatus(
    ECMStageLoadState NewState,
    ECMStageLoadFailureReason NewFailureReason,
    int32 NewReadyPlayerCount,
    int32 NewTargetPlayerCount,
    float NewTimeoutDuration)
{
    if (!HasAuthority())
    {
        return;
    }

    StageLoadSnapshot.State = NewState;
    StageLoadSnapshot.FailureReason = NewFailureReason;
    StageLoadSnapshot.ReadyPlayerCount = FMath::Max(0, NewReadyPlayerCount);
    StageLoadSnapshot.TargetPlayerCount = FMath::Max(0, NewTargetPlayerCount);
    StageLoadSnapshot.TimeoutDuration = FMath::Max(0.0f, NewTimeoutDuration);
    ++StageLoadSnapshot.Revision;
    OnRep_StageLoadSnapshot();
    ForceNetUpdate();
}

// 동기화된 서버 시간을 기준으로 Blocking 로드 잔여 시간 계산
float ACMPlayGameState::GetStageLoadRemainingTime() const
{
    if (StageLoadSnapshot.State != ECMStageLoadState::Loading
        || StageLoadSnapshot.TimeoutDuration <= 0.0f)
    {
        return 0.0f;
    }

    return FMath::Max(0.0,
        StageLoadSnapshot.TimeoutDuration
            - (GetServerWorldTimeSeconds() - StageLoadSnapshot.StartServerTime));
}

// 서버에서 플레이 Phase와 제한 시간 시작점 갱신
void ACMPlayGameState::SetPlayPhase(ECMPlayPhase NewPhase, float NewDuration)
{
    if (!HasAuthority())
    {
        return;
    }

    NewDuration = FMath::Max(0.0f, NewDuration);
    if (PlayPhase == NewPhase && PhaseDuration == NewDuration)
    {
        return;
    }

    const double CurrentServerTime = GetServerWorldTimeSeconds();
    if (PlayPhase == ECMPlayPhase::Playing
        && NewPhase == ECMPlayPhase::Completed)
    {
        CompletedStageTime = FMath::Max(
            0.0,
            CurrentServerTime - PhaseStartServerTime);
    }
    else if (NewPhase == ECMPlayPhase::Playing)
    {
        CompletedStageTime = 0.0f;
    }

    PlayPhase = NewPhase;
    PhaseDuration = NewDuration;
    PhaseStartServerTime = CurrentServerTime;
    OnRep_PlayState();
    ForceNetUpdate();
}

// 서버에서 현재 인덱스와 전체 스테이지 수를 유효 범위로 갱신
void ACMPlayGameState::SetStageProgress(int32 NewStageIndex, int32 NewStageCount)
{
    if (!HasAuthority())
    {
        return;
    }

    NewStageCount = FMath::Max(1, NewStageCount);
    NewStageIndex = FMath::Clamp(NewStageIndex, 0, NewStageCount - 1);
    if (CurrentStageIndex == NewStageIndex && TotalStageCount == NewStageCount)
    {
        return;
    }

    CurrentStageIndex = NewStageIndex;
    TotalStageCount = NewStageCount;
    OnRep_PlayState();
    ForceNetUpdate();
}

// 동기화된 서버 시간을 기준으로 현재 Phase 잔여 시간 계산
float ACMPlayGameState::GetPhaseRemainingTime() const
{
    if (PhaseDuration <= 0.0f)
    {
        return 0.0f;
    }
    return FMath::Max(0.0,
        PhaseDuration - (GetServerWorldTimeSeconds() - PhaseStartServerTime));
}

// 복제된 플레이 상태 변경을 UI 구독자에게 전달
void ACMPlayGameState::OnRep_PlayState()
{
    OnPlayStateChanged.Broadcast();
}

// 복제된 요청과 로드 진행도의 동일 Revision을 로더와 UI에 전달
void ACMPlayGameState::OnRep_StageLoadSnapshot()
{
    OnStageLoadRequestChanged.Broadcast(StageLoadSnapshot.Request);
    OnStageLoadStatusChanged.Broadcast();
}

// 복제된 연출 상태를 레벨 구독자에게 전달
void ACMPlayGameState::OnRep_StagePresentationState()
{
    OnStagePresentationChanged.Broadcast();
}
