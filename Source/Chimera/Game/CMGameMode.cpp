#include "CMGameMode.h"

#include "Game/CMControlAssignmentPolicy.h"
#include "Game/CMGameState.h"
#include "Player/CMPawn.h"
#include "Player/CMPlayerState.h"
#include "Player/CMPlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "ListenServerNetworkSettings.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraMultiplayer, Log, All);

ACMGameMode::ACMGameMode()
{
    PlayerControllerClass = ACMPlayerController::StaticClass();
    PlayerStateClass = ACMPlayerState::StaticClass();
    GameStateClass = ACMGameState::StaticClass();
    DefaultPawnClass = ACMPawn::StaticClass();
    bUseSeamlessTravel = true;
}

void ACMGameMode::BeginPlay()
{
    Super::BeginPlay();

    AssignPlayerColors();

    if (IsGameplayMap())
    {
        EnsureSharedChimera();
        RebalanceControlAssignments();
    }
}

void ACMGameMode::Logout(AController* Exiting)
{
    const ACMPlayerState* ExitingPlayerState = Exiting
        ? Exiting->GetPlayerState<ACMPlayerState>()
        : nullptr;

    Super::Logout(Exiting);

    if (IsGameplayMap())
    {
        RebalanceControlAssignments(ExitingPlayerState);
    }
}

void ACMGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!IsGameplayMap())
    {
        return;
    }

    ACMPawn* SharedChimera = EnsureSharedChimera();
    if (APlayerController* PlayerController =
        Cast<APlayerController>(NewPlayer))
    {
        if (SharedChimera)
        {
            PlayerController->ClientSetViewTarget(SharedChimera);
        }
    }
}

void ACMGameMode::GenericPlayerInitialization(AController* C)
{
    Super::GenericPlayerInitialization(C);

    AssignPlayerColors();

    if (!IsGameplayMap())
    {
        return;
    }

    ACMPawn* SharedChimera = EnsureSharedChimera();
    RebalanceControlAssignments();

    if (APlayerController* PlayerController = Cast<APlayerController>(C))
    {
        if (SharedChimera)
        {
            PlayerController->ClientSetViewTarget(SharedChimera);
        }
    }
}

void ACMGameMode::AssignPlayerColors()
{
    if (!HasAuthority())
    {
        return;
    }

    ACMGameState* CMGameState = GetGameState<ACMGameState>();
    if (!CMGameState)
    {
        return;
    }

    TSet<int32> UsedColorIndices;
    for (APlayerState* PlayerState : CMGameState->PlayerArray)
    {
        const ACMPlayerState* CMPlayerState =
            Cast<ACMPlayerState>(PlayerState);
        if (CMPlayerState
            && CMPlayerState->GetPlayerColorIndex() != INDEX_NONE)
        {
            UsedColorIndices.Add(
                CMPlayerState->GetPlayerColorIndex()
            );
        }
    }

    for (APlayerState* PlayerState : CMGameState->PlayerArray)
    {
        ACMPlayerState* CMPlayerState =
            Cast<ACMPlayerState>(PlayerState);
        if (!CMPlayerState
            || CMPlayerState->GetPlayerColorIndex() != INDEX_NONE)
        {
            continue;
        }

        for (int32 ColorIndex = 0;
            ColorIndex < CMControl::MaxPlayers;
            ++ColorIndex)
        {
            if (!UsedColorIndices.Contains(ColorIndex))
            {
                CMPlayerState->SetPlayerColorIndex(ColorIndex);
                UsedColorIndices.Add(ColorIndex);
                break;
            }
        }
    }
}

bool ACMGameMode::TryRetryGame(APlayerController* RequestingPlayer)
{
    if (!HasAuthority()
        || GetNetMode() != NM_ListenServer
        || !IsGameplayMap()
        || bRetryInProgress
        || !IsValid(RequestingPlayer)
        || !RequestingPlayer->IsLocalController())
    {
        return false;
    }

    bRetryInProgress = GetWorld()->ServerTravel(TEXT("?Restart"), false);
    UE_LOG(
        LogChimeraMultiplayer,
        Log,
        TEXT("Host requested game retry. Started=%d"),
        bRetryInProgress
    );
    return bRetryInProgress;
}

bool ACMGameMode::IsGameplayMap() const
{
    const UWorld* World = GetWorld();
    const UListenServerNetworkSettings* NetworkSettings =
        GetDefault<UListenServerNetworkSettings>();
    if (!World || !NetworkSettings)
    {
        return false;
    }

    const FString GameMapPackage = FPackageName::ObjectPathToPackageName(
        NetworkSettings->DefaultGameMap.ToString()
    );
    const FString GameMapName = FPackageName::GetShortName(GameMapPackage);
    const FString CurrentMapName = UGameplayStatics::GetCurrentLevelName(
        World,
        true
    );

    return !GameMapName.IsEmpty() && CurrentMapName == GameMapName;
}

ACMPawn* ACMGameMode::EnsureSharedChimera()
{
    if (!HasAuthority() || !IsGameplayMap())
    {
        return nullptr;
    }

    ACMGameState* CMGameState = GetGameState<ACMGameState>();
    if (!CMGameState)
    {
        return nullptr;
    }

    if (IsValid(CMGameState->SharedChimera))
    {
        return CMGameState->SharedChimera;
    }

    ACMPawn* SharedChimera = nullptr;
    for (TActorIterator<ACMPawn> It(GetWorld()); It; ++It)
    {
        SharedChimera = *It;
        break;
    }

    if (!SharedChimera)
    {
        FTransform SpawnTransform = FTransform::Identity;
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        {
            SpawnTransform = It->GetActorTransform();
            break;
        }

        UClass* SharedPawnClass = DefaultPawnClass;
        if (!SharedPawnClass
            || !SharedPawnClass->IsChildOf(
                ACMPawn::StaticClass()
            ))
        {
            SharedPawnClass = ACMPawn::StaticClass();
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        SharedChimera = GetWorld()->SpawnActor<ACMPawn>(
            SharedPawnClass,
            SpawnTransform,
            SpawnParameters
        );
    }

    if (!SharedChimera)
    {
        UE_LOG(
            LogChimeraMultiplayer,
            Error,
            TEXT("Failed to create the shared chimera pawn.")
        );
        return nullptr;
    }

    if (AController* ExistingController = SharedChimera->GetController())
    {
        ExistingController->UnPossess();
    }

    CMGameState->SetSharedChimera(SharedChimera);
    UE_LOG(
        LogChimeraMultiplayer,
        Log,
        TEXT("Shared chimera ready: %s"),
        *GetNameSafe(SharedChimera)
    );
    return SharedChimera;
}

void ACMGameMode::RebalanceControlAssignments(
    const ACMPlayerState* ExcludedPlayerState
)
{
    if (!HasAuthority() || !IsGameplayMap())
    {
        return;
    }

    ACMGameState* CMGameState = GetGameState<ACMGameState>();
    if (!CMGameState || !CMGameState->SharedChimera)
    {
        return;
    }

    CMGameState->SharedChimera->ClearPressedControlParts();

    TArray<ACMPlayerState*> Players;
    for (APlayerState* PlayerState : CMGameState->PlayerArray)
    {
        ACMPlayerState* CMPlayerState =
            Cast<ACMPlayerState>(PlayerState);
        if (CMPlayerState
            && CMPlayerState != ExcludedPlayerState
            && !CMPlayerState->IsOnlyASpectator())
        {
            Players.Add(CMPlayerState);
        }
    }

    TArray<TArray<ECMControlPart>> ExistingAssignments;
    ExistingAssignments.Reserve(Players.Num());
    for (const ACMPlayerState* Player : Players)
    {
        ExistingAssignments.Add(Player->AssignedControlParts);
    }

    TArray<TArray<ECMControlPart>> NewAssignments;
    FRandomStream RandomStream(FMath::Rand());
    FCMControlAssignmentPolicy::Rebalance(
        ExistingAssignments,
        RandomStream,
        NewAssignments
    );

    for (int32 PlayerIndex = 0;
        PlayerIndex < Players.Num();
        ++PlayerIndex)
    {
        Players[PlayerIndex]->SetAssignedControlParts(
            NewAssignments[PlayerIndex]
        );

        UE_LOG(
            LogChimeraMultiplayer,
            Log,
            TEXT("Assigned %d control part(s) to %s."),
            NewAssignments[PlayerIndex].Num(),
            *Players[PlayerIndex]->GetPlayerName()
        );
    }
}
