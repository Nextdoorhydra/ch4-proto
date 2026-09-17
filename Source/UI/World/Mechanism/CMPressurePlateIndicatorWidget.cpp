#include "World/Mechanism/CMPressurePlateIndicatorWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "CMPressurePlateIndicatorWidget"

void UCMPressurePlateIndicatorWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WeightText)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Pressure plate indicator is missing WeightText."));
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    WeightText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    RefreshWeightText();
    ApplyCompletionStyle();
}

void UCMPressurePlateIndicatorWidget::NativeDestruct()
{
    StopAnimationTimer();
    Super::NativeDestruct();
}

void UCMPressurePlateIndicatorWidget::SetWeightState(
    float CurrentWeight,
    float RequiredWeight,
    bool bTriggered,
    bool bAnimate
)
{
    const float SafeCurrentWeight = FMath::IsFinite(CurrentWeight)
        ? FMath::Max(CurrentWeight, 0.0f)
        : 0.0f;
    RequiredWeightValue = FMath::IsFinite(RequiredWeight)
        ? FMath::Max(RequiredWeight, 0.0f)
        : 0.0f;

    const bool bIncreased = SafeCurrentWeight > TargetWeight;
    TargetWeight = SafeCurrentWeight;

    if (bAnimate && MaxCountDuration > 0.0f
        && !FMath::IsNearlyEqual(DisplayedWeight, TargetWeight))
    {
        CountStartWeight = DisplayedWeight;
        CountElapsed = 0.0f;
        const float WeightDelta = FMath::Abs(
            TargetWeight - CountStartWeight);
        ActiveCountDuration = FMath::Clamp(
            WeightDelta / FMath::Max(CountWeightPerSecond, 0.01f),
            FMath::Min(MinCountDuration, MaxCountDuration),
            FMath::Max(MinCountDuration, MaxCountDuration));
        bCounting = true;
    }
    else
    {
        DisplayedWeight = TargetWeight;
        bCounting = false;
    }

    if (bAnimate && bIncreased && IncreasePulseDuration > 0.0f
        && IncreasePulseScale > 1.0f)
    {
        PulseElapsed = 0.0f;
        bPulsing = true;
    }

    const bool bJustCompleted = bTriggered && !bComplete;
    bComplete = bTriggered;
    bCompletePulsing = bAnimate && bJustCompleted
        && CompletionGlow && CompletePulseDuration > 0.0f;
    CompletePulseElapsed = 0.0f;

    RefreshWeightText();
    ApplyCompletionStyle();
    StartAnimationTimer();
}

void UCMPressurePlateIndicatorWidget::AdvanceAnimations()
{
    AnimationTimerHandle.Invalidate();

    const UWorld* World = GetWorld();
    const float InDeltaTime = World ? World->GetDeltaSeconds() : 0.0f;

    bool bTextChanged = false;
    if (bCounting)
    {
        CountElapsed += InDeltaTime;
        const float Alpha = FMath::Clamp(
            CountElapsed / ActiveCountDuration, 0.0f, 1.0f);
        DisplayedWeight = FMath::InterpEaseOut(
            CountStartWeight, TargetWeight, Alpha, 2.0f);
        bTextChanged = true;
        if (Alpha >= 1.0f)
        {
            DisplayedWeight = TargetWeight;
            bCounting = false;
        }
    }

    if (bTextChanged)
    {
        RefreshWeightText();
    }

    if (bPulsing && WeightText)
    {
        PulseElapsed += InDeltaTime;
        const float Alpha = FMath::Clamp(
            PulseElapsed / IncreasePulseDuration, 0.0f, 1.0f);
        const float Scale = 1.0f
            + (IncreasePulseScale - 1.0f) * FMath::Sin(PI * Alpha);
        WeightText->SetRenderScale(FVector2D(Scale, Scale));
        if (Alpha >= 1.0f)
        {
            WeightText->SetRenderScale(FVector2D(1.0f, 1.0f));
            bPulsing = false;
        }
    }

    if (bCompletePulsing && CompletionGlow)
    {
        CompletePulseElapsed += InDeltaTime;
        const float Alpha = FMath::Clamp(
            CompletePulseElapsed / CompletePulseDuration, 0.0f, 1.0f);
        CompletionGlow->SetRenderOpacity(FMath::Lerp(
            CompletePulseGlowOpacity,
            CompleteGlowOpacity,
            FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f)));
        if (Alpha >= 1.0f)
        {
            CompletionGlow->SetRenderOpacity(CompleteGlowOpacity);
            bCompletePulsing = false;
        }
    }

    if (bCounting || bPulsing || bCompletePulsing)
    {
        StartAnimationTimer();
    }
    else
    {
        StopAnimationTimer();
    }
}

void UCMPressurePlateIndicatorWidget::RefreshWeightText()
{
    if (!WeightText)
    {
        return;
    }

    WeightText->SetText(FText::Format(
        LOCTEXT("WeightFormat", "{0} / {1} kg"),
        FText::AsNumber(FMath::RoundToInt(DisplayedWeight)),
        FText::AsNumber(FMath::RoundToInt(RequiredWeightValue))));
}

void UCMPressurePlateIndicatorWidget::ApplyCompletionStyle()
{
    if (WeightText)
    {
        WeightText->SetColorAndOpacity(FSlateColor(
            bComplete ? CompleteTextColor : IncompleteTextColor));
        WeightText->SetShadowColorAndOpacity(bComplete
            ? CompleteGlowColor
            : FLinearColor::Transparent);
    }

    if (CompletionGlow)
    {
        CompletionGlow->SetColorAndOpacity(CompleteGlowColor);
        CompletionGlow->SetVisibility(bComplete
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
        CompletionGlow->SetRenderOpacity(bCompletePulsing
            ? CompletePulseGlowOpacity
            : CompleteGlowOpacity);
    }
}

void UCMPressurePlateIndicatorWidget::StartAnimationTimer()
{
    if ((!bCounting && !bPulsing && !bCompletePulsing)
        || AnimationTimerHandle.IsValid())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        AnimationTimerHandle = World->GetTimerManager().SetTimerForNextTick(
            this,
            &ThisClass::AdvanceAnimations);
    }
}

void UCMPressurePlateIndicatorWidget::StopAnimationTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AnimationTimerHandle);
    }
    AnimationTimerHandle.Invalidate();
}

#undef LOCTEXT_NAMESPACE
