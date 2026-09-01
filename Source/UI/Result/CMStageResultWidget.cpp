#include "Result/CMStageResultWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "ListenServerNetworkSettings.h"
#include "ListenServerSessionSubsystem.h"
#include "Player/CMPlayerController.h"

#define LOCTEXT_NAMESPACE "CMStageResultWidget"

UCMStageResultWidget::UCMStageResultWidget(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InputConfig = ENKMUIWidgetInputMode::Menu;
    GameMouseCaptureMode = EMouseCaptureMode::NoCapture;
    SetIsFocusable(true);
}

void UCMStageResultWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RestartButton->OnClicked.AddUniqueDynamic(
        this, &ThisClass::HandleRestartClicked);
    NextLevelButton->OnClicked.AddUniqueDynamic(
        this, &ThisClass::HandleNextLevelClicked);
    MainMenuButton->OnClicked.AddUniqueDynamic(
        this, &ThisClass::HandleMainMenuClicked);
}

void UCMStageResultWidget::NativeDestruct()
{
    RestartButton->OnClicked.RemoveDynamic(
        this, &ThisClass::HandleRestartClicked);
    NextLevelButton->OnClicked.RemoveDynamic(
        this, &ThisClass::HandleNextLevelClicked);
    MainMenuButton->OnClicked.RemoveDynamic(
        this, &ThisClass::HandleMainMenuClicked);
    Super::NativeDestruct();
}

UWidget* UCMStageResultWidget::NativeGetDesiredFocusTarget() const
{
    if (NextLevelButton && NextLevelButton->GetIsEnabled())
    {
        return NextLevelButton;
    }
    return RestartButton && RestartButton->GetIsEnabled()
        ? RestartButton.Get()
        : MainMenuButton.Get();
}

void UCMStageResultWidget::SetResult(
    float ElapsedSeconds,
    bool bCanControlResult,
    bool bHasNextStage)
{
    const int32 TotalSeconds = FMath::Max(0, FMath::RoundToInt(ElapsedSeconds));
    const int32 Minutes = TotalSeconds / 60;
    const int32 Seconds = TotalSeconds % 60;
    TimeText->SetText(FText::Format(
        LOCTEXT("ClearTimeFormat", "클리어 시간  {0}:{1}"),
        FText::AsNumber(Minutes, &FNumberFormattingOptions::DefaultNoGrouping()),
        FText::FromString(FString::Printf(TEXT("%02d"), Seconds))));

    RestartButton->SetIsEnabled(bCanControlResult);
    NextLevelButton->SetIsEnabled(bCanControlResult && bHasNextStage);
}

void UCMStageResultWidget::HandleRestartClicked()
{
    if (ACMPlayerController* PlayerController =
        GetOwningPlayer<ACMPlayerController>())
    {
        PlayerController->RequestRestartCompletedStage();
    }
}

void UCMStageResultWidget::HandleNextLevelClicked()
{
    if (ACMPlayerController* PlayerController =
        GetOwningPlayer<ACMPlayerController>())
    {
        PlayerController->RequestAdvanceCompletedStage();
    }
}

void UCMStageResultWidget::HandleMainMenuClicked()
{
    UListenServerSessionSubsystem* Network = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UListenServerSessionSubsystem>()
        : nullptr;
    if (Network && Network->LeaveAndReturnToMenu())
    {
        return;
    }

    const UListenServerNetworkSettings* Settings =
        GetDefault<UListenServerNetworkSettings>();
    const FString MainMenuPackage = Settings
        ? Settings->MainMenuMap.GetLongPackageName()
        : FString();
    if (!MainMenuPackage.IsEmpty())
    {
        UGameplayStatics::OpenLevel(this, FName(*MainMenuPackage));
    }
}

#undef LOCTEXT_NAMESPACE
