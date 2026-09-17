#include "World/Mechanism/CMVisionStoneIndicatorWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

void UCMVisionStoneIndicatorWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WatcherCountText)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Vision stone indicator is missing WatcherCountText."));
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    ApplyState();
}

void UCMVisionStoneIndicatorWidget::NativeDestruct()
{
    StopPulseTimer();
    Super::NativeDestruct();
}

void UCMVisionStoneIndicatorWidget::SetVisionState(
    const FCMVisionStonePresentationState& State,
    bool bAnimate)
{
    const bool bCountChanged = bHasState
        && State.WatchingPlayerCount != PreviousWatchingPlayerCount;
    PreviousWatchingPlayerCount = State.WatchingPlayerCount;
    CurrentState = State;
    bHasState = State.bReady;
    bPulsing = bAnimate && bCountChanged && PulseDuration > 0.0f;
    PulseElapsed = 0.0f;

    if (!bPulsing)
    {
        StopPulseTimer();
    }
    ApplyState();
    StartPulseTimer();
}

void UCMVisionStoneIndicatorWidget::ApplyState()
{
    if (!WatcherCountText)
    {
        return;
    }

    const bool bNoWatchingMode = CurrentState.Mode
        == ECMVisionStoneMode::RequireNoWatchingPlayers;
    const bool bWarning = bNoWatchingMode
        && CurrentState.WatchingPlayerCount > 0;
    const FLinearColor Color = bWarning
        ? WarningColor
        : CurrentState.bConditionMet ? CompleteColor : IncompleteColor;

    const FString CountString = bNoWatchingMode
        ? FString::Printf(TEXT("%d"), CurrentState.WatchingPlayerCount)
        : FString::Printf(
            TEXT("%d / %d"),
            CurrentState.WatchingPlayerCount,
            CurrentState.RequiredWatchingPlayers);
    WatcherCountText->SetText(FText::FromString(CountString));
    WatcherCountText->SetColorAndOpacity(Color);

    if (OpenEyeIcon)
    {
        OpenEyeIcon->SetVisibility(bNoWatchingMode
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::HitTestInvisible);
        OpenEyeIcon->SetColorAndOpacity(Color);
    }
    if (ClosedEyeIcon)
    {
        ClosedEyeIcon->SetVisibility(bNoWatchingMode
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
        ClosedEyeIcon->SetColorAndOpacity(Color);
    }
    if (!bPulsing)
    {
        SetRenderScale(FVector2D(1.0f, 1.0f));
    }
}

void UCMVisionStoneIndicatorWidget::AdvancePulse()
{
    PulseTimerHandle.Invalidate();

    const UWorld* World = GetWorld();
    PulseElapsed += World ? World->GetDeltaSeconds() : 0.0f;
    const float Alpha = FMath::Clamp(
        PulseElapsed / PulseDuration, 0.0f, 1.0f);
    const bool bWarning = CurrentState.Mode
        == ECMVisionStoneMode::RequireNoWatchingPlayers
        && CurrentState.WatchingPlayerCount > 0;
    const float PeakScale = bWarning ? WarningPulseScale : CountPulseScale;
    const float Scale = 1.0f
        + (PeakScale - 1.0f) * FMath::Sin(PI * Alpha);
    SetRenderScale(FVector2D(Scale, Scale));

    if (Alpha >= 1.0f)
    {
        bPulsing = false;
        ApplyState();
        StopPulseTimer();
        return;
    }
    StartPulseTimer();
}

void UCMVisionStoneIndicatorWidget::StartPulseTimer()
{
    if (!bPulsing || PulseTimerHandle.IsValid())
    {
        return;
    }
    if (UWorld* World = GetWorld())
    {
        PulseTimerHandle = World->GetTimerManager().SetTimerForNextTick(
            this, &ThisClass::AdvancePulse);
    }
}

void UCMVisionStoneIndicatorWidget::StopPulseTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PulseTimerHandle);
    }
    PulseTimerHandle.Invalidate();
}
