#include "PrototypePlayerController.h"

#include "ChimeraPrototypePawn.h"
#include "ChimeraPlayerState.h"
#include "Chimera/Game/ChimeraGameState.h"
#include "Chimera/Game/PrototypeGameMode.h"
#include "Components/InputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"

APrototypePlayerController::APrototypePlayerController()
{
    PrimaryActorTick.bCanEverTick = true;

    for (EChimeraControlPart& PressedPart : PressedControlParts)
    {
        PressedPart = EChimeraControlPart::None;
    }
}

bool APrototypePlayerController::CanRequestRetryGame() const
{
    return IsLocalController()
        && HasAuthority()
        && GetNetMode() == NM_ListenServer
        && IsValid(GetSharedChimera());
}

void APrototypePlayerController::RequestRetryGame()
{
    if (!IsLocalController())
    {
        return;
    }

    ServerRequestRetryGame();
}

void APrototypePlayerController::ServerRequestRetryGame_Implementation()
{
    APrototypeGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<APrototypeGameMode>()
        : nullptr;
    if (GameMode)
    {
        GameMode->TryRetryGame(this);
    }
}

void APrototypePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController() || !DefaultMappingContext)
    {
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

void APrototypePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (!InputComponent)
    {
        return;
    }

    InputComponent->BindKey(
        EKeys::Q,
        IE_Pressed,
        this,
        &APrototypePlayerController::FirstControlKeyPressed
    );
    InputComponent->BindKey(
        EKeys::W,
        IE_Pressed,
        this,
        &APrototypePlayerController::SecondControlKeyPressed
    );
    InputComponent->BindKey(
        EKeys::E,
        IE_Pressed,
        this,
        &APrototypePlayerController::ThirdControlKeyPressed
    );
    InputComponent->BindKey(
        EKeys::R,
        IE_Pressed,
        this,
        &APrototypePlayerController::FourthControlKeyPressed
    );
    InputComponent->BindKey(
        EKeys::Q,
        IE_Released,
        this,
        &APrototypePlayerController::FirstControlKeyReleased
    );
    InputComponent->BindKey(
        EKeys::W,
        IE_Released,
        this,
        &APrototypePlayerController::SecondControlKeyReleased
    );
    InputComponent->BindKey(
        EKeys::E,
        IE_Released,
        this,
        &APrototypePlayerController::ThirdControlKeyReleased
    );
    InputComponent->BindKey(
        EKeys::R,
        IE_Released,
        this,
        &APrototypePlayerController::FourthControlKeyReleased
    );
    InputComponent->BindAxisKey(
        EKeys::MouseWheelAxis,
        this,
        &APrototypePlayerController::ZoomCamera
    );
}

void APrototypePlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    if (!IsLocalController())
    {
        return;
    }

    AChimeraPrototypePawn* SharedChimera = GetSharedChimera();
    if (CachedSharedChimera != SharedChimera)
    {
        CachedSharedChimera = SharedChimera;
        bLocalCameraInitialized = false;
    }

    if (!SharedChimera)
    {
        return;
    }

    if (GetViewTarget() != SharedChimera)
    {
        SetViewTarget(SharedChimera);
    }

    if (!bLocalCameraInitialized)
    {
        LocalCameraRotation = SharedChimera->GetInitialCameraRotation();
        bLocalCameraInitialized = true;
    }

    SharedChimera->SetLocalCameraRotation(LocalCameraRotation);
}

void APrototypePlayerController::FirstControlKeyPressed()
{
    SetControlSlotPressed(0, true);
}

void APrototypePlayerController::SecondControlKeyPressed()
{
    SetControlSlotPressed(1, true);
}

void APrototypePlayerController::ThirdControlKeyPressed()
{
    SetControlSlotPressed(2, true);
}

void APrototypePlayerController::FourthControlKeyPressed()
{
    SetControlSlotPressed(3, true);
}

void APrototypePlayerController::FirstControlKeyReleased()
{
    SetControlSlotPressed(0, false);
}

void APrototypePlayerController::SecondControlKeyReleased()
{
    SetControlSlotPressed(1, false);
}

void APrototypePlayerController::ThirdControlKeyReleased()
{
    SetControlSlotPressed(2, false);
}

void APrototypePlayerController::FourthControlKeyReleased()
{
    SetControlSlotPressed(3, false);
}

void APrototypePlayerController::SetControlSlotPressed(
    int32 SlotIndex,
    bool bPressed
)
{
    if (!IsLocalController()
        || SlotIndex < 0
        || SlotIndex >= ChimeraControl::MaxKeysPerPlayer)
    {
        return;
    }

    ServerSetControlSlotPressed(SlotIndex, bPressed);
}

void APrototypePlayerController::ServerSetControlSlotPressed_Implementation(
    int32 SlotIndex,
    bool bPressed
)
{
    if (SlotIndex < 0 || SlotIndex >= ChimeraControl::MaxKeysPerPlayer)
    {
        return;
    }

    AChimeraPlayerState* ChimeraPlayerState =
        GetPlayerState<AChimeraPlayerState>();
    if (!ChimeraPlayerState)
    {
        return;
    }

    AChimeraGameState* ChimeraGameState =
        GetWorld() ? GetWorld()->GetGameState<AChimeraGameState>() : nullptr;
    AChimeraPrototypePawn* SharedChimera = ChimeraGameState
        ? ChimeraGameState->SharedChimera
        : nullptr;
    if (!SharedChimera)
    {
        return;
    }

    EChimeraControlPart& PressedPart = PressedControlParts[SlotIndex];
    if (!bPressed)
    {
        if (ChimeraControl::IsValidPart(PressedPart))
        {
            SharedChimera->SetControlPartPressed(PressedPart, false);
        }
        PressedPart = EChimeraControlPart::None;
        return;
    }

    const EChimeraControlPart ControlPart =
        ChimeraPlayerState->GetControlPartForSlot(SlotIndex);
    if (!ChimeraControl::IsValidPart(ControlPart))
    {
        return;
    }

    if (ChimeraControl::IsValidPart(PressedPart)
        && PressedPart != ControlPart)
    {
        SharedChimera->SetControlPartPressed(PressedPart, false);
    }

    PressedPart = ControlPart;
    SharedChimera->SetControlPartPressed(ControlPart, true);
    SharedChimera->ActivateControlPart(
        ControlPart,
        ChimeraPlayerState
    );
}

void APrototypePlayerController::LookYaw(float AxisValue)
{
    AChimeraPrototypePawn* SharedChimera = GetSharedChimera();
    if (!SharedChimera || FMath::IsNearlyZero(AxisValue))
    {
        return;
    }

    LocalCameraRotation.Yaw +=
        AxisValue * SharedChimera->GetMouseLookSensitivity();
}

void APrototypePlayerController::LookPitch(float AxisValue)
{
    AChimeraPrototypePawn* SharedChimera = GetSharedChimera();
    if (!SharedChimera || FMath::IsNearlyZero(AxisValue))
    {
        return;
    }

    const float PitchDirection =
        SharedChimera->IsMousePitchInverted() ? 1.0f : -1.0f;
    LocalCameraRotation.Pitch = FMath::Clamp(
        LocalCameraRotation.Pitch
            + AxisValue
                * SharedChimera->GetMouseLookSensitivity()
                * PitchDirection,
        -89.0f,
        89.0f
    );
}

void APrototypePlayerController::ZoomCamera(float AxisValue)
{
    AChimeraPrototypePawn* SharedChimera = GetSharedChimera();
    if (SharedChimera)
    {
        SharedChimera->AdjustLocalCameraZoom(AxisValue);
    }
}

AChimeraPrototypePawn*
APrototypePlayerController::GetSharedChimera() const
{
    const AChimeraGameState* ChimeraGameState =
        GetWorld() ? GetWorld()->GetGameState<AChimeraGameState>() : nullptr;
    return ChimeraGameState ? ChimeraGameState->SharedChimera : nullptr;
}
