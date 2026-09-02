#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "CMVisionComponent.generated.h"

UENUM(BlueprintType)
enum class ECMVisionContribution : uint8
{
    RevealAndTint,
    TintOnly
};

/** One replicated Head source contributing cone and near vision. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMVisionComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UCMVisionComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Vision")
    void ConfigureVision(
        float InAngleDegrees,
        float InDistance,
        float InNearVisionRadius
    );

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Vision")
    void SetVisionActive(bool bInActive);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Vision")
    void SetVisionContribution(ECMVisionContribution InContribution);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Vision")
    void SetAimDirection(const FVector& InAimDirection);

    void SetNetworkAimDirection(
        const FVector& InAimDirection,
        float InAimRotationDegrees
    );

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    bool IsVisionActive() const;

    bool IsVisionActiveWithoutStatus() const { return bVisionActive; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    bool IsBlinded() const { return bBlinded; }

    bool IsBlindnessPending() const { return bBlindnessPending; }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Vision")
    void ApplyBlindness(
        float Duration,
        float Delay = 0.0f,
        float RecoveryDuration = 0.75f);

    void ApplyVisionReduction(
        float Duration,
        float AngleMultiplier,
        float DistanceMultiplier,
        UObject* Source);

    void RemoveVisionReduction(UObject* Source);

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    bool IsLocationInsideVisionCone(const FVector& WorldLocation) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    ECMVisionContribution GetVisionContribution() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    float GetVisionAngleDegrees() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    float GetVisionDistance() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    float GetNearVisionRadius() const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Vision")
    void SetVisionEyeHeightOffset(float InHeightOffset);

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    float GetVisionEyeHeightOffset() const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Vision")
    void SetVisionTint(const FLinearColor& InColor, float InStrength);

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    FLinearColor GetVisionTint() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    FVector GetAimDirection() const;

    /** Visual-only direction: immediate for the owning client, smoothed remotely. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    FVector GetRenderedAimDirection() const;

    /** Local visual prediction only. Never changes the authoritative aim. */
    void SetLocalPredictedAimDirection(const FVector& InAimDirection);
    void ClearLocalAimPrediction();

    /** Logical eye position above the embedded Head actor origin. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    FVector GetVisionOrigin() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    bool IsLocationVisible(const FVector& WorldLocation) const;

    static bool IsPointInsideVisionCone(
        const FVector& Origin,
        const FVector& Direction,
        float AngleDegrees,
        float Distance,
        const FVector& WorldLocation
    );

    static float ResolveUnwrappedAimRotation(
        float WrappedAngleDegrees,
        float ReferenceRotationDegrees
    );

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Vision",
        meta = (AllowPrivateAccess = "true"))
    bool bVisionActive = false;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Vision",
        meta = (AllowPrivateAccess = "true"))
    bool bBlinded = false;

    UPROPERTY(Replicated)
    float BlindnessRecoveryDuration = 0.75f;

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly,
        Category = "Chimera|Vision",
        meta = (AllowPrivateAccess = "true"))
    ECMVisionContribution VisionContribution =
        ECMVisionContribution::RevealAndTint;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Vision",
        meta = (AllowPrivateAccess = "true"))
    float VisionAngleDegrees = 90.0f;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Vision",
        meta = (AllowPrivateAccess = "true"))
    float VisionDistance = 1200.0f;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Vision",
        meta = (AllowPrivateAccess = "true"))
    float NearVisionRadius = 150.0f;

    /** Logical eye height above the embedded Head actor origin. */
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly,
        Category = "Chimera|Vision",
        meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
    float VisionEyeHeightOffset = 80.0f;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Vision",
        meta = (AllowPrivateAccess = "true"))
    FLinearColor VisionTint = FLinearColor::Transparent;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Vision",
        meta = (AllowPrivateAccess = "true"))
    FVector_NetQuantizeNormal AimDirection = FVector::ForwardVector;

    UPROPERTY(ReplicatedUsing = OnRep_AimRotationDegrees)
    float AimRotationDegrees = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Vision|Smoothing",
        meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
    float RemoteAimInterpolationSpeedDegrees = 720.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Vision|Smoothing",
        meta = (ClampMin = "0.01", AllowPrivateAccess = "true"))
    float RemoteAimMaximumCatchUpTime = 0.05f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Vision|Smoothing",
        meta = (ClampMin = "0.1", AllowPrivateAccess = "true"))
    float LocalPredictionTimeout = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Vision|Smoothing",
        meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
    float StatusInterpolationSpeed = 2.0f;

    UPROPERTY(Replicated)
    float VisionAngleStatusMultiplier = 1.0f;

    UPROPERTY(Replicated)
    float VisionDistanceStatusMultiplier = 1.0f;

    struct FActiveVisionReduction
    {
        int32 Handle = INDEX_NONE;
        TWeakObjectPtr<UObject> Source;
        float AngleMultiplier = 1.0f;
        float DistanceMultiplier = 1.0f;
        FTimerHandle ExpirationTimer;
    };

    TArray<FActiveVisionReduction> ActiveVisionReductions;
    float RenderedVisionAngleMultiplier = 1.0f;
    float RenderedVisionDistanceMultiplier = 1.0f;
    float RenderedBlindnessMultiplier = 1.0f;
    FTimerHandle BlindnessTimerHandle;
    FTimerHandle BlindnessDelayTimerHandle;
    float PendingBlindnessDuration = 0.0f;
    bool bBlindnessPending = false;
    int32 NextVisionReductionHandle = 1;

    void ClearBlindness();
    void BeginBlindness();
    void HandleVisionReductionExpired(int32 Handle);
    void RecalculateVisionReduction();

    FVector RenderedAimDirection = FVector::ForwardVector;
    float RenderedAimRotationDegrees = 0.0f;
    FVector LocalPredictedAimDirection = FVector::ForwardVector;
    float LastLocalPredictionTime = 0.0f;
    float RemoteAimCatchUpSpeedDegrees = 0.0f;
    bool bHasLocalAimPrediction = false;

    UFUNCTION()
    void OnRep_AimRotationDegrees();

    void RefreshRemoteAimCatchUpSpeed();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastNetworkAimDirection(
        FVector_NetQuantizeNormal InAimDirection,
        float InAimRotationDegrees
    );
};
