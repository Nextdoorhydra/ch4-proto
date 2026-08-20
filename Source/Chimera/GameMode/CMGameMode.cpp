#include "GameMode/CMGameMode.h"

#include "Game/CMControlAssignmentPolicy.h"
#include "GameMode/CMGameState.h"
#include "GameMode/Play/CMPlayGameMode.h"
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

// 공통 PlayerController·PlayerState·GameState·Pawn 클래스와 Seamless Travel 설정
ACMGameMode::ACMGameMode()
{
    PlayerControllerClass = ACMPlayerController::StaticClass();
    PlayerStateClass = ACMPlayerState::StaticClass();
    GameStateClass = ACMGameState::StaticClass();
    DefaultPawnClass = ACMChimera::StaticClass();
    bUseSeamlessTravel = true;
}

// 맵 시작 시 플레이어 색상과 공용 키메라 조작 상태 초기화
void ACMGameMode::BeginPlay()
{
    Super::BeginPlay();

    AssignPlayerSlots();
    AssignPlayerColors();

    if (IsGameplayMap())
    {
        EnsureSharedChimera();
    }
}

// 플레이어 이탈 시 해당 플레이어를 제외하고 조작 부위 재배정
void ACMGameMode::Logout(AController* Exiting)
{
    ACMPlayerState* ExitingPlayerState = Exiting
        ? Exiting->GetPlayerState<ACMPlayerState>()
        : nullptr;

    const bool bPreserveAssignment = ExitingPlayerState
        && IsGameplayMap()
        && ShouldPreservePlayerOnLogout(Exiting, ExitingPlayerState);
    if (ExitingPlayerState && IsGameplayMap() && !bPreserveAssignment)
    {
        ExitingPlayerState->SetParticipationState(
            ECMPlayerParticipationState::Disconnected);
    }

    Super::Logout(Exiting);

    if (IsGameplayMap() && !bPreserveAssignment)
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

    // Shared Chimera는 누구도 Possess하지 않는다. 각 플레이어에게는 충돌 없는
    // ControlBody를 하나씩 생성해 네트워크 소유권과 입력 RPC의 주체로 사용한다.
    if (NewPlayer && !Cast<ACMControlBody>(NewPlayer->GetPawn()))
    {
        Super::RestartPlayer(NewPlayer);
    }

    ACMChimera* SharedChimera = EnsureSharedChimera();
    if (!RestorePreservedControlAssignment(NewPlayer, SharedChimera))
    {
        RebalanceControlAssignments();
    }
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

// 신규 접속·Seamless Travel 플레이어의 색상·조작·시점 초기화
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

// 기본 GameMode는 이탈 즉시 플레이어 배정을 해제
bool ACMGameMode::ShouldPreservePlayerOnLogout(
    AController* Exiting,
    const ACMPlayerState* ExitingPlayerState) const
{
    return false;
}

// 기본 GameMode에는 복원할 재접속 배정이 없음
bool ACMGameMode::RestorePreservedControlAssignment(
    AController* NewPlayer,
    ACMChimera* SharedChimera)
{
    return false;
}

void ACMGameMode::GenericPlayerInitialization(AController* C)
{
    Super::GenericPlayerInitialization(C);

    AssignPlayerSlots();
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

// 현재 참가자에게 중복되지 않는 고정 슬롯을 입장 순서대로 배정
void ACMGameMode::AssignPlayerSlots()
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

    TSet<int32> UsedSlotIds;
    for (APlayerState* PlayerState : CMGameState->PlayerArray)
    {
        const ACMPlayerState* CMPlayerState =
            Cast<ACMPlayerState>(PlayerState);
        if (CMPlayerState && CMPlayerState->GetPlayerSlotId() != INDEX_NONE)
        {
            UsedSlotIds.Add(CMPlayerState->GetPlayerSlotId());
        }
    }

    for (APlayerState* PlayerState : CMGameState->PlayerArray)
    {
        ACMPlayerState* CMPlayerState = Cast<ACMPlayerState>(PlayerState);
        if (!CMPlayerState
            || CMPlayerState->IsOnlyASpectator()
            || CMPlayerState->GetPlayerSlotId() != INDEX_NONE)
        {
            continue;
        }

        for (int32 SlotId = 0; SlotId < CMControl::MaxPlayers; ++SlotId)
        {
            if (!UsedSlotIds.Contains(SlotId))
            {
                CMPlayerState->SetPlayerSlotId(SlotId);
                UsedSlotIds.Add(SlotId);
                break;
            }
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
        // 공용 키메라는 레벨에 배치된 첫 PlayerStart에서 생성한다.
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

    const int32 RequestedPlayerCount = ExcludedPlayerState
        ? CMGameState->SharedChimera->GetActiveSegmentCount()
            / CMControl::SegmentsPerPlayer
        : Players.Num();
    CMGameState->SharedChimera->SetActiveSegmentCountForPlayers(
        RequestedPlayerCount
    );

    // OwnedSegmentIndex is the first of the player's two consecutive
    // Segments. Preserve existing pairs and give a new player an unused pair.
    TSet<int32> ClaimedSegmentIndices;
    const int32 ActiveSegmentCount =
        CMGameState->SharedChimera->GetActiveSegmentCount();
    for (const ACMControlBody* ControlBody : ControlBodies)
    {
        if (ControlBody
            && ControlBody->GetOwnedSegmentIndex() >= 0
            && ControlBody->GetOwnedSegmentIndex()
                % CMControl::SegmentsPerPlayer == 0
            && ControlBody->GetOwnedSegmentIndex() + 1
                < ActiveSegmentCount)
        {
            ClaimedSegmentIndices.Add(
                ControlBody->GetOwnedSegmentIndex()
            );
            ClaimedSegmentIndices.Add(
                ControlBody->GetOwnedSegmentIndex() + 1
            );
        }
    }
    for (ACMControlBody* ControlBody : ControlBodies)
    {
        const bool bHasValidOwnedPair = ControlBody
            && ControlBody->GetOwnedSegmentIndex() >= 0
            && ControlBody->GetOwnedSegmentIndex()
                % CMControl::SegmentsPerPlayer == 0
            && ControlBody->GetOwnedSegmentIndex() + 1
                < ActiveSegmentCount;
        if (!ControlBody || bHasValidOwnedPair)
        {
            continue;
        }

        for (int32 SegmentIndex = 0;
            SegmentIndex + 1
                < ActiveSegmentCount;
            SegmentIndex += CMControl::SegmentsPerPlayer)
        {
            if (!ClaimedSegmentIndices.Contains(SegmentIndex)
                && !ClaimedSegmentIndices.Contains(SegmentIndex + 1))
            {
                ControlBody->SetOwnedSegmentIndex(SegmentIndex);
                ClaimedSegmentIndices.Add(SegmentIndex);
                ClaimedSegmentIndices.Add(SegmentIndex + 1);
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
            TEXT("Assigned %d mixed PartSlot(s) to %s. OwnedSegments=%d,%d"),
            NewAssignments[PlayerIndex].Num(),
            *Players[PlayerIndex]->GetPlayerName(),
            ControlBodies[PlayerIndex]->GetOwnedSegmentIndex(),
            ControlBodies[PlayerIndex]->GetOwnedSegmentIndex() + 1
        );
    }
}
