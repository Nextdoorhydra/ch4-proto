#include "ChimeraGameState.h"

#include "Chimera/Player/ChimeraControlTypes.h"
#include "GameFramework/PlayerState.h"
#include "ListenServerNetworkSettings.h"
#include "Net/UnrealNetwork.h"

void AChimeraGameState::AddPlayerState(APlayerState* PlayerState)
{
    Super::AddPlayerState(PlayerState);
    NotifyLobbyRosterChanged();
}

void AChimeraGameState::RemovePlayerState(APlayerState* PlayerState)
{
    Super::RemovePlayerState(PlayerState);
    NotifyLobbyRosterChanged();
}

void AChimeraGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AChimeraGameState, SharedChimera);
}

void AChimeraGameState::SetSharedChimera(
    AChimeraPrototypePawn* NewSharedChimera
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

void AChimeraGameState::NotifyLobbyRosterChanged()
{
    OnLobbyRosterChanged.Broadcast();
}

int32 AChimeraGameState::GetLobbyPlayerCount() const
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

int32 AChimeraGameState::GetLobbyMaxPlayers() const
{
    const UListenServerNetworkSettings* NetworkSettings =
        GetDefault<UListenServerNetworkSettings>();
    return NetworkSettings
        ? NetworkSettings->DefaultMaxPlayers
        : ChimeraControl::MaxPlayers;
}

TArray<FString> AChimeraGameState::GetLobbyPlayerNames() const
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

void AChimeraGameState::OnRep_SharedChimera()
{
    OnSharedChimeraChanged.Broadcast();
}
