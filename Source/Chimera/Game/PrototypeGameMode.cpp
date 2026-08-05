#include "PrototypeGameMode.h"

#include "ChimeraControlAssignmentPolicy.h"
#include "ChimeraGameState.h"
#include "Chimera/Player/ChimeraPrototypePawn.h"
#include "Chimera/Player/ChimeraPlayerState.h"
#include "Chimera/Player/PrototypePlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "ListenServerNetworkSettings.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraMultiplayer, Log, All);

APrototypeGameMode::APrototypeGameMode()
{
    PlayerControllerClass = APrototypePlayerController::StaticClass();
    PlayerStateClass = AChimeraPlayerState::StaticClass();
    GameStateClass = AChimeraGameState::StaticClass();
    DefaultPawnClass = AChimeraPrototypePawn::StaticClass();
    bUseSeamlessTravel = true;
}

void APrototypeGameMode::BeginPlay()
{
    Super::BeginPlay();

    AssignPlayerColors();

    if (IsGameplayMap())
    {
        EnsureSharedChimera();
        RebalanceControlAssignments();
    }
}

void APrototypeGameMode::Logout(AController* Exiting)
{
    const AChimeraPlayerState* ExitingPlayerState = Exiting
        ? Exiting->GetPlayerState<AChimeraPlayerState>()
        : nullptr;

    Super::Logout(Exiting);

    if (IsGameplayMap())
    {
        RebalanceControlAssignments(ExitingPlayerState);
    }
}

void APrototypeGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!IsGameplayMap())
    {
        return;
    }

    AChimeraPrototypePawn* SharedChimera = EnsureSharedChimera();
    if (APlayerController* PlayerController =
        Cast<APlayerController>(NewPlayer))
    {
        if (SharedChimera)
        {
            PlayerController->ClientSetViewTarget(SharedChimera);
        }
    }
}

void APrototypeGameMode::GenericPlayerInitialization(AController* C)
{
    Super::GenericPlayerInitialization(C);

    AssignPlayerColors();

    if (!IsGameplayMap())
    {
        return;
    }

    AChimeraPrototypePawn* SharedChimera = EnsureSharedChimera();
    RebalanceControlAssignments();

    if (APlayerController* PlayerController = Cast<APlayerController>(C))
    {
        if (SharedChimera)
        {
            PlayerController->ClientSetViewTarget(SharedChimera);
        }
    }
}

void APrototypeGameMode::AssignPlayerColors()
{
    if (!HasAuthority())
    {
        return;
    }

    AChimeraGameState* ChimeraGameState = GetGameState<AChimeraGameState>();
    if (!ChimeraGameState)
    {
        return;
    }

    TSet<int32> UsedColorIndices;
    for (APlayerState* PlayerState : ChimeraGameState->PlayerArray)
    {
        const AChimeraPlayerState* ChimeraPlayerState =
            Cast<AChimeraPlayerState>(PlayerState);
        if (ChimeraPlayerState
            && ChimeraPlayerState->GetPlayerColorIndex() != INDEX_NONE)
        {
            UsedColorIndices.Add(
                ChimeraPlayerState->GetPlayerColorIndex()
            );
        }
    }

    for (APlayerState* PlayerState : ChimeraGameState->PlayerArray)
    {
        AChimeraPlayerState* ChimeraPlayerState =
            Cast<AChimeraPlayerState>(PlayerState);
        if (!ChimeraPlayerState
            || ChimeraPlayerState->GetPlayerColorIndex() != INDEX_NONE)
        {
            continue;
        }

        for (int32 ColorIndex = 0;
            ColorIndex < ChimeraControl::MaxPlayers;
            ++ColorIndex)
        {
            if (!UsedColorIndices.Contains(ColorIndex))
            {
                ChimeraPlayerState->SetPlayerColorIndex(ColorIndex);
                UsedColorIndices.Add(ColorIndex);
                break;
            }
        }
    }
}

bool APrototypeGameMode::TryRetryGame(APlayerController* RequestingPlayer)
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

bool APrototypeGameMode::IsGameplayMap() const
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

AChimeraPrototypePawn* APrototypeGameMode::EnsureSharedChimera()
{
    if (!HasAuthority() || !IsGameplayMap())
    {
        return nullptr;
    }

    AChimeraGameState* ChimeraGameState = GetGameState<AChimeraGameState>();
    if (!ChimeraGameState)
    {
        return nullptr;
    }

    if (IsValid(ChimeraGameState->SharedChimera))
    {
        return ChimeraGameState->SharedChimera;
    }

    AChimeraPrototypePawn* SharedChimera = nullptr;
    for (TActorIterator<AChimeraPrototypePawn> It(GetWorld()); It; ++It)
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
                AChimeraPrototypePawn::StaticClass()
            ))
        {
            SharedPawnClass = AChimeraPrototypePawn::StaticClass();
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        SharedChimera = GetWorld()->SpawnActor<AChimeraPrototypePawn>(
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

    ChimeraGameState->SetSharedChimera(SharedChimera);
    UE_LOG(
        LogChimeraMultiplayer,
        Log,
        TEXT("Shared chimera ready: %s"),
        *GetNameSafe(SharedChimera)
    );
    return SharedChimera;
}

void APrototypeGameMode::RebalanceControlAssignments(
    const AChimeraPlayerState* ExcludedPlayerState
)
{
    if (!HasAuthority() || !IsGameplayMap())
    {
        return;
    }

    AChimeraGameState* ChimeraGameState = GetGameState<AChimeraGameState>();
    if (!ChimeraGameState || !ChimeraGameState->SharedChimera)
    {
        return;
    }

    ChimeraGameState->SharedChimera->ClearPressedControlParts();

    TArray<AChimeraPlayerState*> Players;
    for (APlayerState* PlayerState : ChimeraGameState->PlayerArray)
    {
        AChimeraPlayerState* ChimeraPlayerState =
            Cast<AChimeraPlayerState>(PlayerState);
        if (ChimeraPlayerState
            && ChimeraPlayerState != ExcludedPlayerState
            && !ChimeraPlayerState->IsOnlyASpectator())
        {
            Players.Add(ChimeraPlayerState);
        }
    }

    TArray<TArray<EChimeraControlPart>> ExistingAssignments;
    ExistingAssignments.Reserve(Players.Num());
    for (const AChimeraPlayerState* Player : Players)
    {
        ExistingAssignments.Add(Player->AssignedControlParts);
    }

    TArray<TArray<EChimeraControlPart>> NewAssignments;
    FRandomStream RandomStream(FMath::Rand());
    FChimeraControlAssignmentPolicy::Rebalance(
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
