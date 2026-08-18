#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "CMVisionComponent.generated.h"

/** One replicated cone-shaped source contributing to shared Chimera vision. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMVisionComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UCMVisionComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Vision")
    void ConfigureVision(float InAngleDegrees, float InDistance);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Vision")
    void SetVisionActive(bool bInActive);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Vision")
    void SetAimDirection(const FVector& InAimDirection);

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    bool IsVisionActive() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    float GetVisionAngleDegrees() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    float GetVisionDistance() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    FVector GetAimDirection() const;

    /** Uses the authored Part Slot transform while this Head is attached. */
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
    float VisionAngleDegrees = 90.0f;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Vision",
        meta = (AllowPrivateAccess = "true"))
    float VisionDistance = 1200.0f;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Vision",
        meta = (AllowPrivateAccess = "true"))
    FVector_NetQuantizeNormal AimDirection = FVector::ForwardVector;
};
