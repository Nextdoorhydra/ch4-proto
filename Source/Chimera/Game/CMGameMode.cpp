#include "CMGameMode.h"

#include "Game/CMControlAssignmentPolicy.h"
#include "Game/CMGameState.h"
#include "Player/CMControlBody.h"
#include "Player/CMChimera.h"
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
    DefaultPawnClass = ACMChimera::StaticClass();
    bUseSeamlessTravel = true;
}

void ACMGameMode::BeginPlay()
{
    Super::BeginPlay();

    AssignPlayerColors();

    if (IsGameplayMap())
    {
        EnsureSharedChimera();
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

    // Shared Chimera는 누구도 Possess하지 않는다. 각 플레이어에게는 충돌 없는
    // ControlBody를 하나씩 생성해 네트워크 소유권과 입력 RPC의 주체로 사용한다.
    if (NewPlayer && !Cast<ACMControlBody>(NewPlayer->GetPawn()))
    {
        Super::RestartPlayer(NewPlayer);
    }

    ACMChimera* SharedChimera = EnsureSharedChimera();
    RebalanceControlAssignments();
    if (APlayerController* PlayerController =
        Cast<APlayerController>(NewPlayer))
    {
        UE_LOG(LogChimeraMultiplayer, Log,
            TEXT("Player=%s possesses ControlBody=%s"),
            *GetNameSafe(PlayerController),
            *GetNameSafe(PlayerController->GetPawn()));

        // Possess 대상은 개인 ControlBody지만, 화면은 계속 한 대의 공용
        // Shared Chimera 카메라를 사용한다.
        if (SharedChimera)
        {
            PlayerController->ClientSetViewTarget(SharedChimera);
        }
    }
}

UClass* ACMGameMode::GetDefaultPawnClassForController_Implementation(
    AController* InController
)
{
    if (IsGameplayMap())
    {
        // DefaultPawnClass는 기존 Shared Chimera 클래스 선택에 사용 중이므로
        // 플레이어 Spawn 경로에서만 ControlBody로 명확히 분리한다.
        return ACMControlBody::StaticClass();
    }

    return Super::GetDefaultPawnClassForController_Implementation(
        InController
    );
}

void ACMGameMode::GenericPlayerInitialization(AController* C)
{
    Super::GenericPlayerInitialization(C);

    AssignPlayerColors();

    if (!IsGameplayMap())
    {
        return;
    }

    ACMChimera* SharedChimera = EnsureSharedChimera();

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

ACMChimera* ACMGameMode::EnsureSharedChimera()
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

    ACMChimera* SharedChimera = nullptr;
    for (TActorIterator<ACMChimera> It(GetWorld()); It; ++It)
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
                ACMChimera::StaticClass()
            ))
        {
            SharedPawnClass = ACMChimera::StaticClass();
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        SharedChimera = GetWorld()->SpawnActor<ACMChimera>(
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

    TArray<ACMControlBody*> ControlBodies;
    TArray<ACMPlayerState*> Players;
    for (APlayerState* PlayerState : CMGameState->PlayerArray)
    {
        ACMPlayerState* CMPlayerState =
            Cast<ACMPlayerState>(PlayerState);
        if (CMPlayerState
            && CMPlayerState != ExcludedPlayerState
            && !CMPlayerState->IsOnlyASpectator())
        {
            AController* PlayerController = Cast<AController>(
                CMPlayerState->GetOwner()
            );
            ACMControlBody* ControlBody = PlayerController
                ? Cast<ACMControlBody>(PlayerController->GetPawn())
                : nullptr;
            if (ControlBody)
            {
                Players.Add(CMPlayerState);
                ControlBodies.Add(ControlBody);
            }
        }
    }

    const int32 RequestedSegmentCount = ExcludedPlayerState
        ? CMGameState->SharedChimera->GetActiveSegmentCount()
        : Players.Num();
    CMGameState->SharedChimera->SetActiveSegmentCountForPlayers(
        RequestedSegmentCount
    );

    // OwnedSegmentIndex controls player life only. Preserve existing
    // ownership and give a newly joined player the first unused Segment.
    TSet<int32> ClaimedSegmentIndices;
    for (const ACMControlBody* ControlBody : ControlBodies)
    {
        if (ControlBody
            && ControlBody->GetOwnedSegmentIndex() >= 0)
        {
            ClaimedSegmentIndices.Add(
                ControlBody->GetOwnedSegmentIndex()
            );
        }
    }
    for (ACMControlBody* ControlBody : ControlBodies)
    {
        if (!ControlBody
            || ControlBody->GetOwnedSegmentIndex() >= 0)
        {
            continue;
        }

        for (int32 SegmentIndex = 0;
            SegmentIndex
                < CMGameState->SharedChimera->GetActiveSegmentCount();
            ++SegmentIndex)
        {
            if (!ClaimedSegmentIndices.Contains(SegmentIndex))
            {
                ControlBody->SetOwnedSegmentIndex(SegmentIndex);
                ClaimedSegmentIndices.Add(SegmentIndex);
                break;
            }
        }
    }

    TArray<TArray<FCMPartSlotAddress>> ExistingAssignments;
    ExistingAssignments.Reserve(ControlBodies.Num());
    for (const ACMControlBody* ControlBody : ControlBodies)
    {
        ExistingAssignments.Add(ControlBody->GetControlSlots());
    }

    TArray<TArray<FCMPartSlotAddress>> NewAssignments;
    FRandomStream RandomStream(FMath::Rand());
    FCMControlAssignmentPolicy::Rebalance(
        ExistingAssignments,
        CMGameState->SharedChimera->GetActiveSegmentCount(),
        RandomStream,
        NewAssignments
    );

    for (int32 PlayerIndex = 0;
        PlayerIndex < Players.Num();
        ++PlayerIndex)
    {
        ControlBodies[PlayerIndex]->SetControlSlots(
            NewAssignments[PlayerIndex]
        );

        UE_LOG(
            LogChimeraMultiplayer,
            Log,
            TEXT("Assigned %d mixed PartSlot(s) to %s. OwnedSegment=%d"),
            NewAssignments[PlayerIndex].Num(),
            *Players[PlayerIndex]->GetPlayerName(),
            ControlBodies[PlayerIndex]->GetOwnedSegmentIndex()
        );
    }
}
