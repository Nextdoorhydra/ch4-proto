#include "GameMode/Play/CMPlayGameMode.h"

#include "GameMode/Play/CMPlayGameState.h"
#include "GameMode/StageRoute/CMStageRouteDefinition.h"
#include "GameMode/StageRoute/CMStageRouteSubsystem.h"
#include "Stage/CMStageDirector.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "AsyncLoad/CMStageLoadBarrierComponent.h"
#include "Player/CMChimera.h"
#include "Player/CMControlBody.h"
#include "Player/CMPlayerController.h"
#include "Player/CMPlayerState.h"
#include "Engine/World.h"
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
    CachedPlayGameState = GetGameState<ACMPlayGameState>();
    if (!CachedPlayGameState)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("PlayGameState를 캐시하지 못해 플레이 맵을 초기화할 수 없습니다."));
        return;
    }
    BindSharedChimeraEvents();

    UCMStageRouteSubsystem* StageRoute = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMStageRouteSubsystem>()
        : nullptr;
    if (!StageRoute)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("StageRouteSubsystem을 찾을 수 없어 플레이 맵을 초기화하지 못했습니다."));
        return;
    }

    if (!StageRoute->IsStageRouteActive()
        && !TryBootstrapDirectStageRoute(StageRoute))
    {
        return;
    }


    if (StageRoute->GetPendingStageIndex() != INDEX_NONE)
    {
        const FCMStageRouteEntry* PendingStage = StageRoute->GetPendingStage();
        const FString CurrentMap = UWorld::RemovePIEPrefix(
            GetWorld()->GetOutermost()->GetName());
        const FString PendingMap = PendingStage
            ? PendingStage->StageMap.ToSoftObjectPath().GetLongPackageName()
            : FString();
        if (CurrentMap == PendingMap)
        {
            StageRoute->CommitPendingStage();
        }
        else
        {
            UE_LOG(LogChimeraStageLoad, Error,
                TEXT("전환 대상으로 예약한 맵과 현재 월드가 일치하지 않습니다. Current=%s Pending=%s"),
                *CurrentMap, *PendingMap);
            StageRoute->CancelPendingStage();
            return;
        }
    }

    int32 InitialStageIndex = 0;
    int32 StageCount = StageLoadScheduleIds.Num();
    if (StageRoute && StageRoute->IsStageRouteActive())
    {
        InitialStageIndex = StageRoute->GetCurrentStageIndex();
        StageCount = StageRoute->GetStageCount();
    }

    if (StageCount > 0)
    {
        InitializeStageProgress(InitialStageIndex, StageCount);
    }

    const FPrimaryAssetId FirstScheduleId = GetStageLoadScheduleId(InitialStageIndex);
    if (FirstScheduleId.IsValid())
    {
        bStageLoadReady = false;
        SetPlayPhase(ECMPlayPhase::Loading);
        PublishStageLoadRequest(FirstScheduleId);
    }
    else
    {
        UE_LOG(LogChimeraStageLoad, Warning,
            TEXT("초기 스테이지 로드 Schedule이 없어 진입 로드 배리어를 생략합니다. GameMode=%s"),
            *GetPathName());
        bStageLoadReady = true;
        SetPlayPhase(ECMPlayPhase::WaitingForPlayers);
        TryStartStageWhenReady();
    }
}

// 에디터에서 플레이 맵을 직접 실행했을 때 현재 맵과 일치하는 스테이지부터 시작
bool ACMPlayGameMode::TryBootstrapDirectStageRoute(
    UCMStageRouteSubsystem* StageRoute)
{
    if (!StageRoute || !DefaultStageRouteDefinition)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("직접 실행할 DefaultStageRouteDefinition이 설정되지 않았습니다. GameMode=%s"),
            *GetPathName());
        return false;
    }

#if WITH_EDITOR
    const FString CurrentMapPackageName = UWorld::RemovePIEPrefix(
        GetWorld()->GetOutermost()->GetName());
    const int32 StageIndex =
        DefaultStageRouteDefinition->FindStageIndexByMap(CurrentMapPackageName);
    if (StageIndex == INDEX_NONE)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("직접 실행한 맵이 StageRoute에 등록되지 않았습니다. Map=%s StageRoute=%s"),
            *CurrentMapPackageName,
            *GetPathNameSafe(DefaultStageRouteDefinition));
        return false;
    }

    if (!StageRoute->StartStageRouteAtIndex(
        DefaultStageRouteDefinition,
        StageIndex))
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("현재 맵의 스테이지 경로를 시작하지 못했습니다. Map=%s StageIndex=%d StageRoute=%s"),
            *CurrentMapPackageName,
            StageIndex,
            *GetPathNameSafe(DefaultStageRouteDefinition));
        return false;
    }

    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("에디터 직접 실행 스테이지를 시작했습니다. Map=%s StageIndex=%d StageRoute=%s"),
        *CurrentMapPackageName,
        StageIndex,
        *GetPathNameSafe(DefaultStageRouteDefinition));
    return true;
#else
    UE_LOG(LogChimeraStageLoad, Error,
        TEXT("정식 실행에서는 로비가 스테이지 경로를 먼저 시작해야 합니다. Map=%s"),
        *GetWorld()->GetOutermost()->GetName());
    return false;
#endif
}

// GameMode 종료 시 이전 월드의 로드 타임아웃 제거
void ACMPlayGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StageLoadBarrier->CancelBarrier();
    GetWorldTimerManager().ClearTimer(StartingPresentationTimeoutHandle);
    GetWorldTimerManager().ClearTimer(StageLoopRestartTimerHandle);
    for (TPair<FString, FCMDisconnectedPlayerRecord>& Pair : DisconnectedPlayers)
    {
        GetWorldTimerManager().ClearTimer(Pair.Value.ExpirationTimer);
    }
    DisconnectedPlayers.Reset();
    ExpiredReconnectKeys.Reset();
    if (const ACMPlayGameState* PlayState = CachedPlayGameState)
    {
        if (ACMChimera* SharedChimera = PlayState->SharedChimera)
        {
            SharedChimera->OnAllSegmentsDead.RemoveAll(this);
        }
    }
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

    ACMPlayerState* PlayerState = Exiting
        ? Exiting->GetPlayerState<ACMPlayerState>()
        : nullptr;
    ACMControlBody* ControlBody = Exiting
        ? Cast<ACMControlBody>(Exiting->GetPawn())
        : nullptr;
    const FString ReconnectKey = BuildReconnectKey(PlayerState);
    if (!ReconnectKey.IsEmpty() && PlayerState && ControlBody)
    {
        ControlBody->ClearPressedControlSlots();

        FCMDisconnectedPlayerRecord& Record =
            DisconnectedPlayers.FindOrAdd(ReconnectKey);
        Record.PlayerSlotId = PlayerState->GetPlayerSlotId();
        Record.PlayerColorIndex = PlayerState->GetPlayerColorIndex();
        Record.OwnedSegmentIndex = ControlBody->GetOwnedSegmentIndex();
        Record.ControlSlots = ControlBody->GetControlSlots();
        GetWorldTimerManager().ClearTimer(Record.ExpirationTimer);

        FTimerDelegate ExpirationDelegate;
        ExpirationDelegate.BindUObject(
            this, &ThisClass::ExpireDisconnectedPlayer, ReconnectKey);
        GetWorldTimerManager().SetTimer(
            Record.ExpirationTimer,
            ExpirationDelegate,
            ReconnectGracePeriod,
            false);

        UE_LOG(LogChimeraStageLoad, Warning,
            TEXT("플레이어 재접속 유예를 시작합니다. Player=%s Grace=%.1fs"),
            *PlayerState->GetPlayerName(), ReconnectGracePeriod);
    }

    Super::Logout(Exiting);
}

// 진행 중 신규 합류자는 현재 스테이지에서 ControlBody를 만들지 않고 관전
void ACMPlayGameMode::RestartPlayer(AController* NewPlayer)
{
    ACMPlayerState* PlayerState = NewPlayer
        ? NewPlayer->GetPlayerState<ACMPlayerState>()
        : nullptr;
    if (ShouldSpectateCurrentStage(PlayerState))
    {
        PlayerState->SetParticipationState(
            ECMPlayerParticipationState::Spectating);
        if (APlayerController* PlayerController = Cast<APlayerController>(NewPlayer))
        {
            PlayerController->StartSpectatingOnly();
            if (const ACMPlayGameState* PlayState = CachedPlayGameState)
            {
                if (PlayState->SharedChimera)
                {
                    PlayerController->ClientSetViewTarget(PlayState->SharedChimera);
                }
            }
        }
        return;
    }

    Super::RestartPlayer(NewPlayer);
}

// Logout 직전에 저장된 플레이어만 기본 즉시 재배정에서 제외
bool ACMPlayGameMode::ShouldPreservePlayerOnLogout(
    AController* Exiting,
    const ACMPlayerState* ExitingPlayerState) const
{
    const FString ReconnectKey = BuildReconnectKey(ExitingPlayerState);
    return !ReconnectKey.IsEmpty()
        && DisconnectedPlayers.Contains(ReconnectKey);
}

// 재접속한 ControlBody와 PlayerState에 기존 슬롯·색상·담당 마디 복원
bool ACMPlayGameMode::RestorePreservedControlAssignment(
    AController* NewPlayer,
    ACMChimera* SharedChimera)
{
    ACMPlayerState* PlayerState = NewPlayer
        ? NewPlayer->GetPlayerState<ACMPlayerState>()
        : nullptr;
    ACMControlBody* ControlBody = NewPlayer
        ? Cast<ACMControlBody>(NewPlayer->GetPawn())
        : nullptr;
    const FString ReconnectKey = BuildReconnectKey(PlayerState);
    FCMDisconnectedPlayerRecord* Record =
        DisconnectedPlayers.Find(ReconnectKey);
    if (!Record || !PlayerState || !ControlBody)
    {
        return false;
    }

    GetWorldTimerManager().ClearTimer(Record->ExpirationTimer);
    PlayerState->SetPlayerSlotId(Record->PlayerSlotId);
    PlayerState->SetPlayerColorIndex(Record->PlayerColorIndex);
    PlayerState->SetParticipationState(ECMPlayerParticipationState::Active);
    ControlBody->SetOwnedSegmentIndex(Record->OwnedSegmentIndex);
    ControlBody->SetControlSlots(Record->ControlSlots);
    DisconnectedPlayers.Remove(ReconnectKey);
    ExpiredReconnectKeys.Remove(ReconnectKey);

    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("유예 시간 안에 재접속하여 기존 조작 배정을 복원했습니다. Player=%s"),
        *PlayerState->GetPlayerName());
    return true;
}

// Steam UniqueId를 재접속 식별자로 사용하고 PIE에서는 이름으로 대체
FString ACMPlayGameMode::BuildReconnectKey(
    const ACMPlayerState* PlayerState) const
{
    if (!PlayerState)
    {
        return FString();
    }

    const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId();
    const TSharedPtr<const FUniqueNetId> NativeId = UniqueId.GetUniqueNetId();
    return NativeId.IsValid()
        ? NativeId->ToString()
        : PlayerState->GetPlayerName();
}

// 60초 만료 시 저장된 Q/W/E/R 담당 파츠를 탈착하고 남은 인원으로 재배정
void ACMPlayGameMode::ExpireDisconnectedPlayer(FString ReconnectKey)
{
    FCMDisconnectedPlayerRecord Record;
    if (!DisconnectedPlayers.RemoveAndCopyValue(ReconnectKey, Record))
    {
        return;
    }

    if (const ACMPlayGameState* PlayState = CachedPlayGameState)
    {
        if (ACMChimera* SharedChimera = PlayState->SharedChimera)
        {
            SharedChimera->ClearPressedControlParts();
            for (const FCMPartSlotAddress& SlotAddress : Record.ControlSlots)
            {
                SharedChimera->DetachPartFromSlot(SlotAddress);
            }
        }
    }

    ExpiredReconnectKeys.Add(ReconnectKey);
    RebalanceControlAssignments();
    UE_LOG(LogChimeraStageLoad, Error,
        TEXT("재접속 유예가 만료되어 담당 파츠를 탈착하고 플레이어를 제외했습니다. Key=%s Grace=%.1fs"),
        *ReconnectKey, ReconnectGracePeriod);
}

// Starting 이후 기록 없는 합류자와 유예 만료 재접속자는 현재 스테이지 관전
bool ACMPlayGameMode::ShouldSpectateCurrentStage(
    const ACMPlayerState* PlayerState) const
{
    const ACMPlayGameState* PlayState = CachedPlayGameState;
    if (!PlayerState || !PlayState)
    {
        return false;
    }

    const ECMPlayPhase Phase = PlayState->GetPlayPhase();
    const bool bStageAlreadyStarted =
        Phase != ECMPlayPhase::Loading
        && Phase != ECMPlayPhase::WaitingForPlayers;
    if (!bStageAlreadyStarted)
    {
        return false;
    }

    const FString ReconnectKey = BuildReconnectKey(PlayerState);
    return !DisconnectedPlayers.Contains(ReconnectKey)
        || ExpiredReconnectKeys.Contains(ReconnectKey);
}

// 서버의 플레이 Phase 변경 요청을 PlayGameState에 전달
void ACMPlayGameMode::SetPlayPhase(
    ECMPlayPhase NewPhase,
    float Duration
)
{
    if (ACMPlayGameState* PlayState = CachedPlayGameState)
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
    if (ACMPlayGameState* PlayState = CachedPlayGameState)
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
    TryStartStageWhenReady();
    return true;
}

// 준비 대기 상태를 검증하고 StageDirector에 시작 연출 요청
bool ACMPlayGameMode::StartStage()
{
    ACMPlayGameState* PlayState = CachedPlayGameState;
    if (!HasAuthority()
        || !PlayState
        || !IsValid(StageDirector)
        || PlayState->GetPlayPhase() != ECMPlayPhase::WaitingForPlayers)
    {
        return false;
    }

    bStageLoadReady = false;

    GetWorldTimerManager().ClearTimer(StartingPresentationTimeoutHandle);
    if (StartingPresentationTimeout > 0.0f)
    {
        GetWorldTimerManager().SetTimer(
            StartingPresentationTimeoutHandle,
            this,
            &ThisClass::HandleStartingPresentationTimeout,
            StartingPresentationTimeout,
            false);
    }
    // 기본 Director는 연출 상태 변경 중 즉시 완료할 수 있으므로
    // 재진입에 필요한 상태와 타이머를 먼저 준비한 뒤 이벤트를 게시한다.
    SetPlayPhase(ECMPlayPhase::Starting, StartingPresentationTimeout);
    PlayState->SetStagePresentationState(ECMStagePresentationState::Starting);
    return true;
}

// 시작 연출 완료 누락을 자동 진행으로 숨기지 않고 개발 오류로 보고
void ACMPlayGameMode::HandleStartingPresentationTimeout()
{
    const ACMPlayGameState* PlayState = CachedPlayGameState;
    if (!HasAuthority() || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Starting)
    {
        return;
    }

    UE_LOG(LogChimeraStageLoad, Error,
        TEXT("시작 연출이 제한 시간 안에 완료되지 않았습니다. Stage=%d Director=%s Timeout=%.1fs"),
        PlayState->GetCurrentStageIndex(),
        *GetNameSafe(StageDirector),
        StartingPresentationTimeout);
}

// 로드 완료와 Director 등록 순서에 관계없이 두 조건이 모였을 때 한 번만 시작
void ACMPlayGameMode::TryStartStageWhenReady()
{
    const ACMPlayGameState* PlayState = CachedPlayGameState;
    if (!HasAuthority() || !bStageLoadReady || !IsValid(StageDirector)
        || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::WaitingForPlayers
        || PlayState->GetLobbyPlayerCount() <= 0)
    {
        return;
    }

    StartStage();
}

// 공용 키메라의 전체 사망 사건을 상위 게임 흐름에 연결
void ACMPlayGameMode::BindSharedChimeraEvents()
{
    const ACMPlayGameState* PlayState = CachedPlayGameState;
    ACMChimera* SharedChimera = PlayState ? PlayState->SharedChimera : nullptr;
    if (HasAuthority() && IsValid(SharedChimera))
    {
        SharedChimera->OnAllSegmentsDead.AddUniqueDynamic(
            this, &ThisClass::HandleAllSegmentsDead);
    }
}

// 실제 플레이 중 모든 활성 Segment가 사망하면 전체 Defeat 확정
void ACMPlayGameMode::HandleAllSegmentsDead()
{
    const ACMPlayGameState* PlayState = CachedPlayGameState;
    if (HasAuthority() && PlayState
        && PlayState->GetPlayPhase() == ECMPlayPhase::Playing)
    {
        ConfirmGameDefeat();
    }
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
        const ACMPlayGameState* PlayState = CachedPlayGameState;
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
    ACMPlayGameState* PlayState = CachedPlayGameState;
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
    ACMPlayGameState* PlayState = CachedPlayGameState;
    if (!HasAuthority() || !PlayState || !QueuedRequest.ScheduleId.IsValid())
    {
        return false;
    }

    FCMStageLoadRequest Request;
    Request.RequestId = FGuid::NewGuid();
    Request.ScheduleId = QueuedRequest.ScheduleId;
    Request.bBlocking = true;

    ActiveStageLoadRequestId = Request.RequestId;
    // 서버 로컬 로드가 요청 게시 중 즉시 완료될 수 있으므로
    // 완료 보고를 받을 배리어를 먼저 연 뒤 GameState에 게시한다.
    StageLoadBarrier->BeginBarrier(
        Request.RequestId,
        true,
        StageLoadTimeout);
    PlayState->BeginStageLoadRequest(
        Request,
        StageLoadBarrier->GetTargetPlayerCount(),
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

    const ACMPlayGameState* PlayState = CachedPlayGameState;
    const FPrimaryAssetId ScheduleId = PlayState
        ? PlayState->GetStageLoadRequest().ScheduleId
        : FPrimaryAssetId();
    UE_LOG(LogChimeraStageLoad, Error,
        TEXT("스테이지 로드 요청이 실패하여 게임 흐름을 중단합니다. Stage=%d Schedule=%s Request=%s State=%d Reason=%d"),
        PlayState ? PlayState->GetCurrentStageIndex() : INDEX_NONE,
        *ScheduleId.ToString(),
        *RequestId.ToString(),
        static_cast<int32>(State),
        static_cast<int32>(Reason));
    FailActiveStageLoad(State, Reason);
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
    const UCMStageRouteSubsystem* StageRoute = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMStageRouteSubsystem>()
        : nullptr;
    if (StageRoute && StageRoute->IsStageRouteActive()
        && StageRoute->GetCurrentStageIndex() == StageIndex)
    {
        const FCMStageRouteEntry* Stage = StageRoute->GetCurrentStage();
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
    if (ACMPlayGameState* PlayState = CachedPlayGameState)
    {
        PlayState->SetStageLoadStatus(
            ECMStageLoadState::Ready,
            ECMStageLoadFailureReason::None,
            StageLoadBarrier->GetReadyPlayerCount(),
            StageLoadBarrier->GetTargetPlayerCount());
    }

    ActiveStageLoadRequestId.Invalidate();

    StartNextQueuedStageLoadRequest();

    if (ActiveStageLoadRequestId.IsValid())
    {
        bStageLoadReady = false;
        SetPlayPhase(ECMPlayPhase::Loading);
        return;
    }

    bStageLoadReady = true;
    SetPlayPhase(ECMPlayPhase::WaitingForPlayers);
    TryStartStageWhenReady();
}

// 기술적 실패 상태를 게임 플레이 Failed Phase와 분리해 요청 종료
void ACMPlayGameMode::FailActiveStageLoad(
    ECMStageLoadState State,
    ECMStageLoadFailureReason Reason)
{
    const int32 ReadyPlayerCount = StageLoadBarrier->GetReadyPlayerCount();
    const int32 TargetPlayerCount = StageLoadBarrier->GetTargetPlayerCount();
    StageLoadBarrier->CancelBarrier();
    if (ACMPlayGameState* PlayState = CachedPlayGameState)
    {
        // 배리어가 이미 실패 Snapshot을 기록했다면 취소 후 0명 값으로 덮어쓰지 않는다.
        if (PlayState->GetStageLoadState() != State
            || PlayState->GetStageLoadFailureReason() != Reason)
        {
            PlayState->SetStageLoadStatus(
                State,
                Reason,
                ReadyPlayerCount,
                TargetPlayerCount);
        }
    }

    ActiveStageLoadRequestId.Invalidate();
    QueuedStageLoadRequests.Reset();
    bStageLoadReady = false;
}

// 등록된 Director의 보고만 받아 Starting에서 Playing으로 전환
bool ACMPlayGameMode::HandleStartingPresentationFinished(
    ACMStageDirector* ReportingDirector
)
{
    ACMPlayGameState* PlayState = CachedPlayGameState;
    if (!IsCurrentStageDirector(ReportingDirector)
        || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Starting)
    {
        return false;
    }

    GetWorldTimerManager().ClearTimer(StartingPresentationTimeoutHandle);
    SetPlayPhase(ECMPlayPhase::Playing);
    PlayState->SetStagePresentationState(ECMStagePresentationState::None);
    return true;
}

// 등록된 Director의 결과 연출 보고만 받아 다음 스테이지 인덱스 진행
bool ACMPlayGameMode::HandleResultPresentationFinished(
    ACMStageDirector* ReportingDirector)
{
    const ACMPlayGameState* PlayState = CachedPlayGameState;
    if (!IsCurrentStageDirector(ReportingDirector)
        || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Completed)
    {
        return false;
    }

    if (ACMPlayGameState* MutablePlayState = CachedPlayGameState)
    {
        MutablePlayState->SetStagePresentationState(ECMStagePresentationState::None);
    }

    if (TryScheduleStageLoopRestart())
    {
        return true;
    }

    const bool bLastStage =
        PlayState->GetCurrentStageIndex() + 1
        >= PlayState->GetTotalStageCount();
    if (bLastStage)
    {
        SetPlayPhase(ECMPlayPhase::Victory);
        return true;
    }

    AdvanceToNextStage();
    return true;
}

// 별도 레벨 전환 구현이 없으면 같은 월드에서 즉시 Entry 로드 단계로 연결
void ACMPlayGameMode::BeginStageTransition_Implementation(int32 NextStageIndex)
{
    UCMStageRouteSubsystem* StageRoute = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMStageRouteSubsystem>()
        : nullptr;
    if (!StageRoute || !StageRoute->IsStageRouteActive())
    {
        FinishStageTransition();
        return;
    }

    const FCMStageRouteEntry* Stage = StageRoute->GetPendingStage();
    if (!Stage || StageRoute->GetPendingStageIndex() != NextStageIndex)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("StageRoute stage transition state is invalid. Requested=%d Current=%d"),
            NextStageIndex, StageRoute->GetPendingStageIndex());
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
        StageRoute->CancelPendingStage();
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
    ACMPlayGameState* PlayState = CachedPlayGameState;
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

    UCMStageRouteSubsystem* StageRoute = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMStageRouteSubsystem>()
        : nullptr;
    if (StageRoute && StageRoute->IsStageRouteActive())
    {
        if (!StageRoute->PrepareNextStage())
        {
            UE_LOG(LogChimeraStageLoad, Error,
                TEXT("StageRoute could not advance from stage %d."),
                PlayState->GetCurrentStageIndex());
            return;
        }
        NextStageIndex = StageRoute->GetPendingStageIndex();
    }

    PendingStageTransitionIndex = NextStageIndex;
    StageDirector = nullptr;
    bStageLoadReady = false;
    InitializeStageProgress(NextStageIndex, PlayState->GetTotalStageCount());
    SetPlayPhase(ECMPlayPhase::Loading);
    BeginStageTransition(NextStageIndex);
}

// 등록된 Director의 클리어 보고를 결과 연출을 거치는 Completed로 확정
bool ACMPlayGameMode::HandleStageCompleted(
    ACMStageDirector* ReportingDirector
)
{
    ACMPlayGameState* PlayState = CachedPlayGameState;
    if (!IsCurrentStageDirector(ReportingDirector)
        || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Playing)
    {
        return false;
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
    return true;
}

// 등록된 Director의 실패 보고를 현재 스테이지 Failed로 확정
bool ACMPlayGameMode::HandleStageFailed(
    ACMStageDirector* ReportingDirector
)
{
    const ACMPlayGameState* PlayState = CachedPlayGameState;
    if (!IsCurrentStageDirector(ReportingDirector)
        || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Playing)
    {
        return false;
    }

    SetPlayPhase(ECMPlayPhase::Failed);
    TryScheduleStageLoopRestart();
    return true;
}

// 현재 스테이지 실패와 구분되는 게임 전체 패배 확정
void ACMPlayGameMode::ConfirmGameDefeat()
{
    if (HasAuthority())
    {
        SetPlayPhase(ECMPlayPhase::Defeat);
        TryScheduleStageLoopRestart();
    }
}

// RouteDefinition이 반복 모드일 때만 현재 맵 재시작 예약
bool ACMPlayGameMode::TryScheduleStageLoopRestart()
{
    const UCMStageRouteSubsystem* StageRoute = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMStageRouteSubsystem>()
        : nullptr;
    if (!HasAuthority() || bStageLoopRestartScheduled
        || !StageRoute || !StageRoute->ShouldLoopCurrentStage())
    {
        return false;
    }

    bStageLoopRestartScheduled = true;
    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("반복 Route 결과가 확정되어 현재 스테이지를 다시 시작합니다. Delay=%.1f"),
        LoopingStageRestartDelay);

    if (LoopingStageRestartDelay <= 0.0f)
    {
        RestartLoopingStage();
    }
    else
    {
        GetWorldTimerManager().SetTimer(
            StageLoopRestartTimerHandle,
            this,
            &ThisClass::RestartLoopingStage,
            LoopingStageRestartDelay,
            false);
    }
    return true;
}

// 같은 맵 ServerTravel로 월드 전체를 초기화하고 PlayerStart부터 재개
void ACMPlayGameMode::RestartLoopingStage()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const FString CurrentMapPackageName = UWorld::RemovePIEPrefix(
        GetWorld()->GetOutermost()->GetName());
    if (CurrentMapPackageName.IsEmpty()
        || !GetWorld()->ServerTravel(CurrentMapPackageName, false))
    {
        bStageLoopRestartScheduled = false;
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("반복 Route 스테이지 재시작에 실패했습니다. Map=%s"),
            *CurrentMapPackageName);
    }
}

// 최종 결과 상태에서 엔딩 표시 상태로 전환
void ACMPlayGameMode::StartEnding()
{
    const ACMPlayGameState* PlayState = CachedPlayGameState;
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
