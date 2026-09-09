#include "CMPlayerController.h"

#include "Player/CMPlayerState.h"
#include "GameMode/CMGameState.h"
#include "GameMode/CMGameMode.h"
#include "GameMode/Play/CMPlayGameMode.h"
#include "GameMode/Play/CMPlayGameState.h"
#include "GameMode/Lobby/CMLobbyGameMode.h"
#include "AsyncLoad/CMClientStageLoadComponent.h"
#include "Player/CMControlBody.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Leg/CMLegPart.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputCoreTypes.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Stage/Test/CMTestAreaManager.h"
#include "Vision/CMVisionInputComponent.h"
#include "Ping/CMPingSelectorWidget.h"
#include "Ping/CMWorldPing.h"
#include "Ping/CMPingTypes.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPlayerController, Log, All);

namespace
{
    const FKey SoloTestControlKeys[CMControl::SoloTestKeyCount] = {
        EKeys::Q, EKeys::W, EKeys::E, EKeys::R,
        EKeys::A, EKeys::S, EKeys::D, EKeys::F,
        EKeys::P, EKeys::O, EKeys::I, EKeys::U,
        EKeys::L, EKeys::K, EKeys::J, EKeys::H
    };

    int32 FindSoloTestControlKeyIndex(const FKey Key)
    {
        for (int32 Index = 0;
            Index < CMControl::SoloTestKeyCount;
            ++Index)
        {
            if (SoloTestControlKeys[Index] == Key)
            {
                return Index;
            }
        }
        return INDEX_NONE;
    }
}

ACMPlayerController::ACMPlayerController()
{
    // APlayerController Tick은 입력 처리와 Non-Shipping 화살표 치트 전송에 사용한다.
    // Shared Chimera 검색은 이벤트 기반이므로 매 프레임 수행하지 않는다.
    PrimaryActorTick.bCanEverTick = true;
    ClientStageLoadComponent = CreateDefaultSubobject<UCMClientStageLoadComponent>(
        TEXT("ClientStageLoadComponent"));
    bAutoManageActiveCameraTarget = false;

    VisionInputComponent = CreateDefaultSubobject<UCMVisionInputComponent>(
        TEXT("VisionInputComponent")
    );

    for (FCMPartSlotAddress& PressedPartSlot : SoloPressedPartSlots)
    {
        PressedPartSlot = FCMPartSlotAddress();
    }
}

bool ACMPlayerController::CanRequestRetryGame() const
{
    const ACMPlayGameState* PlayState = GetWorld()
        ? GetWorld()->GetGameState<ACMPlayGameState>() : nullptr;
    const APlayerState* LocalPlayerState = PlayerState;
    return IsLocalController()
        && PlayState
        && PlayState->GetPlayPhase() == ECMPlayPhase::Playing
        && IsValid(LocalPlayerState)
        && !LocalPlayerState->IsOnlyASpectator()
        && !PlayState->GetRetryVoteSnapshot().VotedPlayerIds.Contains(
            LocalPlayerState->GetPlayerId());
}

void ACMPlayerController::RequestRetryGame()
{
    if (!CanRequestRetryGame())
    {
        const ACMPlayGameState* PlayState = GetWorld()
            ? GetWorld()->GetGameState<ACMPlayGameState>() : nullptr;
        const APlayerState* LocalPlayerState = PlayerState;
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[RetryVote][LocalRequest] Rejected Controller=%s NetMode=%d Phase=%d PlayerState=%d PlayerId=%d Spectator=%d AlreadyVoted=%d"),
            *GetName(), static_cast<int32>(GetNetMode()),
            PlayState ? static_cast<int32>(PlayState->GetPlayPhase()) : INDEX_NONE,
            IsValid(LocalPlayerState),
            LocalPlayerState ? LocalPlayerState->GetPlayerId() : INDEX_NONE,
            LocalPlayerState && LocalPlayerState->IsOnlyASpectator(),
            PlayState && LocalPlayerState
                && PlayState->GetRetryVoteSnapshot().VotedPlayerIds.Contains(
                    LocalPlayerState->GetPlayerId()));
        return;
    }

    bRetryVoteHoldActive = true;
    RetryVoteHoldStartTime = GetWorld()
        ? GetWorld()->GetTimeSeconds() : 0.0;
    UE_LOG(LogChimeraPlayerController, Display,
        TEXT("[RetryVote][LocalRequest] Hold started Controller=%s NetMode=%d PlayerId=%d StartTime=%.3f"),
        *GetName(), static_cast<int32>(GetNetMode()),
        PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE,
        RetryVoteHoldStartTime);
    ServerRequestRetryGame();
}

void ACMPlayerController::CancelRetryGameRequest()
{
    const bool bWasHolding = bRetryVoteHoldActive;
    const float ReleasedProgress = GetRetryVoteHoldProgress();
    bRetryVoteHoldActive = false;
    UE_LOG(LogChimeraPlayerController, Display,
        TEXT("[RetryVote][LocalRequest] Hold released Controller=%s NetMode=%d PlayerId=%d WasHolding=%d Progress=%.2f"),
        *GetName(), static_cast<int32>(GetNetMode()),
        PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE,
        bWasHolding, ReleasedProgress);
    if (IsLocalController())
    {
        ServerCancelRetryGameRequest();
    }
}

bool ACMPlayerController::IsRetryVoteHoldActive() const
{
    return bRetryVoteHoldActive
        && CanRequestRetryGame()
        && GetRetryVoteHoldProgress() < 1.0f;
}

float ACMPlayerController::GetRetryVoteHoldProgress() const
{
    const UWorld* World = GetWorld();
    if (!bRetryVoteHoldActive || !World)
    {
        return 0.0f;
    }
    return FMath::Clamp(
        static_cast<float>(
            (World->GetTimeSeconds() - RetryVoteHoldStartTime)
            / RetryVoteHoldDuration),
        0.0f,
        1.0f);
}

bool ACMPlayerController::CanControlStageResult() const
{
    return IsLocalController()
        && (GetNetMode() == NM_Standalone || HasAuthority());
}

void ACMPlayerController::RequestRestartCompletedStage()
{
    if (IsLocalController())
    {
        ServerRequestRestartCompletedStage();
    }
}

void ACMPlayerController::RequestAdvanceCompletedStage()
{
    if (IsLocalController())
    {
        ServerRequestAdvanceCompletedStage();
    }
}

void ACMPlayerController::RequestCheatKillAllSegments()
{
    if (!IsLocalController())
    {
        return;
    }

    ServerCheatKillAllSegments();
}

void ACMPlayerController::RequestCheatRespawnAtCheckpoint()
{
    if (IsLocalController())
    {
        ServerCheatRespawnAtCheckpoint();
    }
}

void ACMPlayerController::RequestCheatNextStage()
{
#if !UE_BUILD_SHIPPING
    if (IsLocalController())
    {
        ServerCheatNextStage();
    }
#endif
}

void ACMPlayerController::ServerCheatNextStage_Implementation()
{
#if !UE_BUILD_SHIPPING
    ACMPlayGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMPlayGameMode>() : nullptr;
    if (!GameMode || !GameMode->TryCheatNextStage())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat Failed] CM.NextStage requires Playing/Completed/Failed and an active route with a next stage."));
    }
#endif
}

void ACMPlayerController::RequestCheatGoToStage(int32 OneBasedStageNumber)
{
#if !UE_BUILD_SHIPPING
    if (IsLocalController())
    {
        ServerCheatGoToStage(OneBasedStageNumber);
    }
#endif
}

void ACMPlayerController::ServerCheatGoToStage_Implementation(int32 OneBasedStageNumber)
{
#if !UE_BUILD_SHIPPING
    ACMPlayGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMPlayGameMode>() : nullptr;
    if (!GameMode || !GameMode->TryCheatGoToStage(OneBasedStageNumber))
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat Failed] CM.GoToStage %d: requires a valid route number and Playing/Completed/Failed phase."),
            OneBasedStageNumber);
    }
#endif
}

void ACMPlayerController::RequestCheatKillSegment(int32 SegmentIndex)
{
    if (!IsLocalController())
    {
        return;
    }

    ServerCheatKillSegment(SegmentIndex);
}

void ACMPlayerController::RequestCheatGoToCheckpoint(int32 OneBasedCheckpointNumber)
{
#if !UE_BUILD_SHIPPING
    if (IsLocalController() && OneBasedCheckpointNumber > 0)
    {
        ServerCheatGoToCheckpoint(OneBasedCheckpointNumber);
    }
#endif
}

void ACMPlayerController::ServerCheatGoToCheckpoint_Implementation(int32 OneBasedCheckpointNumber)
{
#if !UE_BUILD_SHIPPING
    ACMPlayGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMPlayGameMode>() : nullptr;
    if (!GameMode || !GameMode->TryCheatGoToCheckpoint(OneBasedCheckpointNumber))
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat Failed] CM.GoToCheckpoint %d: requires Playing, a valid one-based Rooms array number, one room controller and a unique checkpoint."),
            OneBasedCheckpointNumber);
    }
#endif
}

void ACMPlayerController::RequestCheatDamageSegment(
    int32 SegmentIndex,
    float Damage
)
{
    if (IsLocalController() && Damage > 0.0f)
    {
        ServerCheatDamageSegment(SegmentIndex, Damage);
    }
}

void ACMPlayerController::RequestCheatDamagePart(
    int32 OneBasedSlotIndex,
    float Damage
)
{
    if (IsLocalController() && Damage > 0.0f)
    {
        ServerCheatDamagePart(OneBasedSlotIndex, Damage);
    }
}

void ACMPlayerController::RequestCheatSetInvincible(bool bEnabled)
{
    if (IsLocalController())
    {
        ServerCheatSetInvincible(bEnabled);
    }
}

void ACMPlayerController::RequestCheatSpawnRandomParts()
{
    if (IsLocalController())
    {
        ServerCheatSpawnRandomParts();
    }
}

void ACMPlayerController::RequestCheatAttachPart(
    int32 OneBasedSlotIndex,
    FName PartName
)
{
    if (IsLocalController())
    {
        ServerCheatAttachPart(OneBasedSlotIndex, PartName);
    }
}

void ACMPlayerController::RequestCheatFillAllSlotsWithPart(FName PartName)
{
    if (IsLocalController())
    {
        ServerCheatFillAllSlotsWithPart(PartName);
    }
}

void ACMPlayerController::RequestCheatClearRandomParts()
{
    if (IsLocalController())
    {
        ServerCheatClearRandomParts();
    }
}

void ACMPlayerController::RequestCheatSpawnLegParts()
{
    if (IsLocalController())
    {
        ServerCheatSpawnLegParts();
    }
}

void ACMPlayerController::RequestCheatClearLegParts()
{
    if (IsLocalController())
    {
        ServerCheatClearLegParts();
    }
}

// 콘솔 명령이 화살표 디버그 이동을 켜거나 끄는 진입점
void ACMPlayerController::SetCheatDebugMovementEnabled(bool bEnabled)
{
    if (!IsLocalController())
    {
        return;
    }

    bCheatDebugMovementEnabled = bEnabled;
    if (!bEnabled)
    {
        bDebugMoveForwardHeld = false;
        bDebugMoveBackwardHeld = false;
        bDebugTurnLeftHeld = false;
        bDebugTurnRightHeld = false;
    }

    UE_LOG(LogChimeraPlayerController, Warning,
        TEXT("[Cheat] Arrow-key debug movement %s for %s."),
        bEnabled ? TEXT("enabled") : TEXT("disabled"),
        *GetName());
}

// 공용 키메라를 독점 이동할 수 있는 로컬 호스트인지 확인
bool ACMPlayerController::CanControlTestAreas() const
{
    return IsLocalController()
        && HasAuthority()
        && (GetNetMode() == NM_ListenServer || GetNetMode() == NM_Standalone)
        && IsValid(FindTestAreaManager());
}

// Test Area Manager가 수집한 UI 목록 반환
TArray<FCMTestAreaInfo> ACMPlayerController::GetAvailableTestAreas() const
{
    if (const ACMTestAreaManager* Manager = FindTestAreaManager())
    {
        return Manager->GetAvailableAreas();
    }
    return {};
}

// 로컬 UI 선택을 서버의 호스트 검증 RPC로 전달
void ACMPlayerController::RequestTeleportToTestArea(FName AreaId)
{
    if (IsLocalController() && !AreaId.IsNone())
    {
        ServerRequestTeleportToTestArea(AreaId);
    }
}

// 현재 월드의 유일한 TestAreaManager 검색
ACMTestAreaManager* ACMPlayerController::FindTestAreaManager() const
{
    for (TActorIterator<ACMTestAreaManager> It(GetWorld()); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

// 원격 참가자의 공용 몸통 이동을 거부하고 호스트 요청만 처리
void ACMPlayerController::ServerRequestTeleportToTestArea_Implementation(FName AreaId)
{
    if (!IsLocalController()
        || (GetNetMode() != NM_ListenServer && GetNetMode() != NM_Standalone))
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("Test Area 이동 요청을 거부했습니다. Controller=%s AreaId=%s"),
            *GetName(), *AreaId.ToString());
        return;
    }

    if (ACMTestAreaManager* Manager = FindTestAreaManager())
    {
        Manager->TeleportToArea(AreaId);
    }
}

void ACMPlayerController::ServerRequestRetryGame_Implementation()
{
    ACMGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMGameMode>()
        : nullptr;
    const bool bAccepted = GameMode && GameMode->TryRetryGame(this);
    UE_LOG(LogChimeraPlayerController, Display,
        TEXT("[RetryVote][ServerRPC] Hold request Controller=%s PlayerId=%d GameMode=%s Accepted=%d"),
        *GetName(), PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE,
        *GetNameSafe(GameMode), bAccepted);
}

void ACMPlayerController::ServerCancelRetryGameRequest_Implementation()
{
    UE_LOG(LogChimeraPlayerController, Display,
        TEXT("[RetryVote][ServerRPC] Hold cancel Controller=%s PlayerId=%d"),
        *GetName(), PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE);
    if (ACMPlayGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMPlayGameMode>() : nullptr)
    {
        GameMode->CancelRetryVoteHold(this);
    }
}

void ACMPlayerController::ServerRequestRestartCompletedStage_Implementation()
{
    if (ACMPlayGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMPlayGameMode>()
        : nullptr)
    {
        GameMode->TryRestartCompletedStage(this);
    }
}

void ACMPlayerController::ServerRequestAdvanceCompletedStage_Implementation()
{
    if (ACMPlayGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMPlayGameMode>()
        : nullptr)
    {
        GameMode->TryAdvanceCompletedStage(this);
    }
}

void ACMPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);

    // 서버에서 Shared Chimera를 만들거나 클라이언트가 그 참조를 복제받으면
    // GameState가 이 이벤트를 한 번 발생시킨다. 매 프레임 포인터를 찾지 않는다.
    if (ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr)
    {
        GameState->OnSharedChimeraChanged.AddUniqueDynamic(
            this,
            &ACMPlayerController::HandleSharedChimeraChanged
        );
    }
    HandleSharedChimeraChanged();

    if (!DefaultMappingContext)
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("DefaultMappingContext is not assigned on %s."),
            *GetName());
        return;
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer)
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
            LocalPlayer
        );

    if (InputSubsystem)
    {
        InputSubsystem->AddMappingContext(
            DefaultMappingContext,
            MappingPriority
        );
    }
}

void ACMPlayerController::ServerCheatKillAllSegments_Implementation()
{
    ACMChimera* SharedChimera = GetSharedChimera();
    if (!SharedChimera)
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat Failed] CM.AllDead could not find SharedChimera."));
        return;
    }

    UE_LOG(LogChimeraPlayerController, Warning,
        TEXT("[Cheat] CM.AllDead requested by %s. Killing every living segment."),
        *GetName());

    const TArray<FCMBodySegmentHealthState> SegmentStates =
        SharedChimera->GetSegmentHealthStates();
    for (const FCMBodySegmentHealthState& SegmentState : SegmentStates)
    {
        if (!SegmentState.bDead)
        {
            SharedChimera->ApplyDamageToSegment(
                SegmentState.SegmentIndex,
                SegmentState.Health
            );
        }
    }
}

void ACMPlayerController::ServerCheatRespawnAtCheckpoint_Implementation()
{
    ACMPlayGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMPlayGameMode>()
        : nullptr;
    if (!GameMode || !GameMode->TryCheatRespawnAtLatestCheckpoint())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat Failed] CM.Checkpoint could not restore the latest checkpoint."));
        return;
    }

    UE_LOG(LogChimeraPlayerController, Warning,
        TEXT("[Cheat] CM.Checkpoint restored the latest checkpoint."));
}

void ACMPlayerController::ServerCheatKillSegment_Implementation(
    int32 SegmentIndex
)
{
    ACMChimera* SharedChimera = GetSharedChimera();
    if (!SharedChimera)
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat Failed] Body%dDead could not find SharedChimera."),
            SegmentIndex);
        return;
    }

    const TArray<FCMBodySegmentHealthState> SegmentStates =
        SharedChimera->GetSegmentHealthStates();
    if (!SegmentStates.IsValidIndex(SegmentIndex))
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat Failed] Segment index %d does not exist. ActiveSegmentCount=%d"),
            SegmentIndex,
            SegmentStates.Num());
        return;
    }

    UE_LOG(LogChimeraPlayerController, Warning,
        TEXT("[Cheat] CM.Body%dDead requested by %s."),
        SegmentIndex,
        *GetName());
    SharedChimera->ApplyDamageToSegment(
        SegmentIndex,
        SegmentStates[SegmentIndex].Health
    );
}

void ACMPlayerController::ServerCheatSpawnRandomParts_Implementation()
{
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat] CM.RandomParts requested by %s."),
            *GetName());
        SharedChimera->SpawnRandomDebugParts();
    }
}

void ACMPlayerController::ServerCheatAttachPart_Implementation(
    int32 OneBasedSlotIndex,
    FName PartName
)
{
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat] CM.AttachPart Slot=%d Part=%s requested by %s."),
            OneBasedSlotIndex,
            *PartName.ToString(),
            *GetName());
        SharedChimera->SpawnDebugPartAtSlot(
            OneBasedSlotIndex - 1,
            PartName
        );
    }
}

void ACMPlayerController::ServerCheatFillAllSlotsWithPart_Implementation(
    FName PartName
)
{
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat] Fill every active slot with Part=%s requested by %s."),
            *PartName.ToString(),
            *GetName());
        SharedChimera->FillAllDebugSlotsWithPart(PartName);
    }
}

void ACMPlayerController::ServerCheatClearRandomParts_Implementation()
{
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat] CM.ClearRandomParts requested by %s."),
            *GetName());
        SharedChimera->ClearRandomDebugParts();
    }
}

// 화살표 입력을 서버 공용 키메라의 개발용 물리 이동으로 전달
void ACMPlayerController::ServerApplyCheatDebugMovement_Implementation(
    float ForwardInput,
    float TurnInput)
{
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        SharedChimera->ApplyDebugMovementInput(
            FMath::Clamp(ForwardInput, -1.0f, 1.0f),
            FMath::Clamp(TurnInput, -1.0f, 1.0f));
    }
}

void ACMPlayerController::ServerCheatSpawnLegParts_Implementation()
{
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat] CM.LegParts requested by %s."),
            *GetName());
        SharedChimera->SpawnTestLegParts();
    }
}

void ACMPlayerController::ServerCheatClearLegParts_Implementation()
{
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat] CM.ClearLegParts requested by %s."),
            *GetName());
        SharedChimera->ClearTestLegParts();
    }
}

void ACMPlayerController::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    CancelPingSelection();

    if (ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr)
    {
        GameState->OnSharedChimeraChanged.RemoveDynamic(
            this,
            &ACMPlayerController::HandleSharedChimeraChanged
        );
    }

    Super::EndPlay(EndPlayReason);
}

void ACMPlayerController::ServerCheatDamageSegment_Implementation(
    int32 SegmentIndex,
    float Damage
)
{
    ACMChimera* SharedChimera = GetSharedChimera();
    const TArray<FCMBodySegmentHealthState> SegmentStates = SharedChimera
        ? SharedChimera->GetSegmentHealthStates()
        : TArray<FCMBodySegmentHealthState>();
    if (!SharedChimera
        || !SegmentStates.IsValidIndex(SegmentIndex)
        || Damage <= 0.0f)
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat Failed] CM.DamageBody Index=%d Damage=%.1f"),
            SegmentIndex,
            Damage);
        return;
    }

    SharedChimera->ApplyDamageToSegment(SegmentIndex, Damage);
    UE_LOG(LogChimeraPlayerController, Warning,
        TEXT("[Cheat] CM.DamageBody Index=%d Damage=%.1f requested by %s."),
        SegmentIndex,
        Damage,
        *GetName());
}

void ACMPlayerController::ServerCheatDamagePart_Implementation(
    int32 OneBasedSlotIndex,
    float Damage
)
{
    ACMChimera* SharedChimera = GetSharedChimera();
    const int32 ActiveSlotCount = SharedChimera
        ? SharedChimera->GetActiveSegmentCount()
            * CMControl::PartSlotsPerSegment
        : 0;
    const int32 FlatSlotIndex = OneBasedSlotIndex - 1;
    const FCMPartSlotAddress SlotAddress =
        CMControl::FromFlatPartSlotIndex(FlatSlotIndex);
    UCMPartSlotComponent* PartSlot = SharedChimera
        && FlatSlotIndex >= 0
        && FlatSlotIndex < ActiveSlotCount
        ? SharedChimera->GetPartSlotComponent(SlotAddress)
        : nullptr;
    ACMPartActorBase* Part = PartSlot
        ? Cast<ACMPartActorBase>(PartSlot->GetAttachedPart())
        : nullptr;
    if (!Part || Damage <= 0.0f)
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat Failed] CM.DamagePart Slot=%d Damage=%.1f ActiveSlots=%d. Slot is invalid or has no attached Part."),
            OneBasedSlotIndex,
            Damage,
            ActiveSlotCount);
        return;
    }

    Part->ApplyPartDamage(Damage);
    UE_LOG(LogChimeraPlayerController, Warning,
        TEXT("[Cheat] CM.DamagePart Slot=%d Part=%s Damage=%.1f requested by %s."),
        OneBasedSlotIndex,
        *Part->GetName(),
        Damage,
        *GetName());
}

void ACMPlayerController::ServerCheatSetInvincible_Implementation(
    bool bEnabled)
{
    ACMChimera* SharedChimera = GetSharedChimera();
    if (!SharedChimera)
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat Failed] CM.God: shared Chimera is unavailable."));
        return;
    }

    SharedChimera->SetCanBeDamaged(!bEnabled);
    SharedChimera->ForceNetUpdate();
    UE_LOG(LogChimeraPlayerController, Warning,
        TEXT("[Cheat] CM.God %d requested by %s."),
        bEnabled ? 1 : 0,
        *GetName());
}

void ACMPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInputComponent =
        Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInputComponent)
    {
        UE_LOG(LogChimeraPlayerController, Error,
            TEXT("CMPlayerController requires EnhancedInputComponent."));
        return;
    }

    if (!FirstControlAction
        || !SecondControlAction
        || !ThirdControlAction
        || !FourthControlAction)
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("One or more Q/W/E/R InputActions are not assigned on %s."),
            *GetName());
    }

    const auto BindControlAction = [this, EnhancedInputComponent](
        const UInputAction* Action,
        void (ACMPlayerController::*PressedFunction)(),
        void (ACMPlayerController::*ReleasedFunction)())
    {
        if (!Action)
        {
            return;
        }

        EnhancedInputComponent->BindAction(
            Action, ETriggerEvent::Started, this, PressedFunction);
        EnhancedInputComponent->BindAction(
            Action, ETriggerEvent::Completed, this, ReleasedFunction);
        EnhancedInputComponent->BindAction(
            Action, ETriggerEvent::Canceled, this, ReleasedFunction);
    };

    BindControlAction(FirstControlAction,
        &ACMPlayerController::FirstControlKeyPressed,
        &ACMPlayerController::FirstControlKeyReleased);
    BindControlAction(SecondControlAction,
        &ACMPlayerController::SecondControlKeyPressed,
        &ACMPlayerController::SecondControlKeyReleased);
    BindControlAction(ThirdControlAction,
        &ACMPlayerController::ThirdControlKeyPressed,
        &ACMPlayerController::ThirdControlKeyReleased);
    BindControlAction(FourthControlAction,
        &ACMPlayerController::FourthControlKeyPressed,
        &ACMPlayerController::FourthControlKeyReleased);

    for (const FKey Key : SoloTestControlKeys)
    {
        InputComponent->BindKey(
            Key,
            IE_Pressed,
            this,
            &ThisClass::SoloControlKeyPressed);
        InputComponent->BindKey(
            Key,
            IE_Released,
            this,
            &ThisClass::SoloControlKeyReleased);
    }

    if (DetachModifierAction)
    {
        EnhancedInputComponent->BindAction(
            DetachModifierAction,
            ETriggerEvent::Started,
            this,
            &ACMPlayerController::DetachModifierPressed
        );
        EnhancedInputComponent->BindAction(
            DetachModifierAction,
            ETriggerEvent::Completed,
            this,
            &ACMPlayerController::DetachModifierReleased
        );
        EnhancedInputComponent->BindAction(
            DetachModifierAction,
            ETriggerEvent::Canceled,
            this,
            &ACMPlayerController::DetachModifierReleased
        );
    }
    else
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("DetachModifierAction is not assigned on %s."),
            *GetName());
    }

    if (ReverseModifierAction)
    {
        EnhancedInputComponent->BindAction(
            ReverseModifierAction,
            ETriggerEvent::Started,
            this,
            &ACMPlayerController::ReverseModifierPressed
        );
        EnhancedInputComponent->BindAction(
            ReverseModifierAction,
            ETriggerEvent::Completed,
            this,
            &ACMPlayerController::ReverseModifierReleased
        );
        EnhancedInputComponent->BindAction(
            ReverseModifierAction,
            ETriggerEvent::Canceled,
            this,
            &ACMPlayerController::ReverseModifierReleased
        );
    }
    else
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("ReverseModifierAction is not assigned on %s."),
            *GetName());
    }

    if (CameraDistanceAction)
    {
        EnhancedInputComponent->BindAction(
            CameraDistanceAction,
            ETriggerEvent::Triggered,
            this,
            &ACMPlayerController::AdjustCameraDistance
        );
    }
    else
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("CameraDistanceAction is not assigned on %s."),
            *GetName());
    }
    
    if (RetryVoteAction)
    {
        EnhancedInputComponent->BindAction(
            RetryVoteAction,
            ETriggerEvent::Started,
            this,
            &ThisClass::RetryVotePressed
        );

        EnhancedInputComponent->BindAction(
            RetryVoteAction,
            ETriggerEvent::Completed,
            this,
            &ThisClass::RetryVoteReleased
        );

        EnhancedInputComponent->BindAction(
            RetryVoteAction,
            ETriggerEvent::Canceled,
            this,
            &ThisClass::RetryVoteReleased
        );
    }

    // 기존 Q/W/E/R Mapping Context와 분리된 개발 전용 화살표 입력
    InputComponent->BindKey(EKeys::Up, IE_Pressed,
        this, &ThisClass::DebugMoveForwardPressed);
    InputComponent->BindKey(EKeys::Up, IE_Released,
        this, &ThisClass::DebugMoveForwardReleased);
    InputComponent->BindKey(EKeys::Down, IE_Pressed,
        this, &ThisClass::DebugMoveBackwardPressed);
    InputComponent->BindKey(EKeys::Down, IE_Released,
        this, &ThisClass::DebugMoveBackwardReleased);
    InputComponent->BindKey(EKeys::Left, IE_Pressed,
        this, &ThisClass::DebugTurnLeftPressed);
    InputComponent->BindKey(EKeys::Left, IE_Released,
        this, &ThisClass::DebugTurnLeftReleased);
    InputComponent->BindKey(EKeys::Right, IE_Pressed,
        this, &ThisClass::DebugTurnRightPressed);
    InputComponent->BindKey(EKeys::Right, IE_Released,
        this, &ThisClass::DebugTurnRightReleased);

    InputComponent->BindKey(EKeys::LeftAlt, IE_Pressed,
        this, &ThisClass::PingModifierPressed);
    InputComponent->BindKey(EKeys::LeftAlt, IE_Released,
        this, &ThisClass::PingModifierReleased);
    InputComponent->BindKey(EKeys::RightAlt, IE_Pressed,
        this, &ThisClass::PingModifierPressed);
    InputComponent->BindKey(EKeys::RightAlt, IE_Released,
        this, &ThisClass::PingModifierReleased);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed,
        this, &ThisClass::PingMousePressed);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released,
        this, &ThisClass::PingMouseReleased);
}

void ACMPlayerController::RetryVotePressed()
{
    RecordApmAction();
    RequestRetryGame();
}

void ACMPlayerController::RetryVoteReleased()
{
    CancelRetryGameRequest();
}

// 활성화된 로컬 화살표 상태를 서버에 낮은 신뢰도의 연속 입력으로 전달
void ACMPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    if (!IsLocalController())
    {
        return;
    }

    if (bPingSelecting && PingSelectorWidget)
    {
        float MouseX = 0.0f;
        float MouseY = 0.0f;
        if (GetMousePosition(MouseX, MouseY))
        {
            PingSelectorWidget->UpdateSelection(
                FVector2D(MouseX, MouseY) - PingDragStart);
        }
    }

    if (!bCheatDebugMovementEnabled)
    {
        return;
    }

    const float ForwardInput =
        static_cast<float>(bDebugMoveForwardHeld)
        - static_cast<float>(bDebugMoveBackwardHeld);
    const float TurnInput =
        static_cast<float>(bDebugTurnRightHeld)
        - static_cast<float>(bDebugTurnLeftHeld);
    if (!FMath::IsNearlyZero(ForwardInput)
        || !FMath::IsNearlyZero(TurnInput))
    {
        ServerApplyCheatDebugMovement(ForwardInput, TurnInput);
    }
}

void ACMPlayerController::PingModifierPressed()
{
    bPingModifierHeld = IsLocalController();
}

void ACMPlayerController::PingModifierReleased()
{
    bPingModifierHeld = IsInputKeyDown(EKeys::LeftAlt)
        || IsInputKeyDown(EKeys::RightAlt);
    if (!bPingModifierHeld)
    {
        CancelPingSelection();
    }
}

void ACMPlayerController::PingMousePressed()
{
    const ACMPlayerState* CMPlayerState = GetPlayerState<ACMPlayerState>();
    if (!bPingModifierHeld || bPingSelecting || !IsLocalController()
        || !CMPlayerState || CMPlayerState->IsOnlyASpectator()
        || CMPlayerState->GetParticipationState()
            != ECMPlayerParticipationState::Active)
    {
        return;
    }

    float MouseX = 0.0f;
    float MouseY = 0.0f;
    if (!GetMousePosition(MouseX, MouseY)
        || !CapturePingTrace(PingTraceOrigin, PingTraceDirection))
    {
        return;
    }

    PingDragStart = FVector2D(MouseX, MouseY);
    PingSelectorWidget = CreateWidget<UCMPingSelectorWidget>(this);
    if (!PingSelectorWidget)
    {
        return;
    }

    bPingSelecting = true;
    PingSelectorWidget->AddToViewport(1000);
    PingSelectorWidget->BeginSelection(PingDragStart);
}

void ACMPlayerController::PingMouseReleased()
{
    if (!bPingSelecting)
    {
        return;
    }

    ECMPingType SelectedType = ECMPingType::GoHere;
    const bool bShouldPing = PingSelectorWidget
        && PingSelectorWidget->GetSelectedType(SelectedType);
    CancelPingSelection();
    if (bShouldPing)
    {
        ServerRequestPing(
            SelectedType, PingTraceOrigin, PingTraceDirection);
    }
}

void ACMPlayerController::CancelPingSelection()
{
    bPingSelecting = false;
    if (PingSelectorWidget)
    {
        PingSelectorWidget->RemoveFromParent();
        PingSelectorWidget = nullptr;
    }
}

bool ACMPlayerController::CapturePingTrace(
    FVector& OutOrigin,
    FVector& OutDirection) const
{
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    return GetMousePosition(MouseX, MouseY)
        && DeprojectScreenPositionToWorld(
            MouseX, MouseY, OutOrigin, OutDirection);
}

void ACMPlayerController::ServerRequestPing_Implementation(
    ECMPingType Type,
    FVector_NetQuantize TraceOrigin,
    FVector_NetQuantizeNormal TraceDirection)
{
    ACMPlayerState* CMPlayerState = GetPlayerState<ACMPlayerState>();
    ACMChimera* SharedChimera = GetSharedChimera();
    const uint8 TypeValue = static_cast<uint8>(Type);
    const FVector Direction = FVector(TraceDirection).GetSafeNormal();
    if (!CMPlayerState || !SharedChimera
        || CMPlayerState->IsOnlyASpectator()
        || CMPlayerState->GetParticipationState()
            != ECMPlayerParticipationState::Active
        || TypeValue > static_cast<uint8>(ECMPingType::SwapParts)
        || FVector(TraceOrigin).ContainsNaN()
        || !Direction.IsNormalized()
        || FVector::DistSquared(TraceOrigin, SharedChimera->GetActorLocation())
            > FMath::Square(CMPing::MaxTraceOriginDistanceFromChimera))
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Ping][Server] Rejected request from %s."),
            *GetNameSafe(CMPlayerState));
        return;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMWorldPing), true);
    QueryParams.AddIgnoredActor(GetPawn());
    QueryParams.AddIgnoredActor(SharedChimera);
    FHitResult Hit;
    const FVector TraceEnd = FVector(TraceOrigin)
        + Direction * CMPing::MaxTraceDistance;
    if (!GetWorld()->LineTraceSingleByChannel(
            Hit, TraceOrigin, TraceEnd, ECC_Visibility, QueryParams))
    {
        return;
    }

    ACMWorldPing::EnforceServerLimit(*GetWorld());
    const FVector SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
    const FVector PingLocation = Hit.ImpactPoint + SurfaceNormal * 2.0f;
    const FRotator SurfaceRotation = FRotationMatrix::MakeFromZ(
        SurfaceNormal).Rotator();
    FTransform SpawnTransform(SurfaceRotation, PingLocation);
    ACMWorldPing* Ping = GetWorld()->SpawnActorDeferred<ACMWorldPing>(
        ACMWorldPing::StaticClass(),
        SpawnTransform,
        nullptr,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Ping)
    {
        return;
    }

    Ping->InitializePing(
        Type,
        CMPlayerState->GetPlayerName(),
        CMPlayerState->GetPlayerColor());
    Ping->FinishSpawning(SpawnTransform);
    Ping->ForceNetUpdate();
    UE_LOG(LogChimeraPlayerController, Display,
        TEXT("[Ping][Server] Spawned Type=%d Player=%s Location=%s Lifetime=%.1f ActiveLimit=%d"),
        TypeValue,
        *CMPlayerState->GetPlayerName(),
        *PingLocation.ToCompactString(),
        CMPing::DisplayDuration,
        CMPing::MaxActivePings);
}

void ACMPlayerController::HandleSharedChimeraChanged()
{
    if (!IsLocalController())
    {
        return;
    }

    ACMChimera* SharedChimera = GetSharedChimera();
    if (!SharedChimera || CachedSharedChimera == SharedChimera)
    {
        return;
    }

    CachedSharedChimera = SharedChimera;
    SetViewTarget(SharedChimera);

    UE_LOG(LogChimeraPlayerController, Log,
        TEXT("[Camera Event] Controller=%s now views SharedChimera=%s"),
        *GetName(),
        *GetNameSafe(SharedChimera));
}

// 로컬 로비 UI의 시작 요청을 서버 RPC로 전달
void ACMPlayerController::RequestStartStageRoute()
{
    if (IsLocalController())
    {
        ServerRequestStartStageRoute();
    }
}

// 로컬 로비 UI의 테스트 시작 요청을 서버 RPC로 전달
void ACMPlayerController::RequestStartTestStageRoute()
{
    if (IsLocalController())
    {
        ServerRequestStartTestStageRoute();
    }
}

// 로컬 Ready UI 입력을 서버 로비 정책으로 전달
void ACMPlayerController::RequestSetReady(bool bReady)
{
    if (IsLocalController())
    {
        ServerSetReady(bReady);
    }
}

// 서버 LobbyGameMode가 준비 상태와 캠페인 설정을 최종 검증
void ACMPlayerController::ServerRequestStartStageRoute_Implementation()
{
    ACMLobbyGameMode* LobbyGameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMLobbyGameMode>() : nullptr;
    if (LobbyGameMode)
    {
        LobbyGameMode->TryStartStageRoute(this);
    }
}

// 서버 LobbyGameMode가 준비 상태와 TestRoute 설정을 최종 검증
void ACMPlayerController::ServerRequestStartTestStageRoute_Implementation()
{
    ACMLobbyGameMode* LobbyGameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMLobbyGameMode>() : nullptr;
    if (LobbyGameMode)
    {
        LobbyGameMode->TryStartTestStageRoute(this);
    }
}

// 서버 LobbyGameMode가 요청자와 현재 로비 Phase를 검증
void ACMPlayerController::ServerSetReady_Implementation(bool bReady)
{
    ACMLobbyGameMode* LobbyGameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMLobbyGameMode>() : nullptr;
    if (LobbyGameMode)
    {
        LobbyGameMode->TrySetPlayerReady(this, bReady);
    }
}

// 로컬 로드 컴포넌트 결과를 소유 Controller의 서버 RPC로 전달
void ACMPlayerController::ReportLocalStageLoadComplete(FGuid RequestId, bool bSucceeded)
{
    if (IsLocalController())
    {
        ServerReportStageLoadComplete(RequestId, bSucceeded);
    }
}

// 보고한 Controller를 서버 권한 로드 배리어에 전달
void ACMPlayerController::ServerReportStageLoadComplete_Implementation(
    FGuid RequestId, bool bSucceeded)
{
    ACMPlayGameMode* PlayGameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMPlayGameMode>() : nullptr;
    if (PlayGameMode)
    {
        PlayGameMode->HandleStageLoadComplete(this, RequestId, bSucceeded);
    }
}

void ACMPlayerController::FirstControlKeyPressed()
{
    SetControlSlotPressed(0, true);
}

void ACMPlayerController::SecondControlKeyPressed()
{
    SetControlSlotPressed(1, true);
}

void ACMPlayerController::ThirdControlKeyPressed()
{
    SetControlSlotPressed(2, true);
}

void ACMPlayerController::FourthControlKeyPressed()
{
    SetControlSlotPressed(3, true);
}

void ACMPlayerController::FirstControlKeyReleased()
{
    SetControlSlotPressed(0, false);
}

void ACMPlayerController::SecondControlKeyReleased()
{
    SetControlSlotPressed(1, false);
}

void ACMPlayerController::ThirdControlKeyReleased()
{
    SetControlSlotPressed(2, false);
}

void ACMPlayerController::FourthControlKeyReleased()
{
    SetControlSlotPressed(3, false);
}

void ACMPlayerController::DetachModifierPressed()
{
    RecordApmAction();
    bDetachModifierHeld = true;
}

void ACMPlayerController::DetachModifierReleased()
{
    bDetachModifierHeld = false;
}

void ACMPlayerController::ReverseModifierPressed()
{
    RecordApmAction();
    bReverseModifierHeld = true;
}

void ACMPlayerController::ReverseModifierReleased()
{
    bReverseModifierHeld = false;
}

void ACMPlayerController::AdjustCameraDistance(
    const FInputActionValue& InputValue
)
{
    if (!IsLocalController())
    {
        return;
    }

    const float WheelInput = InputValue.Get<float>();
    ACMChimera* SharedChimera = GetSharedChimera();
    if (!SharedChimera || FMath::IsNearlyZero(WheelInput))
    {
        return;
    }

    const float NewDistance =
        SharedChimera->AdjustLocalCameraDistance(WheelInput);
    UE_LOG(LogChimeraPlayerController, Verbose,
        TEXT("[Camera Distance] Controller=%s Input=%.2f Distance=%.1f"),
        *GetName(),
        WheelInput,
        NewDistance);
}

void ACMPlayerController::DebugMoveForwardPressed()
{
    bDebugMoveForwardHeld = true;
}

void ACMPlayerController::DebugMoveForwardReleased()
{
    bDebugMoveForwardHeld = false;
}

void ACMPlayerController::DebugMoveBackwardPressed()
{
    bDebugMoveBackwardHeld = true;
}

void ACMPlayerController::DebugMoveBackwardReleased()
{
    bDebugMoveBackwardHeld = false;
}

void ACMPlayerController::DebugTurnLeftPressed()
{
    bDebugTurnLeftHeld = true;
}

void ACMPlayerController::DebugTurnLeftReleased()
{
    bDebugTurnLeftHeld = false;
}

void ACMPlayerController::DebugTurnRightPressed()
{
    bDebugTurnRightHeld = true;
}

void ACMPlayerController::DebugTurnRightReleased()
{
    bDebugTurnRightHeld = false;
}

void ACMPlayerController::SetControlSlotPressed(
    int32 SlotIndex,
    bool bPressed
)
{
    if (!IsLocalController()
        || IsSoloTestMode()
        || SlotIndex < 0
        || SlotIndex >= CMControl::MaxKeysPerPlayer)
    {
        return;
    }

    if (bPressed)
    {
        RecordApmAction();
    }

    // Controller는 어떤 파츠가 배정됐는지 알지 않는다.
    // Possess 중인 ControlBody에 SlotIndex와 Press/Release만 전달한다.
    if (ACMControlBody* ControlBody = GetPawn<ACMControlBody>())
    {
        if (bPressed && VisionInputComponent)
        {
            VisionInputComponent->SetActiveControlSlot(
                ControlBody->ResolveControlInputSlot(SlotIndex));
        }

        const bool bConsumeModifierHeld =
            IsInputKeyDown(EKeys::LeftControl)
            || IsInputKeyDown(EKeys::RightControl);
        if (bPressed && bConsumeModifierHeld)
        {
            ControlBody->RequestConsumePartFromControlSlot(SlotIndex);
            return;
        }

        if (bPressed && bDetachModifierHeld)
        {
            ControlBody->RequestDetachPartFromControlSlot(SlotIndex);
            return;
        }
        // Reverse is sampled only when the control key starts. The server
        // stores the resulting direction in that Leg Step, so changing Space
        // while the Step is active cannot reverse an already-running action.
        ControlBody->SetControlSlotPressed(
            SlotIndex,
            bPressed,
            bPressed && bReverseModifierHeld
        );
    }
}

void ACMPlayerController::SoloControlKeyPressed(FKey Key)
{
    SetSoloControlKeyPressed(Key, true);
}

void ACMPlayerController::SoloControlKeyReleased(FKey Key)
{
    SetSoloControlKeyPressed(Key, false);
}

void ACMPlayerController::SetSoloControlKeyPressed(
    FKey Key,
    bool bPressed)
{
    if (!IsLocalController() || !IsSoloTestMode())
    {
        return;
    }

    const int32 KeyIndex = FindSoloTestControlKeyIndex(Key);
    if (KeyIndex == INDEX_NONE)
    {
        return;
    }

    if (bPressed)
    {
        RecordApmAction();
    }

    if (bPressed && KeyIndex < CMControl::MaxKeysPerPlayer
        && VisionInputComponent)
    {
        VisionInputComponent->SetActiveControlSlot(KeyIndex);
    }

    ServerSetSoloControlKeyPressed(
        KeyIndex,
        bPressed,
        bPressed && bReverseModifierHeld,
        bPressed && bDetachModifierHeld);
}

void ACMPlayerController::RecordApmAction()
{
    const double CurrentTime = FPlatformTime::Seconds();
    if (!PrepareApmForCurrentStage(CurrentTime))
    {
        return;
    }

    PruneRecentApmActions(CurrentTime);
    RecentApmActionTimes.Add(CurrentTime);
}

int32 ACMPlayerController::GetCurrentApm()
{
    const double CurrentTime = FPlatformTime::Seconds();
    if (!PrepareApmForCurrentStage(CurrentTime))
    {
        return 0;
    }

    PruneRecentApmActions(CurrentTime);
    const double MeasurementSeconds = FMath::Clamp(
        CurrentTime - ApmMeasurementStartTime,
        1.0,
        ApmWindowSeconds);
    return FMath::RoundToInt(
        RecentApmActionTimes.Num() * 60.0 / MeasurementSeconds);
}

bool ACMPlayerController::PrepareApmForCurrentStage(double CurrentTime)
{
    ACMPlayGameState* PlayState = GetWorld()
        ? GetWorld()->GetGameState<ACMPlayGameState>()
        : nullptr;
    if (!IsLocalController()
        || !PlayState
        || PlayState->GetPlayPhase() != ECMPlayPhase::Playing)
    {
        ApmTrackedPlayState.Reset();
        RecentApmActionTimes.Reset();
        ApmMeasurementStartTime = 0.0;
        ApmTrackedStageIndex = INDEX_NONE;
        return false;
    }

    if (ApmTrackedPlayState.Get() != PlayState
        || ApmTrackedStageIndex != PlayState->GetCurrentStageIndex())
    {
        ApmTrackedPlayState = PlayState;
        RecentApmActionTimes.Reset();
        ApmMeasurementStartTime = CurrentTime;
        ApmTrackedStageIndex = PlayState->GetCurrentStageIndex();
    }
    return true;
}

void ACMPlayerController::PruneRecentApmActions(double CurrentTime)
{
    const double OldestAllowedTime = CurrentTime - ApmWindowSeconds;
    int32 ExpiredCount = 0;
    while (ExpiredCount < RecentApmActionTimes.Num()
        && RecentApmActionTimes[ExpiredCount] < OldestAllowedTime)
    {
        ++ExpiredCount;
    }
    if (ExpiredCount > 0)
    {
        RecentApmActionTimes.RemoveAt(
            0, ExpiredCount, EAllowShrinking::No);
    }
}

void ACMPlayerController::ServerSetSoloControlKeyPressed_Implementation(
    int32 KeyIndex,
    bool bPressed,
    bool bReverseMovement,
    bool bDetachPart)
{
    if (!IsSoloTestMode()
        || KeyIndex < 0
        || KeyIndex >= CMControl::SoloTestKeyCount)
    {
        return;
    }

    ACMChimera* SharedChimera = GetSharedChimera();
    ACMPlayerState* CMPlayerState = GetPlayerState<ACMPlayerState>();
    if (!SharedChimera
        || !CMPlayerState
        || SharedChimera->GetActiveSegmentCount()
            != CMControl::SoloTestSegmentCount)
    {
        return;
    }

    FCMPartSlotAddress& PressedPartSlot =
        SoloPressedPartSlots[KeyIndex];
    if (!bPressed)
    {
        if (CMControl::IsValidPartSlot(
            PressedPartSlot,
            CMControl::SoloTestSegmentCount))
        {
            const FCMPartSlotAddress ReleasedPartSlot = PressedPartSlot;
            UCMPartSlotComponent* ReleasedSlot =
                SharedChimera->GetPartSlotComponent(ReleasedPartSlot);
            const bool bIsLeg = ReleasedSlot
                && Cast<ACMLegPart>(ReleasedSlot->GetAttachedPart());
            const bool bActivateOnRelease = bIsLeg ||
                SharedChimera->ShouldActivateBasicArmOnRelease(
                    ReleasedPartSlot);
            SharedChimera->SetPartSlotPressed(ReleasedPartSlot, false);
            if (bActivateOnRelease)
            {
                const double CurrentTime = GetWorld()
                    ? GetWorld()->GetTimeSeconds()
                    : SoloControlKeyStartTimes[KeyIndex];
                const float HoldSeconds = static_cast<float>(FMath::Max(
                    CurrentTime - SoloControlKeyStartTimes[KeyIndex],
                    0.0
                ));
                const float LegStrengthMultiplier = bIsLeg
                    ? SharedChimera->GetLegInputStrengthMultiplier(
                        HoldSeconds)
                    : 1.0f;
                SharedChimera->ActivatePartSlotWithLegStrength(
                    ReleasedPartSlot,
                    CMPlayerState,
                    bSoloControlKeyReverseMovement[KeyIndex],
                    LegStrengthMultiplier);
            }
        }
        PressedPartSlot = FCMPartSlotAddress();
        SoloControlKeyStartTimes[KeyIndex] = 0.0;
        bSoloControlKeyReverseMovement[KeyIndex] = false;
        return;
    }

    const FCMPartSlotAddress PartSlotAddress =
        CMControl::GetSoloTestPartSlotAddress(KeyIndex);
    if (bDetachPart)
    {
        if (CMControl::IsValidPartSlot(PressedPartSlot))
        {
            SharedChimera->SetPartSlotPressed(PressedPartSlot, false);
            PressedPartSlot = FCMPartSlotAddress();
        }
        SoloControlKeyStartTimes[KeyIndex] = 0.0;
        bSoloControlKeyReverseMovement[KeyIndex] = false;
        SharedChimera->DetachPartFromSlot(PartSlotAddress);
        return;
    }

    UCMPartSlotComponent* PartSlot =
        SharedChimera->GetPartSlotComponent(PartSlotAddress);
    if (PartSlot
        && !PartSlot->HasAttachedPart()
        && SharedChimera->TryBeginTentaclePartAttachment(
            PartSlotAddress))
    {
        if (CMControl::IsValidPartSlot(PressedPartSlot))
        {
            SharedChimera->SetPartSlotPressed(
                PressedPartSlot, false);
        }
        PressedPartSlot = FCMPartSlotAddress();
        SoloControlKeyStartTimes[KeyIndex] = 0.0;
        bSoloControlKeyReverseMovement[KeyIndex] = false;
        return;
    }

    if (CMControl::IsValidPartSlot(PressedPartSlot)
        && PressedPartSlot != PartSlotAddress)
    {
        SharedChimera->SetPartSlotPressed(PressedPartSlot, false);
    }

    PressedPartSlot = PartSlotAddress;
    SoloControlKeyStartTimes[KeyIndex] = GetWorld()
        ? GetWorld()->GetTimeSeconds()
        : 0.0;
    bSoloControlKeyReverseMovement[KeyIndex] = bReverseMovement;
    SharedChimera->SetPartSlotPressed(PartSlotAddress, true);
    const bool bIsLeg = PartSlot
        && Cast<ACMLegPart>(PartSlot->GetAttachedPart());
    if (!bIsLeg
        && !SharedChimera->IsBasicArmPartSlot(PartSlotAddress))
    {
        SharedChimera->ActivatePartSlot(
            PartSlotAddress,
            CMPlayerState,
            bReverseMovement);
    }
}

bool ACMPlayerController::IsSoloTestMode() const
{
    const ACMGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<ACMGameState>()
        : nullptr;
    return GameState && GameState->IsSoloTestMode();
}

ACMChimera*
ACMPlayerController::GetSharedChimera() const
{
    const ACMGameState* GameState =
        GetWorld() ? GetWorld()->GetGameState<ACMGameState>() : nullptr;
    return GameState ? GameState->SharedChimera : nullptr;
}
