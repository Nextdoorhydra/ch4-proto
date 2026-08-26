#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "CMUserSettingsSubsystem.generated.h"

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

    void SetMasterVolume(float Value);
    void SetBGMVolume(float Value);
    void SetSFXVolume(float Value);
    void SaveSettings();

private:
    void LoadSettings();
    void ApplyVolumes() const;
    void SetVolume(float& Target, float Value);

    float MasterVolume = 1.0f;
    float BGMVolume = 1.0f;
    float SFXVolume = 1.0f;
    bool bSettingsDirty = false;
};
