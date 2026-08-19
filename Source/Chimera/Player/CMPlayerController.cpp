#include "CMPlayerController.h"

#include "Player/CMControlBody.h"
#include "Player/CMChimera.h"
#include "Game/CMGameState.h"
#include "Game/CMGameMode.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPlayerController, Log, All);

ACMPlayerController::ACMPlayerController()
{
    // APlayerController의 기본 Tick은 입력 처리를 위해 유지한다.
    // 대신 이 클래스에서 매 프레임 Shared Chimera를 검색하던 PlayerTick은 제거했다.
    PrimaryActorTick.bCanEverTick = true;
    bAutoManageActiveCameraTarget = false;

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

void ACMPlayerController::RequestCheatSpawnRandomParts()
{
    if (IsLocalController())
    {
        ServerCheatSpawnRandomParts();
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
        ControlBody->SetControlSlotPressed(SlotIndex, bPressed);
    }
}

ACMChimera*
ACMPlayerController::GetSharedChimera() const
{
    const ACMGameState* GameState =
        GetWorld() ? GetWorld()->GetGameState<ACMGameState>() : nullptr;
    return GameState ? GameState->SharedChimera : nullptr;
}
