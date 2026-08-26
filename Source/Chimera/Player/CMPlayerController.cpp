#include "CMPlayerController.h"

#include "Player/CMPlayerState.h"
#include "GameMode/CMGameState.h"
#include "GameMode/CMGameMode.h"
#include "GameMode/Play/CMPlayGameMode.h"
#include "GameMode/Lobby/CMLobbyGameMode.h"
#include "AsyncLoad/CMClientStageLoadComponent.h"
#include "Player/CMControlBody.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"
#include "Parts/Core/CMPartActorBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputCoreTypes.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "EngineUtils.h"
#include "Stage/Test/CMTestAreaManager.h"
#include "Vision/CMVisionInputComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPlayerController, Log, All);

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
}

bool ACMPlayerController::CanRequestRetryGame() const
{
    return IsLocalController()
        && HasAuthority()
        && GetNetMode() == NM_ListenServer
        && IsValid(GetSharedChimera());
}

void ACMPlayerController::RequestRetryGame()
{
    if (!IsLocalController())
    {
        return;
    }

    ServerRequestRetryGame();
}

void ACMPlayerController::RequestCheatKillAllSegments()
{
    if (!IsLocalController())
    {
        return;
    }

    ServerCheatKillAllSegments();
}

void ACMPlayerController::RequestCheatKillSegment(int32 SegmentIndex)
{
    if (!IsLocalController())
    {
        return;
    }

    ServerCheatKillSegment(SegmentIndex);
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

// Non-Shipping 콘솔 명령이 화살표 디버그 이동을 켜거나 끄는 진입점
void ACMPlayerController::SetCheatDebugMovementEnabled(bool bEnabled)
{
#if !UE_BUILD_SHIPPING
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
#endif
}

// 공용 키메라를 독점 이동할 수 있는 로컬 호스트인지 확인
bool ACMPlayerController::CanControlTestAreas() const
{
#if !UE_BUILD_SHIPPING
    return IsLocalController()
        && HasAuthority()
        && (GetNetMode() == NM_ListenServer || GetNetMode() == NM_Standalone)
        && IsValid(FindTestAreaManager());
#else
    return false;
#endif
}

// Test Area Manager가 수집한 UI 목록 반환
TArray<FCMTestAreaInfo> ACMPlayerController::GetAvailableTestAreas() const
{
#if !UE_BUILD_SHIPPING
    if (const ACMTestAreaManager* Manager = FindTestAreaManager())
    {
        return Manager->GetAvailableAreas();
    }
#endif
    return {};
}

// 로컬 UI 선택을 서버의 호스트 검증 RPC로 전달
void ACMPlayerController::RequestTeleportToTestArea(FName AreaId)
{
#if !UE_BUILD_SHIPPING
    if (IsLocalController() && !AreaId.IsNone())
    {
        ServerRequestTeleportToTestArea(AreaId);
    }
#endif
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
#if !UE_BUILD_SHIPPING
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
#endif
}

void ACMPlayerController::ServerRequestRetryGame_Implementation()
{
    ACMGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMGameMode>()
        : nullptr;
    if (GameMode)
    {
        GameMode->TryRetryGame(this);
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
#if !UE_BUILD_SHIPPING
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
#endif
}

void ACMPlayerController::ServerCheatKillSegment_Implementation(
    int32 SegmentIndex
)
{
#if !UE_BUILD_SHIPPING
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
#endif
}

void ACMPlayerController::ServerCheatSpawnRandomParts_Implementation()
{
#if !UE_BUILD_SHIPPING
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat] CM.RandomParts requested by %s."),
            *GetName());
        SharedChimera->SpawnRandomDebugParts();
    }
#endif
}

void ACMPlayerController::ServerCheatAttachPart_Implementation(
    int32 OneBasedSlotIndex,
    FName PartName
)
{
#if !UE_BUILD_SHIPPING
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
#endif
}

void ACMPlayerController::ServerCheatFillAllSlotsWithPart_Implementation(
    FName PartName
)
{
#if !UE_BUILD_SHIPPING
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat] Fill every active slot with Part=%s requested by %s."),
            *PartName.ToString(),
            *GetName());
        SharedChimera->FillAllDebugSlotsWithPart(PartName);
    }
#endif
}

void ACMPlayerController::ServerCheatClearRandomParts_Implementation()
{
#if !UE_BUILD_SHIPPING
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat] CM.ClearRandomParts requested by %s."),
            *GetName());
        SharedChimera->ClearRandomDebugParts();
    }
#endif
}

// 화살표 입력을 서버 공용 키메라의 개발용 물리 이동으로 전달
void ACMPlayerController::ServerApplyCheatDebugMovement_Implementation(
    float ForwardInput,
    float TurnInput)
{
#if !UE_BUILD_SHIPPING
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        SharedChimera->ApplyDebugMovementInput(
            FMath::Clamp(ForwardInput, -1.0f, 1.0f),
            FMath::Clamp(TurnInput, -1.0f, 1.0f));
    }
#endif
}

void ACMPlayerController::ServerCheatSpawnLegParts_Implementation()
{
#if !UE_BUILD_SHIPPING
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat] CM.LegParts requested by %s."),
            *GetName());
        SharedChimera->SpawnTestLegParts();
    }
#endif
}

void ACMPlayerController::ServerCheatClearLegParts_Implementation()
{
#if !UE_BUILD_SHIPPING
    if (ACMChimera* SharedChimera = GetSharedChimera())
    {
        UE_LOG(LogChimeraPlayerController, Warning,
            TEXT("[Cheat] CM.ClearLegParts requested by %s."),
            *GetName());
        SharedChimera->ClearTestLegParts();
    }
#endif
}

void ACMPlayerController::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
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
#if !UE_BUILD_SHIPPING
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
#endif
}

void ACMPlayerController::ServerCheatDamagePart_Implementation(
    int32 OneBasedSlotIndex,
    float Damage
)
{
#if !UE_BUILD_SHIPPING
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
#endif
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

#if !UE_BUILD_SHIPPING
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
#endif
}

// 활성화된 로컬 화살표 상태를 서버에 낮은 신뢰도의 연속 입력으로 전달
void ACMPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

#if !UE_BUILD_SHIPPING
    if (!IsLocalController() || !bCheatDebugMovementEnabled)
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
#endif
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
    bDetachModifierHeld = true;
}

void ACMPlayerController::DetachModifierReleased()
{
    bDetachModifierHeld = false;
}

void ACMPlayerController::ReverseModifierPressed()
{
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
        || SlotIndex < 0
        || SlotIndex >= CMControl::MaxKeysPerPlayer)
    {
        return;
    }

    // Controller는 어떤 파츠가 배정됐는지 알지 않는다.
    // Possess 중인 ControlBody에 SlotIndex와 Press/Release만 전달한다.
    if (ACMControlBody* ControlBody = GetPawn<ACMControlBody>())
    {
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

ACMChimera*
ACMPlayerController::GetSharedChimera() const
{
    const ACMGameState* GameState =
        GetWorld() ? GetWorld()->GetGameState<ACMGameState>() : nullptr;
    return GameState ? GameState->SharedChimera : nullptr;
}
