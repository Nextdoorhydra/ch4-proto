#pragma once

#include "CoreMinimal.h"
#include "Stage/CMStageElementComponent.h"

#include "CMStageLightingComponent.generated.h"

class ALight;
class AExponentialHeightFog;
class APostProcessVolume;

USTRUCT(BlueprintType)
struct FCMStageLightingPreset
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
    float LightIntensityMultiplier = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bOverrideLightColor = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bOverrideLightColor"))
    FLinearColor LightColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
    float FogDensity = 0.02f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FLinearColor PostProcessColorGain = FLinearColor::White;
};

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 레벨에서 직접 조정한 조명 그룹을 CommandTag별 프리셋으로 전환
class CHIMERA_API UCMStageLightingComponent : public UCMStageElementComponent
{
    GENERATED_BODY()

public:
    UCMStageLightingComponent();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Stage|Lighting")
    TArray<TObjectPtr<ALight>> Lights;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Stage|Lighting")
    TObjectPtr<AExponentialHeightFog> HeightFog;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Stage|Lighting")
    TObjectPtr<APostProcessVolume> PostProcessVolume;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Stage|Lighting")
    TMap<FGameplayTag, FCMStageLightingPreset> Presets;
    UPROPERTY(EditAnywhere, Category = "Chimera|Stage|Lighting")
    FGameplayTag PreviewCommandTag;

    UFUNCTION(CallInEditor, Category = "Chimera|Stage|Lighting")
    void PreviewSelectedPreset();

    UFUNCTION(CallInEditor, Category = "Chimera|Stage|Lighting")
    void CaptureCurrentLighting();

    UFUNCTION(CallInEditor, Category = "Chimera|Stage|Lighting")
    void RestoreCapturedLighting();

    virtual void ExecuteStageCommand_Implementation(FGameplayTag CommandTag, UObject* CommandInstigator) override;

protected:
    virtual void BeginPlay() override;

private:
    void ApplyPreset(const FCMStageLightingPreset& Preset);
    TMap<TWeakObjectPtr<ALight>, float> BaseLightIntensities;
};
