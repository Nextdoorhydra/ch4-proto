#include "CMUserSettingsSubsystem.h"

#include "Misc/ConfigCacheIni.h"
#include "Sound/CMGameSoundBridgeSubsystem.h"
#include "Sound/NKMSoundSubsystem.h"

namespace CMUserSettings
{
    const TCHAR* Section = TEXT("/Script/UI.CMUserSettings");
    const TCHAR* MasterVolumeKey = TEXT("MasterVolume");
    const TCHAR* BGMVolumeKey = TEXT("BGMVolume");
    const TCHAR* SFXVolumeKey = TEXT("SFXVolume");
}

void UCMUserSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UNKMSoundSubsystem>();
    LoadSettings();
    ApplyVolumes();
}

void UCMUserSettingsSubsystem::Deinitialize()
{
    SaveSettings();
    Super::Deinitialize();
}

void UCMUserSettingsSubsystem::SetMasterVolume(float Value)
{
    SetVolume(MasterVolume, Value);
    if (UNKMSoundSubsystem* Sound = GetGameInstance()->GetSubsystem<UNKMSoundSubsystem>())
    {
        Sound->SetMasterVolume(MasterVolume);
    }
    if (UCMGameSoundBridgeSubsystem* SoundBridge =
        GetGameInstance()->GetSubsystem<UCMGameSoundBridgeSubsystem>())
    {
        SoundBridge->ApplyPersistentSFXVolumes();
    }
}

void UCMUserSettingsSubsystem::SetBGMVolume(float Value)
{
    SetVolume(BGMVolume, Value);
    if (UNKMSoundSubsystem* Sound = GetGameInstance()->GetSubsystem<UNKMSoundSubsystem>())
    {
        Sound->SetBGMVolume(BGMVolume);
    }
}

void UCMUserSettingsSubsystem::SetSFXVolume(float Value)
{
    SetVolume(SFXVolume, Value);
    if (UNKMSoundSubsystem* Sound = GetGameInstance()->GetSubsystem<UNKMSoundSubsystem>())
    {
        Sound->SetSFXVolume(SFXVolume);
    }
    if (UCMGameSoundBridgeSubsystem* SoundBridge =
        GetGameInstance()->GetSubsystem<UCMGameSoundBridgeSubsystem>())
    {
        SoundBridge->ApplyPersistentSFXVolumes();
    }
}

void UCMUserSettingsSubsystem::LoadSettings()
{
    MasterVolume = 1.0f;
    BGMVolume = 1.0f;
    SFXVolume = 1.0f;

    if (GConfig)
    {
        GConfig->GetFloat(CMUserSettings::Section, CMUserSettings::MasterVolumeKey,
            MasterVolume, GGameUserSettingsIni);
        GConfig->GetFloat(CMUserSettings::Section, CMUserSettings::BGMVolumeKey,
            BGMVolume, GGameUserSettingsIni);
        GConfig->GetFloat(CMUserSettings::Section, CMUserSettings::SFXVolumeKey,
            SFXVolume, GGameUserSettingsIni);
    }

    MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
    BGMVolume = FMath::Clamp(BGMVolume, 0.0f, 1.0f);
    SFXVolume = FMath::Clamp(SFXVolume, 0.0f, 1.0f);
    bSettingsDirty = false;
}

void UCMUserSettingsSubsystem::ApplyVolumes() const
{
    if (UNKMSoundSubsystem* Sound = GetGameInstance()->GetSubsystem<UNKMSoundSubsystem>())
    {
        Sound->SetMasterVolume(MasterVolume);
        Sound->SetBGMVolume(BGMVolume);
        Sound->SetSFXVolume(SFXVolume);
    }
    if (UCMGameSoundBridgeSubsystem* SoundBridge =
        GetGameInstance()->GetSubsystem<UCMGameSoundBridgeSubsystem>())
    {
        SoundBridge->ApplyPersistentSFXVolumes();
    }
}

void UCMUserSettingsSubsystem::SetVolume(float& Target, float Value)
{
    const float ClampedValue = FMath::Clamp(Value, 0.0f, 1.0f);
    if (!FMath::IsNearlyEqual(Target, ClampedValue))
    {
        Target = ClampedValue;
        bSettingsDirty = true;
    }
}

void UCMUserSettingsSubsystem::SaveSettings()
{
    if (!bSettingsDirty || !GConfig)
    {
        return;
    }

    GConfig->SetFloat(CMUserSettings::Section, CMUserSettings::MasterVolumeKey,
        MasterVolume, GGameUserSettingsIni);
    GConfig->SetFloat(CMUserSettings::Section, CMUserSettings::BGMVolumeKey,
        BGMVolume, GGameUserSettingsIni);
    GConfig->SetFloat(CMUserSettings::Section, CMUserSettings::SFXVolumeKey,
        SFXVolume, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
    bSettingsDirty = false;
}
