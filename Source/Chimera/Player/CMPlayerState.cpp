#include "CMPlayerState.h"

#include "GameMode/CMGameState.h"
#include "Net/UnrealNetwork.h"

ACMPlayerState::ACMPlayerState()
{
    SetReplicates(true);
}

void ACMPlayerState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACMPlayerState, AssignedControlParts);
    DOREPLIFETIME(ACMPlayerState, PlayerColorIndex);
}

void ACMPlayerState::SetPlayerName(const FString& NewPlayerName)
{
    Super::SetPlayerName(NewPlayerName);

    if (ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr)
    {
        GameState->NotifyLobbyRosterChanged();
    }
}

void ACMPlayerState::OnRep_PlayerName()
{
    Super::OnRep_PlayerName();

    if (ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr)
    {
        GameState->NotifyLobbyRosterChanged();
    }
}

ECMControlPart ACMPlayerState::GetControlPartForSlot(
    int32 SlotIndex
) const
{
    return AssignedControlParts.IsValidIndex(SlotIndex)
        ? AssignedControlParts[SlotIndex]
        : ECMControlPart::None;
}

void ACMPlayerState::SetAssignedControlParts(
    const TArray<ECMControlPart>& NewAssignments
)
{
    if (!HasAuthority() || AssignedControlParts == NewAssignments)
    {
        return;
    }

    AssignedControlParts = NewAssignments;
    OnRep_AssignedControlParts();
    ForceNetUpdate();
}

int32 ACMPlayerState::GetAssignedControlCount() const
{
    return AssignedControlParts.Num();
}

void ACMPlayerState::SetPlayerColorIndex(int32 NewPlayerColorIndex)
{
    if (!HasAuthority() || PlayerColorIndex == NewPlayerColorIndex)
    {
        return;
    }

    PlayerColorIndex = NewPlayerColorIndex;
    OnRep_PlayerColorIndex();
    ForceNetUpdate();
}

FLinearColor ACMPlayerState::GetPlayerColor() const
{
    static const FLinearColor PlayerColors[] =
    {
        FLinearColor(0.906f, 0.224f, 0.208f),
        FLinearColor(0.118f, 0.533f, 0.898f),
        FLinearColor(0.263f, 0.627f, 0.278f),
        FLinearColor(0.557f, 0.141f, 0.667f),
        FLinearColor(0.984f, 0.549f, 0.0f),
        FLinearColor(0.0f, 0.537f, 0.482f),
        FLinearColor(0.847f, 0.106f, 0.376f),
        FLinearColor(0.427f, 0.298f, 0.255f)
    };

    return PlayerColorIndex >= 0
        && PlayerColorIndex < UE_ARRAY_COUNT(PlayerColors)
        ? PlayerColors[PlayerColorIndex]
        : FLinearColor::White;
}

int32 ACMPlayerState::GetPlayerColorIndex() const
{
    return PlayerColorIndex;
}

void ACMPlayerState::OnRep_AssignedControlParts()
{
    OnControlAssignmentsChanged.Broadcast();
}

void ACMPlayerState::OnRep_PlayerColorIndex()
{
    OnPlayerColorChanged.Broadcast();

    if (ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr)
    {
        GameState->NotifyLobbyRosterChanged();
    }
}
