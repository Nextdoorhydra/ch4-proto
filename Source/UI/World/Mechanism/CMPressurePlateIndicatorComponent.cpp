#include "World/Mechanism/CMPressurePlateIndicatorComponent.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Stage/Trigger/CMPressurePlateBase.h"
#include "World/Mechanism/CMPressurePlateIndicatorWidget.h"

UCMPressurePlateIndicatorComponent::UCMPressurePlateIndicatorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickInterval = 0.0f;
    SetWidgetSpace(EWidgetSpace::World);
    SetBlendMode(EWidgetBlendMode::Transparent);
    SetTwoSided(true);
    SetDrawAtDesiredSize(true);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetPivot(FVector2D(0.5f, 0.5f));
}

void UCMPressurePlateIndicatorComponent::BeginPlay()
{
    Super::BeginPlay();

    // Keep pre-existing BP component templates from restoring the old default.
    SetBlendMode(EWidgetBlendMode::Transparent);
    SetTwoSided(true);
    SetComponentTickEnabled(true);

    PressurePlate = Cast<ACMPressurePlateBase>(GetOwner());
    if (!PressurePlate.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Pressure plate indicator owner is not a pressure plate: %s"),
            *GetNameSafe(GetOwner()));
        SetComponentTickEnabled(false);
        SetVisibility(false);
        return;
    }

    PressurePlate->OnPresentationStateChanged.AddUniqueDynamic(
        this, &ThisClass::HandlePresentationStateChanged);
    ApplyPresentationState(
        PressurePlate->GetPresentationState(), false);
}

void UCMPressurePlateIndicatorComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bHasDisplayableState || FadeEndDistance <= 0.0f)
    {
        return;
    }

    DistanceUpdateElapsed += DeltaTime;
    if (bInsideFadeRange
        || DistanceUpdateElapsed >= FMath::Max(
            DistanceUpdateInterval, 0.02f))
    {
        DistanceUpdateElapsed = 0.0f;
        UpdateDistanceFade();
    }
}

void UCMPressurePlateIndicatorComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (PressurePlate.IsValid())
    {
        PressurePlate->OnPresentationStateChanged.RemoveDynamic(
            this, &ThisClass::HandlePresentationStateChanged);
    }
    PressurePlate.Reset();

    Super::EndPlay(EndPlayReason);
}

void UCMPressurePlateIndicatorComponent::HandlePresentationStateChanged(
    const FCMTriggerPresentationState& State
)
{
    ApplyPresentationState(State, bAnimateWeightChanges);
}

void UCMPressurePlateIndicatorComponent::ApplyPresentationState(
    const FCMTriggerPresentationState& State,
    bool bAnimate
)
{
    const bool bCanDisplay = State.bReady && State.bSupportsWeight;
    bHasDisplayableState = bCanDisplay;
    if (!bCanDisplay)
    {
        DistanceUpdateElapsed = 0.0f;
        bInsideFadeRange = false;
        SetComponentTickEnabled(false);
        SetVisibility(false);
        return;
    }

    SetComponentTickEnabled(true);

    InitWidget();
    if (UCMPressurePlateIndicatorWidget* IndicatorWidget =
            Cast<UCMPressurePlateIndicatorWidget>(GetUserWidgetObject()))
    {
        IndicatorWidget->SetWeightState(
            State.CurrentWeight,
            State.RequiredWeight,
            State.bTriggered,
            bAnimate && bHasAppliedReadyState);
        bHasAppliedReadyState = true;
    }

    UpdateDistanceFade();
}

void UCMPressurePlateIndicatorComponent::UpdateDistanceFade()
{
    if (!bHasDisplayableState)
    {
        SetVisibility(false);
        return;
    }

    if (FadeEndDistance <= 0.0f)
    {
        bInsideFadeRange = false;
        SetVisibility(true);
        if (UUserWidget* IndicatorWidget = GetUserWidgetObject())
        {
            IndicatorWidget->SetRenderOpacity(1.0f);
        }
        return;
    }

    const APlayerController* LocalPlayerController =
        UGameplayStatics::GetPlayerController(this, 0);
    const APlayerCameraManager* CameraManager = LocalPlayerController
        ? LocalPlayerController->PlayerCameraManager
        : nullptr;
    if (!CameraManager)
    {
        bInsideFadeRange = false;
        SetVisibility(false);
        return;
    }

    // Top-view zoom height must not affect world indicator visibility.
    const float Distance = FVector::Dist2D(
        CameraManager->GetCameraLocation(), GetComponentLocation());

    bInsideFadeRange = FadeEndDistance > FadeStartDistance
        && Distance > FadeStartDistance
        && Distance < FadeEndDistance;

    float Opacity = 1.0f;
    if (FadeEndDistance > 0.0f)
    {
        if (FadeEndDistance <= FadeStartDistance)
        {
            Opacity = Distance < FadeEndDistance ? 1.0f : 0.0f;
        }
        else
        {
            Opacity = 1.0f - FMath::Clamp(
                (Distance - FadeStartDistance)
                    / (FadeEndDistance - FadeStartDistance),
                0.0f,
                1.0f);
        }
    }

    SetVisibility(Opacity > KINDA_SMALL_NUMBER);
    if (UUserWidget* IndicatorWidget = GetUserWidgetObject())
    {
        IndicatorWidget->SetRenderOpacity(Opacity);
    }
}
