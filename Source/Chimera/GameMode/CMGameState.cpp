#include "GameMode/CMGameState.h"

#include "Player/CMControlTypes.h"
#include "GameFramework/PlayerState.h"
#include "ListenServerNetworkSettings.h"
#include "Net/UnrealNetwork.h"

// 참가 PlayerState 추가 후 로비 명단 변경 알림
void ACMGameState::AddPlayerState(APlayerState* PlayerState)
{
    Super::AddPlayerState(PlayerState);
    NotifyLobbyRosterChanged();
}

// 참가 PlayerState 제거 후 로비 명단 변경 알림
void ACMGameState::RemovePlayerState(APlayerState* PlayerState)
{
    Super::RemovePlayerState(PlayerState);
    NotifyLobbyRosterChanged();
}

// 공용 키메라 참조를 네트워크 복제 대상으로 등록
void ACMGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACMGameState, SharedChimera);
    DOREPLIFETIME(ACMGameState, bSoloTestMode);
    DOREPLIFETIME(ACMGameState, WorldPresentationState);
}

void ACMGameState::SetSoloTestMode(bool bEnabled)
{
    if (!HasAuthority() || bSoloTestMode == bEnabled)
    {
        return;
    }

    bSoloTestMode = bEnabled;
    ForceNetUpdate();
}

void ACMGameState::SetWorldPresentationState(
    ECMWorldPresentationState NewState)
{
    if (!HasAuthority() || WorldPresentationState == NewState)
    {
        return;
    }

    WorldPresentationState = NewState;
    ForceNetUpdate();
}

// 서버에서 공용 키메라 참조를 변경하고 즉시 복제 요청
void ACMGameState::SetSharedChimera(
    ACMChimera* NewSharedChimera
)
{
    if (!HasAuthority() || SharedChimera == NewSharedChimera)
    {
        return;
    }

    SharedChimera = NewSharedChimera;
    OnRep_SharedChimera();
    ForceNetUpdate();
}

// 로비 참가자 명단 변경 이벤트 전달
void ACMGameState::NotifyLobbyRosterChanged()
{
    OnLobbyRosterChanged.Broadcast();
}

// 관전자를 제외한 현재 참가 플레이어 수 계산
int32 ACMGameState::GetLobbyPlayerCount() const
{
    int32 PlayerCount = 0;
    for (const APlayerState* PlayerState : PlayerArray)
    {
        if (IsValid(PlayerState) && !PlayerState->IsOnlyASpectator())
        {
            ++PlayerCount;
        }
    }

    return PlayerCount;
}

// ListenServer 설정에서 로비 최대 인원 조회
int32 ACMGameState::GetLobbyMaxPlayers() const
{
    const UListenServerNetworkSettings* NetworkSettings =
        GetDefault<UListenServerNetworkSettings>();
    return NetworkSettings
        ? NetworkSettings->DefaultMaxPlayers
        : CMControl::MaxPlayers;
}

// 관전자를 제외한 현재 참가 플레이어 이름 목록 생성
TArray<FString> ACMGameState::GetLobbyPlayerNames() const
{
    TArray<FString> PlayerNames;
    PlayerNames.Reserve(PlayerArray.Num());

    for (const APlayerState* PlayerState : PlayerArray)
    {
        if (IsValid(PlayerState) && !PlayerState->IsOnlyASpectator())
        {
            PlayerNames.Add(PlayerState->GetPlayerName());
        }
    }

    return PlayerNames;
}

// 공용 키메라 참조 변경을 구독 중인 시스템에 전달
void ACMGameState::OnRep_SharedChimera()
{
    OnSharedChimeraChanged.Broadcast();
}
