#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMCameraOcclusionComponent.generated.h"

class APlayerController;
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
    APlayerController* FindLocalViewer() const;
    void FindCurrentOccluders(
        APlayerController& PlayerController,
        TSet<UPrimitiveComponent*>& OutOccluders,
        FVector2D& OutScreenCenter
    ) const;
    FCMCameraOccluderFadeState* FindOrAddFadeState(
        UPrimitiveComponent& Component
    );
    void UpdateFadeStates(float DeltaTime, const FVector2D& ScreenCenter);
    void RestoreFadeState(FCMCameraOccluderFadeState& State);
    void RestoreAllFadeStates();

    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion")
    TArray<TEnumAsByte<ECollisionChannel>> OccluderObjectTypes;

    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.0"))
    float TraceRadius = 50.0f;

    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion")
    float TargetHeightOffset = 80.0f;

    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.0"))
    float FadeOutSpeed = 5.0f;

    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.0"))
    float FadeInSpeed = 3.0f;

    /** Normalized viewport radius used by the obstacle material's screen mask. */
    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ScreenFadeRadius = 0.12f;

    /** Approximate visible coverage at the center of the dithered fade. */
    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumOpacity = 0.2f;

    /** Width of the soft transition around the screen-space fade circle. */
    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion",
        meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float EdgeSoftness = 0.03f;

    /** Material scalar: 0 is opaque, 1 is maximally faded. */
    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion|Material")
    FName FadeParameterName = TEXT("CM_OcclusionFade");

    /** Material vector: RG contains the character center in viewport UV. */
    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion|Material")
    FName ScreenCenterParameterName = TEXT("CM_OcclusionCenter");

    /** Material scalar containing the normalized viewport fade radius. */
    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion|Material")
    FName ScreenRadiusParameterName = TEXT("CM_OcclusionRadius");

    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion|Material")
    FName MinimumOpacityParameterName = TEXT("CM_OcclusionMinOpacity");

    UPROPERTY(EditAnywhere, Category = "Chimera|Camera Occlusion|Material")
    FName EdgeSoftnessParameterName = TEXT("CM_OcclusionEdgeSoftness");

    UPROPERTY(Transient)
    TArray<FCMCameraOccluderFadeState> FadeStates;
};
