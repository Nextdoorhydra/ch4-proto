#include "GameMode/Lobby/CMLobbyGameState.h"

#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"
#include "Net/UnrealNetwork.h"

// 로비 Phase·Ready 인원·시작 가능 여부를 복제 대상으로 등록
void ACMLobbyGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACMLobbyGameState, LobbyPhase);
    DOREPLIFETIME(ACMLobbyGameState, ReadyPlayerCount);
    DOREPLIFETIME(ACMLobbyGameState, bCanStartGame);
}

// 서버에서 로비 Phase를 변경하고 클라이언트 갱신 요청
void ACMLobbyGameState::SetLobbyPhase(ECMLobbyPhase NewPhase)
{
    if (!HasAuthority() || LobbyPhase == NewPhase)
    {
        return;
    }

    LobbyPhase = NewPhase;
    OnRep_LobbyState();
    ForceNetUpdate();
}

// 서버에서 Ready 집계와 게임 시작 가능 여부 갱신
void ACMLobbyGameState::SetLobbySummary(
    int32 NewReadyCount,
    bool bNewCanStart
)
{
    if (!HasAuthority())
    {
        return;
    }

    NewReadyCount = FMath::Max(0, NewReadyCount);
    if (ReadyPlayerCount == NewReadyCount
        && bCanStartGame == bNewCanStart)
    {
        return;
    }

    ReadyPlayerCount = NewReadyCount;
    bCanStartGame = bNewCanStart;
    OnRep_LobbyState();
    ForceNetUpdate();
}

// 로비 복제 상태 변경을 UI와 구독 시스템에 전달
void ACMLobbyGameState::OnRep_LobbyState()
{
    OnLobbyStateChanged.Broadcast();
}

// 새 Route가 이전 Route의 로드 핸들과 준비 상태를 재사용하지 않도록 초기화
void ACMLobbyGameState::MulticastResetStageRouteLoading_Implementation()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UCMStageLoadCoordinatorSubsystem* Coordinator =
            GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>())
        {
            Coordinator->ResetStageRouteLoading();
        }
    }
}
