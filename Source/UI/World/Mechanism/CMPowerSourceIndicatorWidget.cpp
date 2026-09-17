#include "World/Mechanism/CMPowerSourceIndicatorWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

UCMPowerSourceIndicatorWidget::UCMPowerSourceIndicatorWidget(
    const FObjectInitializer& ObjectInitializer
)
    : Super(ObjectInitializer)
{
    SparkTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
        TEXT("/Game/Chimera/Environment/Obstacle/Powercable/Texture/T_UI_Spark.T_UI_Spark")));
}

void UCMPowerSourceIndicatorWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!PowerIcon)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Power source indicator is missing PowerIcon."));
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (UTexture2D* Texture = SparkTexture.LoadSynchronous())
    {
        PowerIcon->SetBrushFromTexture(Texture, true);
        if (PowerGlow)
        {
            PowerGlow->SetBrushFromTexture(Texture, true);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("Power source indicator could not load SparkTexture."));
    }

    PowerIcon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    if (PowerGlow)
    {
        PowerGlow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    }
    ApplyVisualState();
}

void UCMPowerSourceIndicatorWidget::NativeDestruct()
{
    StopAnimationTimer();
    Super::NativeDestruct();
}

void UCMPowerSourceIndicatorWidget::SetPowerState(
    bool bPoweredConnection,
    bool bAnimate
)
{
    // Connection and power delegates can report the same powered state back to
    // back. Do not let the duplicate event cancel an in-progress power-on cue.
    if (bPulsing && bPoweredConnection == bPowered)
    {
        return;
    }

    const bool bBecamePowered = bPoweredConnection && !bPowered;
    bPowered = bPoweredConnection;
    bPulsing = bAnimate && bBecamePowered
        && PulseDuration > 0.0f;
    PulseElapsed = 0.0f;

    if (!bPulsing)
    {
        StopAnimationTimer();
    }
    ApplyVisualState();
    StartAnimationTimer();
}

void UCMPowerSourceIndicatorWidget::ApplyVisualState()
{
    if (PowerIcon)
    {
        PowerIcon->SetColorAndOpacity(bPulsing
            ? DisconnectedColor
            : bPowered ? PoweredColor : DisconnectedColor);
        PowerIcon->SetRenderScale(FVector2D(1.0f, 1.0f));
        PowerIcon->SetRenderOpacity(1.0f);
    }

    if (PowerGlow)
    {
        PowerGlow->SetColorAndOpacity(GlowColor);
        PowerGlow->SetVisibility(ESlateVisibility::Collapsed);
        PowerGlow->SetRenderOpacity(0.0f);
    }
}

void UCMPowerSourceIndicatorWidget::AdvanceAnimation()
{
    AnimationTimerHandle.Invalidate();

    const UWorld* World = GetWorld();
    const float DeltaTime = World ? World->GetDeltaSeconds() : 0.0f;
    PulseElapsed += DeltaTime;
    const float Alpha = FMath::Clamp(
        PulseElapsed / PulseDuration, 0.0f, 1.0f);

    if (Alpha < 0.5f)
    {
        const bool bBlinkOn = Alpha < 0.125f
            || (Alpha >= 0.25f && Alpha < 0.375f);
        if (PowerIcon)
        {
            PowerIcon->SetColorAndOpacity(
                bBlinkOn ? PoweredColor : DisconnectedColor);
            PowerIcon->SetRenderScale(FVector2D(1.0f, 1.0f));
        }
        if (PowerGlow)
        {
            PowerGlow->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    else
    {
        const float SettleAlpha = FMath::InterpEaseOut(
            0.0f, 1.0f, (Alpha - 0.5f) * 2.0f, 2.0f);
        const float Scale = FMath::Lerp(
            PulseScale, 1.0f, SettleAlpha);

        if (PowerIcon)
        {
            PowerIcon->SetColorAndOpacity(FMath::Lerp(
                FlashColor, PoweredColor, SettleAlpha));
            PowerIcon->SetRenderScale(FVector2D(Scale, Scale));
        }
        if (PowerGlow)
        {
            PowerGlow->SetVisibility(ESlateVisibility::HitTestInvisible);
            PowerGlow->SetRenderOpacity(FMath::Lerp(
                PulseGlowOpacity, 0.0f, SettleAlpha));
        }
    }

    if (Alpha >= 1.0f)
    {
        bPulsing = false;
        ApplyVisualState();
        StopAnimationTimer();
        return;
    }

    StartAnimationTimer();
}

void UCMPowerSourceIndicatorWidget::StartAnimationTimer()
{
    if (!bPulsing || AnimationTimerHandle.IsValid())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        AnimationTimerHandle = World->GetTimerManager().SetTimerForNextTick(
            this, &ThisClass::AdvanceAnimation);
    }
}

void UCMPowerSourceIndicatorWidget::StopAnimationTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AnimationTimerHandle);
    }
    AnimationTimerHandle.Invalidate();
}
