#pragma once

#include "CoreMinimal.h"
#include "Parts/Head/CMVisionComponent.h"

#include "CMAggressiveSightComponent.generated.h"

/**
 * Server-authoritative hostile sight source. It reuses Chimera's vision tint
 * mask and enables its red sector only while the shared player pawn is seen.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class AI_API UCMAggressiveSightComponent : public UCMVisionComponent
{
    GENERATED_BODY()

public:
    UCMAggressiveSightComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void SetSightDefaults(float InSightDistanceCm, float InHorizontalSightAngleDegrees, float InVerticalSightAngleDegrees = 180.0f);

    void SetSightForwardReversed(bool bInReversed);

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Sight")
    float GetSightDistanceCm() const
    {
        return SightDistanceCm;
    }

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Sight")
    float GetHorizontalSightAngleDegrees() const
    {
        return HorizontalSightAngleDegrees;
    }

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Sight")
    float GetVerticalSightAngleDegrees() const
    {
        return VerticalSightAngleDegrees;
    }

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Sight")
    bool CanSeeActor(const AActor* Target) const;

    static bool IsPointInsideSight(const FVector& Origin, const FVector& Forward, float HorizontalAngleDegrees, float VerticalAngleDegrees, float DistanceCm, const FVector& Point);

protected:
    virtual void BeginPlay() override;

private:
    void ApplyVisionSettings();
    void DrawSightDebug() const;
    void UpdateAuthoritySight();
    bool CanSeeTargetPoint(const AActor& Target, const FVector& TargetPoint) const;
    bool HasClearSightTo(const AActor& Target, const FVector& TargetPoint) const;
    FVector GetSightForward() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Sight", meta = (ClampMin = "0.0", Units = "cm", AllowPrivateAccess = "true"))
    float SightDistanceCm = 300.0f;

    /** Total horizontal field of view, centered on the component forward. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Sight", meta = (ClampMin = "0.0", ClampMax = "360.0", Units = "deg", AllowPrivateAccess = "true"))
    float HorizontalSightAngleDegrees = 60.0f;

    /** Total vertical field of view. 180 degrees accepts every elevation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Sight", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg", AllowPrivateAccess = "true"))
    float VerticalSightAngleDegrees = 180.0f;

    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Sight", meta = (AllowPrivateAccess = "true"))
    bool bSightForwardReversed = false;
};
