#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "CMUserSettingsSubsystem.generated.h"

enum class ECMColorVisionMode : uint8
{
    Normal,
    Deuteranope,
    Protanope,
    Tritanope
};

UCLASS()
class UI_API UCMUserSettingsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    float GetMasterVolume() const { return MasterVolume; }
    float GetBGMVolume() const { return BGMVolume; }
    float GetSFXVolume() const { return SFXVolume; }
    ECMColorVisionMode GetColorVisionMode() const { return ColorVisionMode; }

    void SetMasterVolume(float Value);
    void SetBGMVolume(float Value);
    void SetSFXVolume(float Value);
    void SetColorVisionMode(ECMColorVisionMode Value);
    void SaveSettings();

private:
    void LoadSettings();
    void ApplyVolumes() const;
    void ApplyColorVisionSettings() const;
    void SetVolume(float& Target, float Value);

    float MasterVolume = 1.0f;
    float BGMVolume = 1.0f;
    float SFXVolume = 1.0f;
    ECMColorVisionMode ColorVisionMode = ECMColorVisionMode::Normal;
    bool bSettingsDirty = false;
};
