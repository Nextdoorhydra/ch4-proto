#include "GameMode/CMGameMode.h"

#include "Game/CMControlAssignmentPolicy.h"
#include "GameMode/CMGameState.h"
#include "GameMode/Play/CMPlayGameMode.h"
#include "Player/CMPawn.h"
#include "Player/CMPlayerState.h"
#include "Player/CMPlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "ListenServerNetworkSettings.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraMultiplayer, Log, All);

// 공통 PlayerController·PlayerState·GameState·Pawn 클래스와 Seamless Travel 설정
ACMGameMode::ACMGameMode()
{
    PlayerControllerClass = ACMPlayerController::StaticClass();
    PlayerStateClass = ACMPlayerState::StaticClass();
    GameStateClass = ACMGameState::StaticClass();
    DefaultPawnClass = ACMPawn::StaticClass();
    bUseSeamlessTravel = true;
}

// 맵 시작 시 플레이어 색상과 공용 키메라 조작 상태 초기화
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

// 플레이어 이탈 시 해당 플레이어를 제외하고 조작 부위 재배정
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

// 개별 Pawn 생성 대신 플레이어 시점을 공용 키메라로 설정
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

// 신규 접속·Seamless Travel 플레이어의 색상·조작·시점 초기화
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

// 서버에서 각 플레이어에게 중복되지 않는 색상 인덱스 배정
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

// 리슨 서버 호스트 요청을 검증하고 현재 플레이 맵 재시작
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

// 현재 월드가 네트워크 설정에 등록된 기본 플레이 맵인지 확인
bool ACMGameMode::IsGameplayMap() const
{
    if (IsA<ACMPlayGameMode>())
    {
        return true;
    }

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

// 월드의 기존 공용 키메라를 찾거나 서버에서 새로 생성하여 GameState에 등록
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

// 활성 플레이어들에게 공용 키메라 조작 부위를 다시 배정
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
