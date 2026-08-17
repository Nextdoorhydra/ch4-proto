#include "CMPlayerController.h"

#include "Player/CMPawn.h"
#include "Player/CMPlayerState.h"
#include "GameMode/CMGameState.h"
#include "GameMode/CMGameMode.h"
#include "GameMode/Play/CMPlayGameMode.h"
#include "GameMode/Lobby/CMLobbyGameMode.h"
#include "AsyncLoad/CMClientStageLoadComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"

ACMPlayerController::ACMPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    ClientStageLoadComponent = CreateDefaultSubobject<UCMClientStageLoadComponent>(
        TEXT("ClientStageLoadComponent"));

    for (ECMControlPart& PressedPart : PressedControlParts)
    {
        PressedPart = ECMControlPart::None;
    }
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

void ACMPlayerController::SetupInputComponent()
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
        &ACMPlayerController::FirstControlKeyPressed
    );
    InputComponent->BindKey(
        EKeys::W,
        IE_Pressed,
        this,
        &ACMPlayerController::SecondControlKeyPressed
    );
    InputComponent->BindKey(
        EKeys::E,
        IE_Pressed,
        this,
        &ACMPlayerController::ThirdControlKeyPressed
    );
    InputComponent->BindKey(
        EKeys::R,
        IE_Pressed,
        this,
        &ACMPlayerController::FourthControlKeyPressed
    );
    InputComponent->BindKey(
        EKeys::Q,
        IE_Released,
        this,
        &ACMPlayerController::FirstControlKeyReleased
    );
    InputComponent->BindKey(
        EKeys::W,
        IE_Released,
        this,
        &ACMPlayerController::SecondControlKeyReleased
    );
    InputComponent->BindKey(
        EKeys::E,
        IE_Released,
        this,
        &ACMPlayerController::ThirdControlKeyReleased
    );
    InputComponent->BindKey(
        EKeys::R,
        IE_Released,
        this,
        &ACMPlayerController::FourthControlKeyReleased
    );
    InputComponent->BindAxisKey(
        EKeys::MouseWheelAxis,
        this,
        &ACMPlayerController::ZoomCamera
    );
}

void ACMPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    if (!IsLocalController())
    {
        return;
    }


    ACMPawn* SharedChimera = GetSharedChimera();
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

// 로컬 로비 UI의 시작 요청을 서버 RPC로 전달
void ACMPlayerController::RequestStartCampaign()
{
    if (IsLocalController())
    {
        ServerRequestStartCampaign();
    }
}

// 서버 LobbyGameMode가 준비 상태와 캠페인 설정을 최종 검증
void ACMPlayerController::ServerRequestStartCampaign_Implementation()
{
    ACMLobbyGameMode* LobbyGameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMLobbyGameMode>() : nullptr;
    if (LobbyGameMode)
    {
        LobbyGameMode->TryStartCampaign(this);
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

    ServerSetControlSlotPressed(SlotIndex, bPressed);
}

void ACMPlayerController::ServerSetControlSlotPressed_Implementation(
    int32 SlotIndex,
    bool bPressed
)
{
    if (SlotIndex < 0 || SlotIndex >= CMControl::MaxKeysPerPlayer)
    {
        return;
    }

    ACMPlayerState* CMPlayerState =
        GetPlayerState<ACMPlayerState>();
    if (!CMPlayerState)
    {
        return;
    }

    ACMGameState* GameState =
        GetWorld() ? GetWorld()->GetGameState<ACMGameState>() : nullptr;
    ACMPawn* SharedChimera = GameState
        ? GameState->SharedChimera
        : nullptr;
    if (!SharedChimera)
    {
        return;
    }

    ECMControlPart& PressedPart = PressedControlParts[SlotIndex];
    if (!bPressed)
    {
        if (CMControl::IsValidPart(PressedPart))
        {
            SharedChimera->SetControlPartPressed(PressedPart, false);
        }
        PressedPart = ECMControlPart::None;
        return;
    }

    const ECMControlPart ControlPart =
        CMPlayerState->GetControlPartForSlot(SlotIndex);
    if (!CMControl::IsValidPart(ControlPart))
    {
        return;
    }

    if (CMControl::IsValidPart(PressedPart)
        && PressedPart != ControlPart)
    {
        SharedChimera->SetControlPartPressed(PressedPart, false);
    }

    PressedPart = ControlPart;
    SharedChimera->SetControlPartPressed(ControlPart, true);
    SharedChimera->ActivateControlPart(
        ControlPart,
        CMPlayerState
    );
}

void ACMPlayerController::LookYaw(float AxisValue)
{
    ACMPawn* SharedChimera = GetSharedChimera();
    if (!SharedChimera || FMath::IsNearlyZero(AxisValue))
    {
        return;
    }

    LocalCameraRotation.Yaw +=
        AxisValue * SharedChimera->GetMouseLookSensitivity();
}

void ACMPlayerController::LookPitch(float AxisValue)
{
    ACMPawn* SharedChimera = GetSharedChimera();
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

void ACMPlayerController::ZoomCamera(float AxisValue)
{
    ACMPawn* SharedChimera = GetSharedChimera();
    if (SharedChimera)
    {
        SharedChimera->AdjustLocalCameraZoom(AxisValue);
    }
}

ACMPawn*
ACMPlayerController::GetSharedChimera() const
{
    const ACMGameState* GameState =
        GetWorld() ? GetWorld()->GetGameState<ACMGameState>() : nullptr;
    return GameState ? GameState->SharedChimera : nullptr;
}
