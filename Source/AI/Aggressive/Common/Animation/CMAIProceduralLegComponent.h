#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"

#include "CMAIProceduralLegComponent.generated.h"

class UPrimitiveComponent;
class USceneComponent;

namespace CMAIProceduralLeg
{
    AI_API FVector CalculateSwingLocation(const FVector& Start, const FVector& Target, float Phase, float StepHeight);
    AI_API FVector CalculateDesiredFootLocation(const FVector& ContactLocation, const FVector& PlanarVelocity, float LeadSeconds, float MaximumLeadDistance);
    AI_API FVector CalculateKneeLocation(const FVector& Hip, const FVector& Foot, const FVector& PoleDirection, float UpperLength, float LowerLength);
    AI_API bool ShouldReplantFoot(const FVector& FootAnchor, const FVector& DesiredLocation, float PlanarSpeed, float StepTriggerDistance, float MinimumSpeed, float EmergencyDistance);
    AI_API bool ShouldStartStep(const FVector& PlantedLocation, const FVector& DesiredLocation, float PlanarSpeed, float TriggerDistance, float MinimumSpeed);
}

/** AI 전용 시각 다리다. 플레이어 Part/AnimInstance 상태를 사용하지 않고 접지점과 몸통 속도만으로 포즈를 만든다. */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class AI_API UCMAIProceduralLegComponent : public UPoseableMeshComponent
{
    GENERATED_BODY()

public:
    UCMAIProceduralLegComponent(const FObjectInitializer& ObjectInitializer);

    void Configure(USceneComponent* InContactPoint, FVector InOutwardLocalDirection, float InPhaseOffset, float InVisualScale = 1.0f);

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Procedural Leg", meta = (ClampMin = "0.01"))
    float StepDuration = 0.22f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Procedural Leg", meta = (ClampMin = "0.0"))
    float StepHeight = 28.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Procedural Leg", meta = (ClampMin = "0.0"))
    float StepTriggerDistance = 24.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Procedural Leg", meta = (ClampMin = "0.0"))
    float MinimumPlanarSpeed = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Procedural Leg", meta = (ClampMin = "0.0"))
    float FootLeadSeconds = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Procedural Leg", meta = (ClampMin = "0.0"))
    float MaximumFootLeadDistance = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Procedural Leg", meta = (ClampMin = "0.0"))
    float EmergencyReplantDistance = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Procedural Leg", meta = (ClampMin = "0.0"))
    float GroundTraceUpDistance = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Procedural Leg", meta = (ClampMin = "0.0"))
    float GroundTraceDownDistance = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Procedural Leg", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float MinimumGroundNormalZ = 0.5f;

private:
    bool InitializeLeg();
    bool FindGround(const FVector& CandidateLocation, FVector& OutLocation, FVector& OutNormal) const;
    UPrimitiveComponent* ResolveMovementBody() const;
    void StartStep(const FVector& DesiredLocation, const FVector& DesiredNormal, double CurrentTime);
    void UpdateLegPose(const FVector& FootLocation, const FVector& GroundNormal);

    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> ContactPoint;

    FVector OutwardLocalDirection = FVector::RightVector;
    FVector PlantedLocation = FVector::ZeroVector;
    FVector PlantedNormal = FVector::UpVector;
    FVector StepStartLocation = FVector::ZeroVector;
    FVector StepStartNormal = FVector::UpVector;
    FVector StepTargetLocation = FVector::ZeroVector;
    FVector StepTargetNormal = FVector::UpVector;
    double StepStartTime = 0.0;
    double NextStepTime = 0.0;
    float PhaseOffset = 0.0f;
    bool bInitialized = false;
    bool bStepping = false;

    static const FName ThighBoneName;
    static const FName CalfBoneName;
    static const FName FootBoneName;
};
