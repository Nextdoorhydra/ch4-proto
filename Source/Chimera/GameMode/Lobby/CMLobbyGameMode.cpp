#include "GameMode/Lobby/CMLobbyGameMode.h"

#include "GameMode/Lobby/CMLobbyGameState.h"
#include "GameMode/StageRoute/CMStageRouteDefinition.h"
#include "GameMode/StageRoute/CMStageRouteSubsystem.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "Player/CMPlayerState.h"

// 로비 전용 GameState 설정 및 개별 Pawn 생성을 비활성화
ACMLobbyGameMode::ACMLobbyGameMode()
{
    GameStateClass = ACMLobbyGameState::StaticClass();
    DefaultPawnClass = nullptr;
}

// 플레이어 접속 완료 후 로비 참가·Ready 집계 갱신
void ACMLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    RefreshLobbySummary();
}

// 로비 월드에서 반복 사용할 LobbyGameState 참조 초기화
void ACMLobbyGameMode::BeginPlay()
{
    Super::BeginPlay();
    CachedLobbyGameState = GetGameState<ACMLobbyGameState>();
    if (!CachedLobbyGameState)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("LobbyGameState를 캐시하지 못해 로비 흐름을 초기화할 수 없습니다."));
        return;
    }

    RefreshLobbySummary();
}

// 최초 접속과 Seamless Travel 복귀 모두 Ready와 참여 상태를 로비 기준으로 초기화
void ACMLobbyGameMode::GenericPlayerInitialization(AController* C)
{
    Super::GenericPlayerInitialization(C);

    if (ACMPlayerState* PlayerState = C
        ? C->GetPlayerState<ACMPlayerState>()
        : nullptr)
    {
        PlayerState->SetReady(false);
        PlayerState->SetParticipationState(
            ECMPlayerParticipationState::Lobby);
    }
    RefreshLobbySummary();
}

// 플레이어 이탈 처리 후 로비 참가·Ready 집계 갱신
void ACMLobbyGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    RefreshLobbySummary();
}

// 서버의 로비 집계 결과를 LobbyGameState에 반영
void ACMLobbyGameMode::RefreshLobbySummary()
{
    if (!HasAuthority())
    {
        return;
    }

    ACMLobbyGameState* LobbyState = CachedLobbyGameState;
    if (!LobbyState)
    {
        return;
    }

    int32 PlayerCount = 0;
    int32 ReadyCount = 0;
    for (APlayerState* PlayerState : LobbyState->PlayerArray)
    {
        const ACMPlayerState* CMPlayerState =
            Cast<ACMPlayerState>(PlayerState);
        if (!CMPlayerState || CMPlayerState->IsOnlyASpectator())
        {
            continue;
        }

        ++PlayerCount;
        ReadyCount += CMPlayerState->IsReady() ? 1 : 0;
    }

    const bool bCanStart = PlayerCount >= MinimumPlayersToStart
        && ReadyCount == PlayerCount;
    LobbyState->SetLobbySummary(ReadyCount, bCanStart);
    LobbyState->NotifyLobbyRosterChanged();
}

// Waiting 로비의 실제 참가자만 Ready 상태를 변경하도록 허용
bool ACMLobbyGameMode::TrySetPlayerReady(
    APlayerController* RequestingPlayer,
    bool bReady)
{
    ACMLobbyGameState* LobbyState = CachedLobbyGameState;
    ACMPlayerState* PlayerState = RequestingPlayer
        ? RequestingPlayer->GetPlayerState<ACMPlayerState>()
        : nullptr;
    if (!HasAuthority() || bStageRouteStartInProgress || !LobbyState
        || LobbyState->GetLobbyPhase() != ECMLobbyPhase::Waiting
        || !PlayerState || PlayerState->IsOnlyASpectator())
    {
        return false;
    }

    PlayerState->SetReady(bReady);
    RefreshLobbySummary();
    return true;
}

// 준비 완료 로비에서 유효한 캠페인을 등록하고 첫 스테이지 맵으로 이동
bool ACMLobbyGameMode::TryStartStageRoute(APlayerController* RequestingPlayer)
{
    return TryStartRouteDefinition(RequestingPlayer, StageRouteDefinition);
}

// 준비 완료 로비에서 TestRoute를 선택해 멀티플레이 테스트 시작
bool ACMLobbyGameMode::TryStartTestStageRoute(
    APlayerController* RequestingPlayer)
{
    return TryStartRouteDefinition(RequestingPlayer, TestStageRouteDefinition);
}

// 준비 완료 로비에서 전달받은 Route를 등록하고 첫 스테이지 맵으로 이동
bool ACMLobbyGameMode::TryStartRouteDefinition(
    APlayerController* RequestingPlayer,
    UCMStageRouteDefinition* RouteDefinition)
{
    ACMLobbyGameState* LobbyState = CachedLobbyGameState;
    UCMStageRouteSubsystem* StageRoute = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMStageRouteSubsystem>() : nullptr;
    if (!HasAuthority() || bStageRouteStartInProgress || !IsValid(RequestingPlayer)
        || !LobbyState || LobbyState->GetLobbyPhase() != ECMLobbyPhase::Waiting
        || !LobbyState->CanStartGame() || !StageRoute || !RouteDefinition)
    {
        return false;
    }

    if (!StageRoute->StartStageRoute(RouteDefinition))
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Lobby could not start stage route. StageRoute=%s Requester=%s"),
            *GetPathNameSafe(RouteDefinition), *GetNameSafe(RequestingPlayer));
        return false;
    }

    const FCMStageRouteEntry* FirstStage = StageRoute->GetCurrentStage();
    const FString MapPackageName = FirstStage
        ? FirstStage->StageMap.ToSoftObjectPath().GetLongPackageName()
        : FString();
    if (MapPackageName.IsEmpty())
    {
        StageRoute->ResetStageRoute();
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("StageRoute first stage map is invalid. StageRoute=%s"),
            *GetPathNameSafe(RouteDefinition));
        return false;
    }

    bStageRouteStartInProgress = true;
    LobbyState->SetLobbyPhase(ECMLobbyPhase::Loading);
    LobbyState->MulticastResetStageRouteLoading();
    if (!GetWorld()->ServerTravel(MapPackageName, false))
    {
        bStageRouteStartInProgress = false;
        LobbyState->SetLobbyPhase(ECMLobbyPhase::Waiting);
        StageRoute->ResetStageRoute();
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Lobby ServerTravel failed. StageRoute=%s Map=%s"),
            *GetPathNameSafe(RouteDefinition), *MapPackageName);
        return false;
    }

    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("Lobby started stage route travel. StageRoute=%s Map=%s Requester=%s"),
        *GetPathNameSafe(RouteDefinition), *MapPackageName,
        *GetNameSafe(RequestingPlayer));
    return true;
}
