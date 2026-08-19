#include "Stage/CMStageLightingComponent.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Light.h"
#include "Engine/PostProcessVolume.h"

UCMStageLightingComponent::UCMStageLightingComponent()
{
    ExecutionPolicy = ECMStageCommandExecutionPolicy::AllMachines;
}

void UCMStageLightingComponent::BeginPlay()
{
    CaptureCurrentLighting();
    Super::BeginPlay();
}

void UCMStageLightingComponent::ExecuteStageCommand_Implementation(
    FGameplayTag CommandTag,
    UObject* CommandInstigator)
{
    if (const FCMStageLightingPreset* Preset = Presets.Find(CommandTag))
    {
        ApplyPreset(*Preset);
    }
}

void UCMStageLightingComponent::PreviewSelectedPreset()
{
    if (BaseLightIntensities.IsEmpty())
    {
        CaptureCurrentLighting();
    }
    if (const FCMStageLightingPreset* Preset = Presets.Find(PreviewCommandTag))
    {
        ApplyPreset(*Preset);
    }
}

void UCMStageLightingComponent::CaptureCurrentLighting()
{
    BaseLightIntensities.Reset();
    for (ALight* Light : Lights)
    {
        if (Light && Light->GetLightComponent())
        {
            BaseLightIntensities.Add(Light, Light->GetLightComponent()->Intensity);
        }
    }
}

void UCMStageLightingComponent::RestoreCapturedLighting()
{
    for (const TPair<TWeakObjectPtr<ALight>, float>& Pair : BaseLightIntensities)
    {
        if (Pair.Key.IsValid() && Pair.Key->GetLightComponent())
        {
            Pair.Key->GetLightComponent()->SetIntensity(Pair.Value);
        }
    }
}

void UCMStageLightingComponent::ApplyPreset(const FCMStageLightingPreset& Preset)
{
    for (ALight* Light : Lights)
    {
        if (!Light || !Light->GetLightComponent())
        {
            continue;
        }
        const float BaseIntensity = BaseLightIntensities.FindRef(Light) > 0.0f
            ? BaseLightIntensities.FindRef(Light)
            : Light->GetLightComponent()->Intensity;
        Light->GetLightComponent()->SetIntensity(BaseIntensity * Preset.LightIntensityMultiplier);
        if (Preset.bOverrideLightColor)
        {
            Light->GetLightComponent()->SetLightColor(Preset.LightColor);
        }
    }
    if (HeightFog && HeightFog->GetComponent())
    {
        HeightFog->GetComponent()->SetFogDensity(Preset.FogDensity);
    }
    if (PostProcessVolume)
    {
        PostProcessVolume->Settings.ColorGain = Preset.PostProcessColorGain;
    }
}
