#include "World/Mechanism/CMVisionStoneIndicatorComponent.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "World/Mechanism/CMVisionStoneIndicatorWidget.h"

UCMVisionStoneIndicatorComponent::UCMVisionStoneIndicatorComponent()
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

void UCMVisionStoneIndicatorComponent::BeginPlay()
{
    Super::BeginPlay();

    FadeEndDistance = 0.0f;
    SetBlendMode(EWidgetBlendMode::Transparent);
    SetTwoSided(true);
    VisionStone = Cast<ACMVisionStoneBase>(GetOwner());
    if (!VisionStone.IsValid())
    {
        SetComponentTickEnabled(false);
        SetVisibility(false);
        return;
    }

    InitWidget();
    if (!Cast<UCMVisionStoneIndicatorWidget>(GetUserWidgetObject()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Vision stone indicator has no compatible WidgetClass: %s"),
            *GetNameSafe(GetOwner()));
        SetComponentTickEnabled(false);
        SetVisibility(false);
        return;
    }

    VisionStone->OnVisionPresentationStateChanged.AddUniqueDynamic(
        this, &ThisClass::HandleVisionStateChanged);
    ApplyVisionState(VisionStone->GetVisionPresentationState(), false);
}

void UCMVisionStoneIndicatorComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (VisionStone.IsValid())
    {
        VisionStone->OnVisionPresentationStateChanged.RemoveDynamic(
            this, &ThisClass::HandleVisionStateChanged);
    }
    VisionStone.Reset();
    Super::EndPlay(EndPlayReason);
}

void UCMVisionStoneIndicatorComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
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

void UCMVisionStoneIndicatorComponent::HandleVisionStateChanged(
    const FCMVisionStonePresentationState& State)
{
    ApplyVisionState(State, bHasAppliedReadyState);
}

void UCMVisionStoneIndicatorComponent::ApplyVisionState(
    const FCMVisionStonePresentationState& State,
    bool bAnimate)
{
    bHasDisplayableState = State.bReady;
    if (!State.bReady)
    {
        SetComponentTickEnabled(false);
        SetVisibility(false);
        return;
    }

    SetComponentTickEnabled(true);
    if (UCMVisionStoneIndicatorWidget* IndicatorWidget =
            Cast<UCMVisionStoneIndicatorWidget>(GetUserWidgetObject()))
    {
        IndicatorWidget->SetVisionState(
            State, bAnimate && bHasAppliedReadyState);
        bHasAppliedReadyState = true;
    }
    UpdateDistanceFade();
}

void UCMVisionStoneIndicatorComponent::UpdateDistanceFade()
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
        SetVisibility(true);
        if (UUserWidget* IndicatorWidget = GetUserWidgetObject())
        {
            IndicatorWidget->SetRenderOpacity(1.0f);
        }
        return;
    }

    const float Distance = FVector::Dist2D(
        CameraManager->GetCameraLocation(), GetComponentLocation());
    bInsideFadeRange = FadeEndDistance > FadeStartDistance
        && Distance > FadeStartDistance
        && Distance < FadeEndDistance;
    const float Opacity = FadeEndDistance <= FadeStartDistance
        ? Distance < FadeEndDistance ? 1.0f : 0.0f
        : 1.0f - FMath::Clamp(
            (Distance - FadeStartDistance)
                / (FadeEndDistance - FadeStartDistance),
            0.0f,
            1.0f);

    SetVisibility(Opacity > KINDA_SMALL_NUMBER);
    if (UUserWidget* IndicatorWidget = GetUserWidgetObject())
    {
        IndicatorWidget->SetRenderOpacity(Opacity);
    }
}
