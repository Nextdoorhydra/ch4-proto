#include "CMOptionWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Settings/CMUserSettingsSubsystem.h"

namespace CMOptionWidget
{
    const FString Fullscreen = TEXT("전체 화면");
    const FString Borderless = TEXT("테두리 없는 창");
    const FString Windowed = TEXT("창 모드");
    const FString Korean = TEXT("한국어");
    const FString English = TEXT("English");
}

UCMOptionWidget::UCMOptionWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InputConfig = ENKMUIWidgetInputMode::Menu;
    GameMouseCaptureMode = EMouseCaptureMode::NoCapture;
    SetIsFocusable(true);

    ResolutionPresets = {
        { TEXT("1280 x 720"), FIntPoint(1280, 720) },
        { TEXT("1600 x 900"), FIntPoint(1600, 900) },
        { TEXT("1920 x 1080"), FIntPoint(1920, 1080) },
        { TEXT("2560 x 1440"), FIntPoint(2560, 1440) }
    };
}

void UCMOptionWidget::NativeConstruct()
{
    Super::NativeConstruct();

    Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
    Combo_Resolution->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleResolutionChanged);
    Combo_WindowMode->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleWindowModeChanged);
    Combo_Language->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleLanguageChanged);
    Slider_MasterVolume->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleMasterVolumeChanged);
    Slider_BGMVolume->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleBGMVolumeChanged);
    Slider_SFXVolume->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSFXVolumeChanged);
}

void UCMOptionWidget::NativeDestruct()
{
    Btn_Close->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
    Combo_Resolution->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleResolutionChanged);
    Combo_WindowMode->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleWindowModeChanged);
    Combo_Language->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
    Slider_MasterVolume->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleMasterVolumeChanged);
    Slider_BGMVolume->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleBGMVolumeChanged);
    Slider_SFXVolume->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleSFXVolumeChanged);
    Super::NativeDestruct();
}

void UCMOptionWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    RefreshFromRuntime();
}

void UCMOptionWidget::NativeOnDeactivated()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UCMUserSettingsSubsystem* Settings =
            GameInstance->GetSubsystem<UCMUserSettingsSubsystem>())
        {
            Settings->SaveSettings();
        }
    }
    Super::NativeOnDeactivated();
}

FReply UCMOptionWidget::NativeOnPreviewKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        DeactivateWidget();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

UWidget* UCMOptionWidget::NativeGetDesiredFocusTarget() const
{
    return Combo_Resolution;
}

void UCMOptionWidget::RefreshFromRuntime()
{
    TGuardValue<bool> RefreshGuard(bIsRefreshing, true);
    UGameUserSettings* VideoSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;

    if (VideoSettings)
    {
        Combo_Resolution->ClearOptions();
        const FIntPoint CurrentResolution = VideoSettings->GetScreenResolution();
        FString SelectedResolution;
        for (const FResolutionPreset& Preset : ResolutionPresets)
        {
            Combo_Resolution->AddOption(Preset.Label);
            if (Preset.Size == CurrentResolution)
            {
                SelectedResolution = Preset.Label;
            }
        }
        if (SelectedResolution.IsEmpty())
        {
            SelectedResolution = FString::Printf(
                TEXT("%d x %d"), CurrentResolution.X, CurrentResolution.Y);
            Combo_Resolution->AddOption(SelectedResolution);
        }
        Combo_Resolution->SetSelectedOption(SelectedResolution);

        Combo_WindowMode->ClearOptions();
        Combo_WindowMode->AddOption(CMOptionWidget::Fullscreen);
        Combo_WindowMode->AddOption(CMOptionWidget::Borderless);
        Combo_WindowMode->AddOption(CMOptionWidget::Windowed);

        FString SelectedMode = CMOptionWidget::Windowed;
        if (VideoSettings->GetFullscreenMode() == EWindowMode::Fullscreen)
        {
            SelectedMode = CMOptionWidget::Fullscreen;
        }
        else if (VideoSettings->GetFullscreenMode() == EWindowMode::WindowedFullscreen)
        {
            SelectedMode = CMOptionWidget::Borderless;
        }
        Combo_WindowMode->SetSelectedOption(SelectedMode);
    }

    Combo_Language->ClearOptions();
    Combo_Language->AddOption(CMOptionWidget::Korean);
    Combo_Language->AddOption(CMOptionWidget::English);
    FString Language = UKismetInternationalizationLibrary::GetCurrentLanguage();
#if WITH_EDITOR
    if (GIsEditor)
    {
        const FString PreviewLanguage = FTextLocalizationManager::Get()
            .GetConfiguredGameLocalizationPreviewLanguage();
        if (!PreviewLanguage.IsEmpty())
        {
            Language = PreviewLanguage;
        }
    }
#endif
    Combo_Language->SetSelectedOption(
        Language.StartsWith(TEXT("ko"), ESearchCase::IgnoreCase)
            ? CMOptionWidget::Korean
            : CMOptionWidget::English);

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (const UCMUserSettingsSubsystem* Settings =
            GameInstance->GetSubsystem<UCMUserSettingsSubsystem>())
        {
            Slider_MasterVolume->SetValue(Settings->GetMasterVolume());
            Slider_BGMVolume->SetValue(Settings->GetBGMVolume());
            Slider_SFXVolume->SetValue(Settings->GetSFXVolume());
            RefreshVolumeText(Text_MasterVolume, Settings->GetMasterVolume());
            RefreshVolumeText(Text_BGMVolume, Settings->GetBGMVolume());
            RefreshVolumeText(Text_SFXVolume, Settings->GetSFXVolume());
        }
    }
}

void UCMOptionWidget::RefreshVolumeText(UTextBlock* TextBlock, float Value) const
{
    TextBlock->SetText(FText::FromString(FString::Printf(
        TEXT("%d%%"), FMath::RoundToInt(Value * 100.0f))));
}

void UCMOptionWidget::ApplyVideoSettings(UGameUserSettings* UserSettings) const
{
    if (!UserSettings)
    {
        return;
    }
    UserSettings->ApplyResolutionSettings(false);
    UserSettings->ConfirmVideoMode();
    UserSettings->SaveSettings();
}

void UCMOptionWidget::HandleCloseClicked()
{
    DeactivateWidget();
}

void UCMOptionWidget::HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type)
{
    if (bIsRefreshing)
    {
        return;
    }

    UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (!Settings)
    {
        return;
    }
    for (const FResolutionPreset& Preset : ResolutionPresets)
    {
        if (Preset.Label == SelectedItem)
        {
            Settings->SetScreenResolution(Preset.Size);
            ApplyVideoSettings(Settings);
            return;
        }
    }
}

void UCMOptionWidget::HandleWindowModeChanged(FString SelectedItem, ESelectInfo::Type)
{
    if (bIsRefreshing)
    {
        return;
    }

    UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (!Settings)
    {
        return;
    }

    if (SelectedItem == CMOptionWidget::Fullscreen)
    {
        Settings->SetFullscreenMode(EWindowMode::Fullscreen);
    }
    else if (SelectedItem == CMOptionWidget::Borderless)
    {
        Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
    }
    else
    {
        Settings->SetFullscreenMode(EWindowMode::Windowed);
    }
    ApplyVideoSettings(Settings);
}

void UCMOptionWidget::HandleLanguageChanged(FString SelectedItem, ESelectInfo::Type)
{
    if (bIsRefreshing)
    {
        return;
    }

    const FString Culture = SelectedItem == CMOptionWidget::Korean
        ? TEXT("ko")
        : TEXT("en");

#if WITH_EDITOR
    if (GIsEditor)
    {
        FTextLocalizationManager::Get().ConfigureGameLocalizationPreviewLanguage(Culture);
        FTextLocalizationManager::Get().EnableGameLocalizationPreview(Culture);
        return;
    }
#endif

    UKismetInternationalizationLibrary::SetCurrentCulture(Culture, true);
}

void UCMOptionWidget::HandleMasterVolumeChanged(float Value)
{
    if (bIsRefreshing) return;
    if (UCMUserSettingsSubsystem* Settings = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMUserSettingsSubsystem>() : nullptr)
    {
        Settings->SetMasterVolume(Value);
        RefreshVolumeText(Text_MasterVolume, Value);
    }
}

void UCMOptionWidget::HandleBGMVolumeChanged(float Value)
{
    if (bIsRefreshing) return;
    if (UCMUserSettingsSubsystem* Settings = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMUserSettingsSubsystem>() : nullptr)
    {
        Settings->SetBGMVolume(Value);
        RefreshVolumeText(Text_BGMVolume, Value);
    }
}

void UCMOptionWidget::HandleSFXVolumeChanged(float Value)
{
    if (bIsRefreshing) return;
    if (UCMUserSettingsSubsystem* Settings = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCMUserSettingsSubsystem>() : nullptr)
    {
        Settings->SetSFXVolume(Value);
        RefreshVolumeText(Text_SFXVolume, Value);
    }
}
