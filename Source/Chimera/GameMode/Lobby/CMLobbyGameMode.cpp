#include "GameMode/Lobby/CMLobbyGameMode.h"

#include "GameMode/Lobby/CMLobbyGameState.h"
#include "GameMode/Campaign/CMCampaignDefinition.h"
#include "GameMode/Campaign/CMCampaignSubsystem.h"
#include "AsyncLoad/CMStageLoadLog.h"

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

    ACMLobbyGameState* LobbyState =
        GetGameState<ACMLobbyGameState>();
    if (!LobbyState)
    {
        return;
    }

    // Ready state will be sourced from ACMPlayerState in the next layer.
    LobbyState->SetLobbySummary(0, false);
    LobbyState->NotifyLobbyRosterChanged();
}

// 준비 완료 로비에서 유효한 캠페인을 등록하고 첫 스테이지 맵으로 이동
bool ACMLobbyGameMode::TryStartCampaign(APlayerController* RequestingPlayer)
{
    ACMLobbyGameState* LobbyState = GetGameState<ACMLobbyGameState>();
    UCMCampaignSubsystem* Campaign = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMCampaignSubsystem>() : nullptr;
    if (!HasAuthority() || bCampaignStartInProgress || !IsValid(RequestingPlayer)
        || !LobbyState || LobbyState->GetLobbyPhase() != ECMLobbyPhase::Waiting
        || !LobbyState->CanStartGame() || !Campaign || !CampaignDefinition)
    {
        return false;
    }

    if (!Campaign->StartCampaign(CampaignDefinition))
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Lobby could not start campaign. Campaign=%s Requester=%s"),
            *GetPathNameSafe(CampaignDefinition), *GetNameSafe(RequestingPlayer));
        return false;
    }

    const FCMCampaignStageDefinition* FirstStage = Campaign->GetCurrentStage();
    const FString MapPackageName = FirstStage
        ? FirstStage->StageMap.ToSoftObjectPath().GetLongPackageName()
        : FString();
    if (MapPackageName.IsEmpty())
    {
        Campaign->ResetCampaign();
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Campaign first stage map is invalid. Campaign=%s"),
            *GetPathNameSafe(CampaignDefinition));
        return false;
    }

    bCampaignStartInProgress = true;
    LobbyState->SetLobbyPhase(ECMLobbyPhase::Loading);
    if (!GetWorld()->ServerTravel(MapPackageName, false))
    {
        bCampaignStartInProgress = false;
        LobbyState->SetLobbyPhase(ECMLobbyPhase::Waiting);
        Campaign->ResetCampaign();
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Lobby ServerTravel failed. Campaign=%s Map=%s"),
            *GetPathNameSafe(CampaignDefinition), *MapPackageName);
        return false;
    }

    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("Lobby started campaign travel. Campaign=%s Map=%s Requester=%s"),
        *GetPathNameSafe(CampaignDefinition), *MapPackageName,
        *GetNameSafe(RequestingPlayer));
    return true;
}
