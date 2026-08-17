#include "GameMode/Play/CMPlayGameMode.h"

#include "GameMode/Play/CMPlayGameState.h"
#include "GameMode/Campaign/CMCampaignDefinition.h"
#include "GameMode/Campaign/CMCampaignSubsystem.h"
#include "Stage/CMStageDirector.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "AsyncLoad/CMStageLoadBarrierComponent.h"
#include "Player/CMPlayerController.h"
#include "TimerManager.h"

// 플레이 전용 GameState와 스테이지 간 Seamless Travel 설정
ACMPlayGameMode::ACMPlayGameMode()
{
    GameStateClass = ACMPlayGameState::StaticClass();
    bUseSeamlessTravel = true;
    StageLoadBarrier = CreateDefaultSubobject<UCMStageLoadBarrierComponent>(
        TEXT("StageLoadBarrier"));
    StageLoadBarrier->OnBarrierCompleted.AddUObject(
        this, &ThisClass::HandleLoadBarrierCompleted);
    StageLoadBarrier->OnBarrierFailed.AddUObject(
        this, &ThisClass::HandleLoadBarrierFailed);
}

// 플레이 맵 초기화 후 참가 플레이어 준비 대기 Phase 진입
void ACMPlayGameMode::BeginPlay()
{
    Super::BeginPlay();

    UCMCampaignSubsystem* Campaign = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMCampaignSubsystem>()
        : nullptr;
    if (Campaign && !Campaign->IsCampaignActive() && DefaultCampaignDefinition)
    {
        if (!Campaign->StartCampaign(DefaultCampaignDefinition))
        {
            UE_LOG(LogChimeraStageLoad, Error,
                TEXT("Could not start default campaign. Campaign=%s"),
                *GetPathNameSafe(DefaultCampaignDefinition));
        }
    }


    if (Campaign && Campaign->GetPendingStageIndex() != INDEX_NONE)
    {
        const FCMCampaignStageDefinition* PendingStage = Campaign->GetPendingStage();
        const FString CurrentMap = GetWorld()->GetOutermost()->GetName();
        const FString PendingMap = PendingStage
            ? PendingStage->StageMap.ToSoftObjectPath().GetLongPackageName()
            : FString();
        if (CurrentMap == PendingMap)
        {
            Campaign->CommitPendingStage();
        }
        else
        {
            UE_LOG(LogChimeraStageLoad, Error,
                TEXT("Pending campaign map does not match loaded world. Current=%s Pending=%s"),
                *CurrentMap, *PendingMap);
        }
    }

    int32 InitialStageIndex = 0;
    int32 StageCount = StageLoadScheduleIds.Num();
    if (Campaign && Campaign->IsCampaignActive())
    {
        InitialStageIndex = Campaign->GetCurrentStageIndex();
        StageCount = Campaign->GetStageCount();
    }

    if (StageCount > 0)
    {
        InitializeStageProgress(InitialStageIndex, StageCount);
    }

    const FPrimaryAssetId FirstScheduleId = GetStageLoadScheduleId(InitialStageIndex);
    if (FirstScheduleId.IsValid())
    {
        SetPlayPhase(ECMPlayPhase::Loading);
        PublishStageLoadRequest(FirstScheduleId);
    }
    else
    {
        UE_LOG(LogChimeraStageLoad, Warning,
            TEXT("Initial stage load Schedule is not set; skipping entry load barrier. GameMode=%s"),
            *GetPathName());
        SetPlayPhase(ECMPlayPhase::WaitingForPlayers);
    }
}

// GameMode 종료 시 이전 월드의 로드 타임아웃 제거
void ACMPlayGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StageLoadBarrier->CancelBarrier();
    Super::EndPlay(EndPlayReason);
}

// 로드 진행 중 합류한 플레이어를 현재 배리어 대상 수에 반영
void ACMPlayGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    StageLoadBarrier->HandlePlayerJoined();
}

// 퇴장한 플레이어를 완료 집합에서 제거하고 남은 인원 기준으로 배리어 재평가
void ACMPlayGameMode::Logout(AController* Exiting)
{
    if (ACMPlayerController* PlayerController = Cast<ACMPlayerController>(Exiting))
    {
        StageLoadBarrier->HandlePlayerLeft(PlayerController);
    }

    Super::Logout(Exiting);
}

// 서버의 플레이 Phase 변경 요청을 PlayGameState에 전달
void ACMPlayGameMode::SetPlayPhase(
    ECMPlayPhase NewPhase,
    float Duration
)
{
    if (ACMPlayGameState* PlayState =
        GetGameState<ACMPlayGameState>())
    {
        PlayState->SetPlayPhase(NewPhase, Duration);
    }
}

// 현재 스테이지 인덱스와 전체 스테이지 수를 PlayGameState에 전달
void ACMPlayGameMode::InitializeStageProgress(
    int32 StageIndex,
    int32 StageCount
)
{
    if (ACMPlayGameState* PlayState =
        GetGameState<ACMPlayGameState>())
    {
        PlayState->SetStageProgress(StageIndex, StageCount);
    }
}

// 현재 맵에서 유효한 StageDirector 하나만 등록
bool ACMPlayGameMode::RegisterStageDirector(
    ACMStageDirector* NewStageDirector
)
{
    if (!HasAuthority() || !IsValid(NewStageDirector))
    {
        return false;
    }

    if (IsValid(StageDirector) && StageDirector != NewStageDirector)
    {
        UE_LOG(
            LogChimeraStageLoad,
            Error,
            TEXT("Multiple StageDirectors found. Current=%s New=%s"),
            *GetNameSafe(StageDirector),
            *GetNameSafe(NewStageDirector)
        );
        return false;
    }

    StageDirector = NewStageDirector;
    return true;
}

// 준비 대기 상태를 검증하고 StageDirector에 시작 연출 요청
bool ACMPlayGameMode::StartStage()
{
    ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>();
    if (!HasAuthority()
        || !PlayState
        || !IsValid(StageDirector)
        || PlayState->GetPlayPhase() != ECMPlayPhase::WaitingForPlayers)
    {
        return false;
    }

    SetPlayPhase(ECMPlayPhase::Starting);
    PlayState->SetStagePresentationState(ECMStagePresentationState::Starting);
    return true;
}

// 현재 요청과 성공 보고자를 검증하고 전원 완료 시 Blocking Phase를 해제
void ACMPlayGameMode::HandleStageLoadComplete(
    ACMPlayerController* ReportingController,
    FGuid RequestId,
    bool bSucceeded)
{
    if (!HasAuthority() || !IsValid(ReportingController)
        || RequestId != ActiveStageLoadRequestId)
    {
        return;
    }

    if (!bSucceeded)
    {
        const ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>();
        const FCMStageLoadRequest* Request = PlayState
            ? &PlayState->GetStageLoadRequest()
            : nullptr;
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Stage load barrier report failed. Schedule=%s Player=%s Request=%s"),
            Request ? *Request->ScheduleId.ToString() : TEXT("None"),
            *GetNameSafe(ReportingController), *RequestId.ToString());
        StageLoadBarrier->ReportPlayerResult(
            ReportingController, RequestId, false);
        return;
    }

    StageLoadBarrier->ReportPlayerResult(
        ReportingController, RequestId, true);
}

// GameState에 새 요청을 게시하고 Blocking 요청이면 완료 배리어를 초기화
bool ACMPlayGameMode::PublishStageLoadRequest(
    FPrimaryAssetId ScheduleId)
{
    ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>();
    if (!HasAuthority() || !PlayState || !ScheduleId.IsValid())
    {
        return false;
    }

    FCMQueuedStageLoadRequest Request;
    Request.ScheduleId = ScheduleId;

    if (ActiveStageLoadRequestId.IsValid())
    {
        QueuedStageLoadRequests.Add(Request);
        UE_LOG(LogChimeraStageLoad, Verbose,
            TEXT("Stage load request queued. Schedule=%s QueueDepth=%d"),
            *ScheduleId.ToString(),
            QueuedStageLoadRequests.Num());
        return true;
    }

    return StartStageLoadRequest(Request);
}

// 서버 직렬 큐의 첫 요청을 GameState 복제와 완료 배리어에 연결
bool ACMPlayGameMode::StartStageLoadRequest(
    const FCMQueuedStageLoadRequest& QueuedRequest)
{
    ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>();
    if (!HasAuthority() || !PlayState || !QueuedRequest.ScheduleId.IsValid())
    {
        return false;
    }

    FCMStageLoadRequest Request;
    Request.RequestId = FGuid::NewGuid();
    Request.ScheduleId = QueuedRequest.ScheduleId;
    Request.bBlocking = true;

    ActiveStageLoadRequestId = Request.RequestId;
    PlayState->BeginStageLoadRequest(
        Request,
        StageLoadBarrier->GetTargetPlayerCount(),
        StageLoadTimeout);
    StageLoadBarrier->BeginBarrier(
        Request.RequestId,
        true,
        StageLoadTimeout);
    return true;
}

// 배리어 완료를 현재 GameMode 요청 종료 정책에 연결
void ACMPlayGameMode::HandleLoadBarrierCompleted(FGuid RequestId)
{
    if (RequestId == ActiveStageLoadRequestId)
    {
        CompleteActiveStageLoad();
    }
}

// 배리어 실패 시 현재 요청과 대기 큐를 종료 상태로 정리
void ACMPlayGameMode::HandleLoadBarrierFailed(
    FGuid RequestId,
    ECMStageLoadState State,
    ECMStageLoadFailureReason Reason)
{
    if (RequestId != ActiveStageLoadRequestId)
    {
        return;
    }
    ActiveStageLoadRequestId.Invalidate();
    QueuedStageLoadRequests.Reset();
}

// 현재 요청 종료 후 대기 중인 다음 요청 하나를 순서대로 시작
void ACMPlayGameMode::StartNextQueuedStageLoadRequest()
{
    if (ActiveStageLoadRequestId.IsValid() || QueuedStageLoadRequests.IsEmpty())
    {
        return;
    }

    const FCMQueuedStageLoadRequest NextRequest = QueuedStageLoadRequests[0];
    QueuedStageLoadRequests.RemoveAt(0);
    if (!StartStageLoadRequest(NextRequest))
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Queued stage load request could not start. Schedule=%s"),
            *NextRequest.ScheduleId.ToString());
    }
}

// 배열이 설정되면 스테이지별 Schedule을 사용하고 기존 단일 설정은 첫 스테이지 fallback으로 사용
FPrimaryAssetId ACMPlayGameMode::GetStageLoadScheduleId(int32 StageIndex) const
{
    const UCMCampaignSubsystem* Campaign = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMCampaignSubsystem>()
        : nullptr;
    if (Campaign && Campaign->IsCampaignActive()
        && Campaign->GetCurrentStageIndex() == StageIndex)
    {
        const FCMCampaignStageDefinition* Stage = Campaign->GetCurrentStage();
        return Stage ? Stage->LoadScheduleId : FPrimaryAssetId();
    }

    if (StageLoadScheduleIds.IsValidIndex(StageIndex))
    {
        return StageLoadScheduleIds[StageIndex];
    }
    return StageIndex == 0 ? InitialStageLoadScheduleId : FPrimaryAssetId();
}

// 현재 요청을 Ready로 종료하고 Blocking 요청만 플레이 준비 Phase로 전환
void ACMPlayGameMode::CompleteActiveStageLoad()
{
    if (ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>())
    {
        PlayState->SetStageLoadStatus(
            ECMStageLoadState::Ready,
            ECMStageLoadFailureReason::None,
            StageLoadBarrier->GetReadyPlayerCount(),
            StageLoadBarrier->GetTargetPlayerCount());
    }

    ActiveStageLoadRequestId.Invalidate();

    StartNextQueuedStageLoadRequest();

    SetPlayPhase(ECMPlayPhase::WaitingForPlayers);
}

// 기술적 실패 상태를 게임 플레이 Failed Phase와 분리해 요청 종료
void ACMPlayGameMode::FailActiveStageLoad(
    ECMStageLoadState State,
    ECMStageLoadFailureReason Reason)
{
    StageLoadBarrier->CancelBarrier();
    if (ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>())
    {
        PlayState->SetStageLoadStatus(
            State,
            Reason,
            StageLoadBarrier->GetReadyPlayerCount(),
            StageLoadBarrier->GetTargetPlayerCount());
    }

    ActiveStageLoadRequestId.Invalidate();
    QueuedStageLoadRequests.Reset();
}

// 등록된 Director의 보고만 받아 Starting에서 Playing으로 전환
void ACMPlayGameMode::HandleStartingPresentationFinished(
    ACMStageDirector* ReportingDirector
)
{
    ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>();
    if (!IsCurrentStageDirector(ReportingDirector)
        || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Starting)
    {
        return;
    }

    SetPlayPhase(ECMPlayPhase::Playing);
    PlayState->SetStagePresentationState(ECMStagePresentationState::None);
}

// 등록된 Director의 결과 연출 보고만 받아 다음 스테이지 인덱스 진행
void ACMPlayGameMode::HandleResultPresentationFinished(
    ACMStageDirector* ReportingDirector)
{
    const ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>();
    if (!IsCurrentStageDirector(ReportingDirector)
        || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Completed)
    {
        return;
    }

    if (ACMPlayGameState* MutablePlayState = GetGameState<ACMPlayGameState>())
    {
        MutablePlayState->SetStagePresentationState(ECMStagePresentationState::None);
    }
    AdvanceToNextStage();
}

// 별도 레벨 전환 구현이 없으면 같은 월드에서 즉시 Entry 로드 단계로 연결
void ACMPlayGameMode::BeginStageTransition_Implementation(int32 NextStageIndex)
{
    UCMCampaignSubsystem* Campaign = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMCampaignSubsystem>()
        : nullptr;
    if (!Campaign || !Campaign->IsCampaignActive())
    {
        FinishStageTransition();
        return;
    }

    const FCMCampaignStageDefinition* Stage = Campaign->GetPendingStage();
    if (!Stage || Campaign->GetPendingStageIndex() != NextStageIndex)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Campaign stage transition state is invalid. Requested=%d Current=%d"),
            NextStageIndex, Campaign->GetPendingStageIndex());
        FailActiveStageLoad(
            ECMStageLoadState::Failed,
            ECMStageLoadFailureReason::InvalidConfiguration);
        return;
    }

    const FString MapPackageName = Stage->StageMap.ToSoftObjectPath().GetLongPackageName();
    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("Starting seamless stage travel. Stage=%d StageId=%s Map=%s Schedule=%s"),
        NextStageIndex, *Stage->StageId.ToString(), *MapPackageName,
        *Stage->LoadScheduleId.ToString());
    if (MapPackageName.IsEmpty() || !GetWorld()->ServerTravel(MapPackageName, false))
    {
        Campaign->CancelPendingStage();
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("ServerTravel failed. Stage=%d StageId=%s Map=%s"),
            NextStageIndex, *Stage->StageId.ToString(), *MapPackageName);
        FailActiveStageLoad(
            ECMStageLoadState::Failed,
            ECMStageLoadFailureReason::InvalidConfiguration);
    }
}

// 전환 대상 스테이지의 Entry Schedule을 전원 완료 배리어로 시작
void ACMPlayGameMode::FinishStageTransition()
{
    if (!HasAuthority() || PendingStageTransitionIndex == INDEX_NONE)
    {
        return;
    }

    const int32 StageIndex = PendingStageTransitionIndex;
    PendingStageTransitionIndex = INDEX_NONE;
    const FPrimaryAssetId ScheduleId = GetStageLoadScheduleId(StageIndex);
    if (!PublishStageLoadRequest(ScheduleId))
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Could not publish next stage entry load. Stage=%d Schedule=%s"),
            StageIndex, *ScheduleId.ToString());
        FailActiveStageLoad(
            ECMStageLoadState::Failed,
            ECMStageLoadFailureReason::InvalidConfiguration);
    }
}

// 현재 진행도를 다음 인덱스로 갱신하고 실제 레벨 전환 경계를 호출
void ACMPlayGameMode::AdvanceToNextStage()
{
    ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>();
    if (!HasAuthority() || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Completed)
    {
        return;
    }

    int32 NextStageIndex = PlayState->GetCurrentStageIndex() + 1;
    if (NextStageIndex >= PlayState->GetTotalStageCount())
    {
        SetPlayPhase(ECMPlayPhase::Victory);
        return;
    }

    UCMCampaignSubsystem* Campaign = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMCampaignSubsystem>()
        : nullptr;
    if (Campaign && Campaign->IsCampaignActive())
    {
        if (!Campaign->PrepareNextStage())
        {
            UE_LOG(LogChimeraStageLoad, Error,
                TEXT("Campaign could not advance from stage %d."),
                PlayState->GetCurrentStageIndex());
            return;
        }
        NextStageIndex = Campaign->GetPendingStageIndex();
    }

    PendingStageTransitionIndex = NextStageIndex;
    StageDirector = nullptr;
    InitializeStageProgress(NextStageIndex, PlayState->GetTotalStageCount());
    SetPlayPhase(ECMPlayPhase::Loading);
    BeginStageTransition(NextStageIndex);
}

// 등록된 Director의 클리어 보고를 Completed 또는 Victory로 확정
void ACMPlayGameMode::HandleStageCompleted(
    ACMStageDirector* ReportingDirector
)
{
    ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>();
    if (!IsCurrentStageDirector(ReportingDirector)
        || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Playing)
    {
        return;
    }

    const bool bLastStage =
        PlayState->GetCurrentStageIndex() + 1
        >= PlayState->GetTotalStageCount();
    if (bLastStage)
    {
        SetPlayPhase(ECMPlayPhase::Victory);
        return;
    }

    SetPlayPhase(ECMPlayPhase::Completed);
    if (IsValid(StageDirector))
    {
        PlayState->SetStagePresentationState(ECMStagePresentationState::Result);
    }
    else
    {
        AdvanceToNextStage();
    }
}

// 등록된 Director의 실패 보고를 현재 스테이지 Failed로 확정
void ACMPlayGameMode::HandleStageFailed(
    ACMStageDirector* ReportingDirector
)
{
    const ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>();
    if (!IsCurrentStageDirector(ReportingDirector)
        || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Playing)
    {
        return;
    }

    SetPlayPhase(ECMPlayPhase::Failed);
}

// 현재 스테이지 실패와 구분되는 게임 전체 패배 확정
void ACMPlayGameMode::ConfirmGameDefeat()
{
    if (HasAuthority())
    {
        SetPlayPhase(ECMPlayPhase::Defeat);
    }
}

// 최종 결과 상태에서 엔딩 표시 상태로 전환
void ACMPlayGameMode::StartEnding()
{
    const ACMPlayGameState* PlayState = GetGameState<ACMPlayGameState>();
    if (!HasAuthority() || !PlayState)
    {
        return;
    }

    const ECMPlayPhase CurrentPhase = PlayState->GetPlayPhase();
    if (CurrentPhase == ECMPlayPhase::Victory
        || CurrentPhase == ECMPlayPhase::Defeat)
    {
        SetPlayPhase(ECMPlayPhase::Ending);
    }
}

// 보고자가 현재 맵에 등록된 서버 StageDirector인지 확인
bool ACMPlayGameMode::IsCurrentStageDirector(
    const ACMStageDirector* Director
) const
{
    return HasAuthority()
        && IsValid(Director)
        && StageDirector == Director;
}
