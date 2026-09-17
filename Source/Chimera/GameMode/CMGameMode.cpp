#include "GameMode/CMGameMode.h"

#include "Game/CMControlAssignmentPolicy.h"
#include "GameMode/CMGameState.h"
#include "GameMode/Play/CMPlayGameMode.h"
#include "GameMode/StageRoute/CMStageRouteSubsystem.h"
#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"
#include "Player/CMControlBody.h"
#include "Player/CMChimera.h"
#include "Player/CMPlayerState.h"
#include "Player/CMPlayerController.h"
#include "Engine/AssetManager.h"
#include "EngineUtils.h"
#include "Engine/LevelStreaming.h"
#include "GameFramework/PlayerStart.h"
#include "Stage/Test/CMTestAreaManager.h"
#include "Kismet/GameplayStatics.h"
#include "ListenServerNetworkSettings.h"
#include "Misc/PackageName.h"
#include "Sound/CMGameSoundBridgeSubsystem.h"
#include "Sound/CMSoundTags.h"
#include "Sound/NKMSoundSubsystem.h"
#include "TimerManager.h"
#include "Vision/CMVisionManagerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraMultiplayer, Log, All);

// 공통 PlayerController·PlayerState·GameState·Pawn 클래스와 Seamless Travel 설정
ACMGameMode::ACMGameMode()
{
    PlayerControllerClass = ACMPlayerController::StaticClass();
    PlayerStateClass = ACMPlayerState::StaticClass();
    GameStateClass = ACMGameState::StaticClass();
    DefaultPawnClass = ACMChimera::StaticClass();
    bUseSeamlessTravel = true;
    MainMenuLoadScheduleId = FPrimaryAssetId(
        FPrimaryAssetType(TEXT("CMStageLoadSchedule")),
        TEXT("PDA_CMLoadSchedule_MainMenu"));
}

// 맵 시작 시 플레이어 색상과 공용 키메라 조작 상태 초기화
void ACMGameMode::BeginPlay()
{
    Super::BeginPlay();

    AssignPlayerSlots();
    AssignPlayerColors();

    if (IsMainMenuMap())
    {
        if (UCMVisionManagerSubsystem* VisionManager =
                GetWorld()->GetSubsystem<UCMVisionManagerSubsystem>())
        {
            VisionManager->DisableVisionSystem();
        }

        bMainMenuAssetsResolved = false;
        bMainMenuAssetsLoaded = false;
        bMainMenuPresentationReady = false;
        bMainMenuFinishScheduled = false;
        if (ACMGameState* CMGameState = GetGameState<ACMGameState>())
        {
            CMGameState->SetWorldPresentationState(
                ECMWorldPresentationState::Loading);
        }
        MainMenuPresentationPollTimer = GetWorldTimerManager().SetTimerForNextTick(
            this, &ThisClass::CheckMainMenuPresentationReady);
        GetWorldTimerManager().SetTimer(
            MainMenuPresentationTimeoutTimer,
            this,
            &ThisClass::HandleMainMenuLoadingTimeout,
            MainMenuPresentationTimeoutSeconds,
            false);
        StartMainMenuAudio();
    }
    else if (IsGameplayMap())
    {
        if (UCMVisionManagerSubsystem* VisionManager =
                GetWorld()->GetSubsystem<UCMVisionManagerSubsystem>())
        {
            VisionManager->EnableVisionSystem();
        }

        if (ACMGameState* CMGameState = GetGameState<ACMGameState>())
        {
            CMGameState->SetSoloTestMode(IsSoloTestMode());
        }
        EnsureSharedChimera();
    }
}

void ACMGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(MainMenuPresentationPollTimer);
    GetWorldTimerManager().ClearTimer(MainMenuPresentationFinishTimer);
    GetWorldTimerManager().ClearTimer(MainMenuPresentationTimeoutTimer);
    StopMainMenuAudio();
    Super::EndPlay(EndPlayReason);
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
            // 서버의 복제 시점도 키메라를 따라야 한다. SetViewTarget은 원격 클라이언트에도 전달된다.
            PlayerController->SetViewTarget(SharedChimera);
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

    if (ACMPlayerState* CMPlayerState =
            C ? C->GetPlayerState<ACMPlayerState>() : nullptr)
    {
        CMPlayerState->SetVisionSystemEnabled(IsGameplayMap());
    }

    if (!IsGameplayMap())
    {
        return;
    }

    ACMChimera* SharedChimera = EnsureSharedChimera();

    if (APlayerController* PlayerController = Cast<APlayerController>(C))
    {
        if (SharedChimera)
        {
            PlayerController->SetViewTarget(SharedChimera);
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

bool ACMGameMode::IsMainMenuMap() const
{
    const UWorld* World = GetWorld();
    const UListenServerNetworkSettings* NetworkSettings =
        GetDefault<UListenServerNetworkSettings>();
    if (!World || !NetworkSettings)
    {
        return false;
    }

    const FString MainMenuPackage = FPackageName::ObjectPathToPackageName(
        NetworkSettings->MainMenuMap.ToString());
    const FString MainMenuName = FPackageName::GetShortName(MainMenuPackage);
    return !MainMenuName.IsEmpty()
        && UGameplayStatics::GetCurrentLevelName(World, true) == MainMenuName;
}

void ACMGameMode::StartMainMenuAudio()
{
    if (GetNetMode() == NM_DedicatedServer
        || !MainMenuLoadScheduleId.IsValid()
        || !GetGameInstance())
    {
        bMainMenuAssetsResolved = true;
        bMainMenuAssetsLoaded = false;
        TryFinishMainMenuLoading();
        return;
    }

    MainMenuLoadCoordinator = GetGameInstance()->GetSubsystem<
        UCMStageLoadCoordinatorSubsystem>();
    if (!MainMenuLoadCoordinator)
    {
        bMainMenuAssetsResolved = true;
        bMainMenuAssetsLoaded = false;
        TryFinishMainMenuLoading();
        return;
    }

    MainMenuLoadCoordinator->OnStageStartRequiredFinished.AddUniqueDynamic(
        this,
        &ThisClass::HandleMainMenuLoadFinished);
    MainMenuLoadRequestId = FGuid::NewGuid();
    if (!MainMenuLoadCoordinator->StartStageScheduleRequest(
            MainMenuLoadScheduleId,
            MainMenuLoadRequestId))
    {
        UE_LOG(LogChimeraMultiplayer, Error,
            TEXT("Main menu audio schedule failed to start. Schedule=%s"),
            *MainMenuLoadScheduleId.ToString());
        StopMainMenuAudio();
        bMainMenuAssetsResolved = true;
        bMainMenuAssetsLoaded = false;
        TryFinishMainMenuLoading();
    }
}

void ACMGameMode::StopMainMenuAudio()
{
    if (MainMenuLoadCoordinator)
    {
        MainMenuLoadCoordinator->OnStageStartRequiredFinished.RemoveDynamic(
            this,
            &ThisClass::HandleMainMenuLoadFinished);
        MainMenuLoadCoordinator = nullptr;
    }
    MainMenuLoadRequestId.Invalidate();

    if (IsMainMenuMap() && GetGameInstance())
    {
        if (UNKMSoundSubsystem* SoundSubsystem =
                GetGameInstance()->GetSubsystem<UNKMSoundSubsystem>())
        {
            SoundSubsystem->StopBGM();
        }
    }
}

void ACMGameMode::HandleMainMenuLoadFinished(
    FGuid RequestId,
    bool bSucceeded)
{
    if (RequestId != MainMenuLoadRequestId)
    {
        return;
    }

    if (MainMenuLoadCoordinator)
    {
        MainMenuLoadCoordinator->OnStageStartRequiredFinished.RemoveDynamic(
            this,
            &ThisClass::HandleMainMenuLoadFinished);
    }
    MainMenuLoadRequestId.Invalidate();
    bMainMenuAssetsResolved = true;
    bMainMenuAssetsLoaded = bSucceeded;
    TryFinishMainMenuLoading();

    if (!bSucceeded || !GetGameInstance())
    {
        UE_LOG(LogChimeraMultiplayer, Error,
            TEXT("Main menu audio schedule failed. Schedule=%s"),
            *MainMenuLoadScheduleId.ToString());
        return;
    }

    if (UCMGameSoundBridgeSubsystem* SoundBridge =
            GetGameInstance()->GetSubsystem<UCMGameSoundBridgeSubsystem>())
    {
        SoundBridge->RebuildRegisteredSoundCatalogs();
    }
    if (UNKMSoundSubsystem* SoundSubsystem =
            GetGameInstance()->GetSubsystem<UNKMSoundSubsystem>())
    {
        const FPrimaryAssetId SharedBGMCatalogId(
            FPrimaryAssetType(TEXT("NKMSoundDataAsset")),
            TEXT("DA_CMSound_Stage01"));
        if (const FSoftObjectPath CatalogPath =
                UAssetManager::Get().GetPrimaryAssetPath(SharedBGMCatalogId);
            CatalogPath.IsValid())
        {
            SoundSubsystem->RegisterSoundCatalog(CatalogPath.TryLoad());
        }

        // 현재 프로젝트는 하나의 공용 BGM을 메뉴와 모든 스테이지에서 사용한다.
        SoundSubsystem->PlayBGM(CMSoundTags::BGM_Stage_Stage01);
    }
}

void ACMGameMode::CheckMainMenuPresentationReady()
{
    if (!IsMainMenuMap() || bMainMenuPresentationReady)
    {
        return;
    }

    UWorld* World = GetWorld();
    ULevelStreaming* PresentationLevel = UGameplayStatics::GetStreamingLevel(
        this, MainMenuPresentationLevelName);
    if (PresentationLevel)
    {
        // 맵 재진입 시 이전 스트리밍 상태가 남아 있어도 메뉴 연출 레벨을 다시 요청한다.
        PresentationLevel->SetShouldBeLoaded(true);
        PresentationLevel->SetShouldBeVisible(true);
    }
    bool bAllRequestedLevelsReady = World
        && PresentationLevel
        && PresentationLevel->IsLevelLoaded()
        && PresentationLevel->IsLevelVisible()
        && !World->IsVisibilityRequestPending();
    if (bAllRequestedLevelsReady)
    {
        for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
        {
            if (!StreamingLevel)
            {
                continue;
            }

            if (StreamingLevel->IsStreamingStatePending()
                || (StreamingLevel->ShouldBeLoaded()
                    && !StreamingLevel->IsLevelLoaded())
                || (StreamingLevel->ShouldBeVisible()
                    && !StreamingLevel->IsLevelVisible()))
            {
                bAllRequestedLevelsReady = false;
                break;
            }
        }
    }

    if (bAllRequestedLevelsReady)
    {
        bMainMenuPresentationReady = true;
        TryFinishMainMenuLoading();
        return;
    }

    MainMenuPresentationPollTimer = GetWorldTimerManager().SetTimerForNextTick(
        this, &ThisClass::CheckMainMenuPresentationReady);
}

void ACMGameMode::TryFinishMainMenuLoading()
{
    if (bMainMenuFinishScheduled
        || !bMainMenuAssetsResolved
        || !bMainMenuPresentationReady)
    {
        return;
    }

    bMainMenuFinishScheduled = true;
    MainMenuPresentationFinishTimer = GetWorldTimerManager().SetTimerForNextTick(
        this, &ThisClass::FinishMainMenuLoading);
}

void ACMGameMode::FinishMainMenuLoading()
{
    GetWorldTimerManager().ClearTimer(MainMenuPresentationPollTimer);
    GetWorldTimerManager().ClearTimer(MainMenuPresentationTimeoutTimer);

    if (ACMGameState* CMGameState = GetGameState<ACMGameState>())
    {
        CMGameState->SetWorldPresentationState(
            bMainMenuAssetsLoaded
                ? ECMWorldPresentationState::Ready
                : ECMWorldPresentationState::Failed);
    }

    UE_LOG(LogChimeraMultiplayer, Display,
        TEXT("Main menu presentation finished. AssetsLoaded=%s Level=%s"),
        bMainMenuAssetsLoaded ? TEXT("true") : TEXT("false"),
        *MainMenuPresentationLevelName.ToString());
}

void ACMGameMode::HandleMainMenuLoadingTimeout()
{
    GetWorldTimerManager().ClearTimer(MainMenuPresentationPollTimer);
    GetWorldTimerManager().ClearTimer(MainMenuPresentationFinishTimer);
    bMainMenuFinishScheduled = true;

    if (ACMGameState* CMGameState = GetGameState<ACMGameState>())
    {
        CMGameState->SetWorldPresentationState(
            ECMWorldPresentationState::Failed);
    }

    UE_LOG(LogChimeraMultiplayer, Error,
        TEXT("Main menu presentation timed out. AssetsResolved=%s AssetsLoaded=%s LevelReady=%s Level=%s"),
        bMainMenuAssetsResolved ? TEXT("true") : TEXT("false"),
        bMainMenuAssetsLoaded ? TEXT("true") : TEXT("false"),
        bMainMenuPresentationReady ? TEXT("true") : TEXT("false"),
        *MainMenuPresentationLevelName.ToString());
}

bool ACMGameMode::IsSoloTestMode() const
{
    const UCMStageRouteSubsystem* StageRoute = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMStageRouteSubsystem>()
        : nullptr;
    if (StageRoute && StageRoute->IsSoloTestMode())
    {
        return true;
    }

#if WITH_EDITOR
    const UWorld* World = GetWorld();
    if (!World || World->WorldType != EWorldType::PIE)
    {
        return false;
    }

    if (GetNetMode() == NM_Standalone)
    {
        return true;
    }

    const ACMGameState* CMGameState = GetGameState<ACMGameState>();
    return GetNetMode() == NM_ListenServer
        && CMGameState
        && CMGameState->GetLobbyPlayerCount() == 1;
#else
    return false;
#endif
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
        APlayerStart* FallbackPlayerStart = nullptr;
        APlayerStart* TaggedPlayerStart = nullptr;
        // 태그가 설정된 경우 일치하는 PlayerStart를 우선하고 기존 맵은 첫 시작점을 사용
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        {
            if (!FallbackPlayerStart)
            {
                FallbackPlayerStart = *It;
            }
            if (!SharedChimeraPlayerStartTag.IsNone()
                && It->PlayerStartTag == SharedChimeraPlayerStartTag)
            {
                TaggedPlayerStart = *It;
                break;
            }
        }

        APlayerStart* TestAreaPlayerStart = nullptr;
        if (SharedChimeraPlayerStartTag.IsNone())
        {
            for (TActorIterator<ACMTestAreaManager> It(GetWorld()); It; ++It)
            {
                TestAreaPlayerStart = It->GetInitialPlayerStart();
                break;
            }
        }

        APlayerStart* SelectedPlayerStart = TaggedPlayerStart
            ? TaggedPlayerStart
            : (TestAreaPlayerStart ? TestAreaPlayerStart : FallbackPlayerStart);
        if (SelectedPlayerStart)
        {
            SpawnTransform = SelectedPlayerStart->GetActorTransform();
        }
        if (!SharedChimeraPlayerStartTag.IsNone() && !TaggedPlayerStart)
        {
            UE_LOG(LogChimeraMultiplayer, Warning,
                TEXT("공용 키메라 PlayerStartTag를 찾지 못해 첫 시작점을 사용합니다. Tag=%s"),
                *SharedChimeraPlayerStartTag.ToString());
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

    // 플레이어 모두의 카메라 대상인 공용 인스턴스만 거리와 무관하게 유지한다.
    // 생성자 기본값이 아니라 여기서 지정해 기존 BP 설정에도 적용한다.
    SharedChimera->bAlwaysRelevant = true;
    SharedChimera->ForceNetUpdate();
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

    const bool bSoloTestMode = IsSoloTestMode();
    CMGameState->SetSoloTestMode(bSoloTestMode);
    const int32 RequestedPlayerCount = bSoloTestMode
        ? CMControl::SoloTestSegmentCount
            / CMControl::SegmentsPerPlayer
        : ExcludedPlayerState
            ? CMGameState->SharedChimera->GetActiveSegmentCount()
                / CMControl::SegmentsPerPlayer
            : Players.Num();
    CMGameState->SharedChimera->SetActiveSegmentCountForPlayers(
        RequestedPlayerCount
    );

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
        ACMControlBody* ControlBody = ControlBodies[PlayerIndex];
        ControlBody->SetControlSlots(NewAssignments[PlayerIndex]);
    }
}
