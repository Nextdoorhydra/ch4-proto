#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/CMControlTypes.h"

#include "CMWireframeHUDCaptureActor.generated.h"

class ACMChimera;
class ACMPartActorBase;
class UCurveLinearColor;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneCaptureComponent2D;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/**
 * Local-only capture rig used by the control HUD.
 * Scene-capture-only proxies follow the replicated Chimera without appearing
 * in the main world view.
 */
UCLASS(NotBlueprintable, Transient)
class UI_API ACMWireframeHUDCaptureActor : public AActor
{
    GENERATED_BODY()

public:
    ACMWireframeHUDCaptureActor();

    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void Initialize(
        ACMChimera* InChimera,
        UCurveLinearColor* InHealthColorCurve,
        const FRotator& InCaptureRotation,
        int32 RenderTargetSize = 512
    );

    UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

    void SetCameraView(const FRotator& InRotation, float InZoom);

    bool ProjectWorldLocation(
        const FVector& WorldLocation,
        FVector2D& OutNormalizedPosition
    ) const;

private:
    struct FBodyProxy
    {
        int32 SegmentIndex = INDEX_NONE;
        TWeakObjectPtr<USkeletalMeshComponent> SourceMesh;
        TObjectPtr<USkeletalMeshComponent> Mesh;
        TObjectPtr<UMaterialInstanceDynamic> Material;
    };

    struct FPartProxy
    {
        FCMPartSlotAddress Address;
        TWeakObjectPtr<ACMPartActorBase> SourcePart;
        TObjectPtr<USkeletalMeshComponent> Mesh;
        TObjectPtr<UMaterialInstanceDynamic> Material;
    };

    void SynchronizeBodyProxies();
    void SynchronizePartProxies();
    void UpdateCaptureView(float DeltaSeconds);
    void ConfigureProxy(class UPrimitiveComponent& Proxy) const;
    UMaterialInstanceDynamic* CreateWireframeMaterial(UObject* Outer) const;
    FLinearColor EvaluateHealthColor(
        float Health,
        float MaxHealth,
        bool bDestroyedBody
    ) const;
    void RemoveBodyProxy(int32 ProxyIndex);
    void RemovePartProxy(int32 FlatSlotIndex);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneCaptureComponent2D> SceneCapture;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> RenderTarget;

    UPROPERTY(Transient)
    TObjectPtr<UCurveLinearColor> HealthColorCurve;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> WireframeMaterial;

    TWeakObjectPtr<ACMChimera> Chimera;
    TArray<FBodyProxy> BodyProxies;
    TArray<FPartProxy> PartProxies;
    FRotator CaptureRotation = FRotator(-90.0f, -90.0f, 0.0f);
    float CaptureZoom = 1.0f;
    FVector SmoothedViewCenter = FVector::ZeroVector;
    float SmoothedOrthoWidth = 800.0f;
    bool bViewInitialized = false;
};
