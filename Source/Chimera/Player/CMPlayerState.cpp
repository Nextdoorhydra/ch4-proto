#include "CMPlayerState.h"

#include "GameMode/CMGameState.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Vision/CMVisionManagerSubsystem.h"

ACMPlayerState::ACMPlayerState()
{
    SetReplicates(true);
}

void ACMPlayerState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACMPlayerState, PlayerColorIndex);
    DOREPLIFETIME(ACMPlayerState, bReady);
    DOREPLIFETIME(ACMPlayerState, PlayerSlotId);
    DOREPLIFETIME(ACMPlayerState, ParticipationState);
    DOREPLIFETIME(ACMPlayerState, bVisionSystemEnabled);
}

// PlayerState가 교체되는 Travel·재접속 경계에서 플레이어 고유 상태 보존
void ACMPlayerState::CopyProperties(APlayerState* PlayerState)
{
    Super::CopyProperties(PlayerState);

    if (ACMPlayerState* NewPlayerState = Cast<ACMPlayerState>(PlayerState))
    {
        NewPlayerState->PlayerColorIndex = PlayerColorIndex;
        NewPlayerState->bReady = bReady;
        NewPlayerState->PlayerSlotId = PlayerSlotId;
        NewPlayerState->ParticipationState = ParticipationState;
        NewPlayerState->bVisionSystemEnabled = bVisionSystemEnabled;
    }
}

void ACMPlayerState::ClientInitialize(AController* Controller)
{
    Super::ClientInitialize(Controller);
    OnRep_VisionSystemEnabled();

    if (ACMGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ACMGameState>() : nullptr)
    {
        GameState->NotifyLobbyRosterChanged();
    }
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

// 서버에서 로비 준비 상태를 변경하고 모든 참가자에게 복제
void ACMPlayerState::SetReady(bool bNewReady)
{
    if (!HasAuthority() || bReady == bNewReady)
    {
        return;
    }

    bReady = bNewReady;
    OnRep_Ready();
    ForceNetUpdate();
}

// 서버에서 비어 있는 고정 플레이어 자리를 배정하고 모든 참가자에게 복제
void ACMPlayerState::SetPlayerSlotId(int32 NewPlayerSlotId)
{
    if (!HasAuthority() || PlayerSlotId == NewPlayerSlotId)
    {
        return;
    }

    PlayerSlotId = NewPlayerSlotId;
    OnRep_PlayerSlotId();
    ForceNetUpdate();
}

// 서버에서 플레이어의 로비·활성·관전·패배 상태를 변경
void ACMPlayerState::SetParticipationState(
    ECMPlayerParticipationState NewState)
{
    if (!HasAuthority() || ParticipationState == NewState)
    {
        return;
    }

    ParticipationState = NewState;
    OnRep_ParticipationState();
    ForceNetUpdate();
}

void ACMPlayerState::SetVisionSystemEnabled(bool bEnabled)
{
    if (!HasAuthority() || bVisionSystemEnabled == bEnabled)
    {
        return;
    }

    bVisionSystemEnabled = bEnabled;
    OnRep_VisionSystemEnabled();
    ForceNetUpdate();
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

// Ready UI와 로비 명단 구독자에게 상태 변경 전달
void ACMPlayerState::OnRep_Ready()
{
    OnReadyChanged.Broadcast();

    if (ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr)
    {
        GameState->NotifyLobbyRosterChanged();
    }
}

// 슬롯 기반 UI와 플레이어 명단 구독자에게 변경 알림
void ACMPlayerState::OnRep_PlayerSlotId()
{
    OnPlayerSlotChanged.Broadcast();

    if (ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr)
    {
        GameState->NotifyLobbyRosterChanged();
    }
}

// 관전·패배 UI와 조작 상태 구독자에게 참여 상태 변경 알림
void ACMPlayerState::OnRep_ParticipationState()
{
    OnParticipationStateChanged.Broadcast();
}

void ACMPlayerState::OnRep_VisionSystemEnabled()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator();
        It;
        ++It)
    {
        APlayerController* PlayerController = It->Get();
        if (!PlayerController
            || !PlayerController->IsLocalController()
            || PlayerController->GetPlayerState<APlayerState>() != this)
        {
            continue;
        }

        if (UCMVisionManagerSubsystem* VisionManager =
            World->GetSubsystem<UCMVisionManagerSubsystem>())
        {
            if (bVisionSystemEnabled)
            {
                VisionManager->EnableVisionSystem();
            }
            else
            {
                VisionManager->DisableVisionSystem();
            }
        }
        return;
    }
}
