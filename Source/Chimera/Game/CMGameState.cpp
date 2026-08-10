#include "CMGameState.h"

#include "Player/CMControlTypes.h"
#include "GameFramework/PlayerState.h"
#include "ListenServerNetworkSettings.h"
#include "Net/UnrealNetwork.h"

void ACMGameState::AddPlayerState(APlayerState* PlayerState)
{
    Super::AddPlayerState(PlayerState);
    NotifyLobbyRosterChanged();
}

void ACMGameState::RemovePlayerState(APlayerState* PlayerState)
{
    Super::RemovePlayerState(PlayerState);
    NotifyLobbyRosterChanged();
}

void ACMGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACMGameState, SharedChimera);
}

void ACMGameState::SetSharedChimera(
    ACMPawn* NewSharedChimera
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

void ACMGameState::NotifyLobbyRosterChanged()
{
    OnLobbyRosterChanged.Broadcast();
}

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

int32 ACMGameState::GetLobbyMaxPlayers() const
{
    const UListenServerNetworkSettings* NetworkSettings =
        GetDefault<UListenServerNetworkSettings>();
    return NetworkSettings
        ? NetworkSettings->DefaultMaxPlayers
        : CMControl::MaxPlayers;
}

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

void ACMGameState::OnRep_SharedChimera()
{
    OnSharedChimeraChanged.Broadcast();
}
