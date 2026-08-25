#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMCameraOcclusionComponent.generated.h"

class APlayerController;
class UCMCameraOcclusionConfig;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;

USTRUCT()
struct FCMCameraOccluderFadeState
{
    GENERATED_BODY()

    TWeakObjectPtr<UPrimitiveComponent> Component;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;

    TArray<FVector2D> ScreenCenters;

    float Fade = 0.0f;
    bool bOccluding = false;
};

/** Locally fades objects between the active top-view camera and its owner. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMCameraOcclusionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMCameraOcclusionComponent();

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    const UCMCameraOcclusionConfig& GetOcclusionConfig() const;
    APlayerController* FindLocalViewer() const;
    void FindTargetLocations(
        const UCMCameraOcclusionConfig& Config,
        TArray<FVector>& OutTargetLocations
    ) const;
    void FindCurrentOccluders(
        APlayerController& PlayerController,
        TMap<UPrimitiveComponent*, TArray<FVector2D>>& OutOccluders
    ) const;
    FCMCameraOccluderFadeState* FindOrAddFadeState(
        UPrimitiveComponent& Component
    );
    void UpdateFadeStates(float DeltaTime);
    void RestoreFadeState(FCMCameraOccluderFadeState& State);
    void RestoreAllFadeStates();

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Camera Occlusion",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCMCameraOcclusionConfig> OcclusionConfig;

    UPROPERTY(Transient)
    TArray<FCMCameraOccluderFadeState> FadeStates;
};
