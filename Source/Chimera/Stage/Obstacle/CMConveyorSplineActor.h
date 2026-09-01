#pragma once

#include "CoreMinimal.h"
#include "Stage/Obstacle/CMStageObstacleBase.h"

#include "CMConveyorSplineActor.generated.h"

class ACMPartActorBase;
class ACMConveyorSegmentActor;
class UBoxComponent;
class UPrimitiveComponent;
class USplineComponent;

UENUM(BlueprintType)
enum class ECMConveyorDirection : uint8
{
    Forward,
    Reverse
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMConveyorDirectionChangedSignature, ECMConveyorDirection, Direction);

/** Moves unattached usable Parts along one open or closed conveyor route. */
UCLASS(Blueprintable)
class CHIMERA_API ACMConveyorSplineActor : public ACMStageObstacleBase
{
    GENERATED_BODY()

public:
    ACMConveyorSplineActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Conveyor")
    void SetConveyorDirection(ECMConveyorDirection NewDirection);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Conveyor")
    void ReverseConveyorDirection();

    UFUNCTION(BlueprintPure, Category = "Chimera|Conveyor")
    ECMConveyorDirection GetConveyorDirection() const { return Direction; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Conveyor")
    int32 GetTrackedPartCount() const { return TrackedParts.Num(); }

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Chimera|Conveyor|Route")
    void RebuildRoute();

    UFUNCTION(BlueprintPure, Category = "Chimera|Conveyor|Route")
    int32 GetRouteSegmentCount() const { return RouteSegments.Num(); }

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Conveyor")
    FCMConveyorDirectionChangedSignature OnDirectionChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void HandleObstacleActiveStateChanged(bool bIsActive) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<USplineComponent> ConveyorPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
    float MoveSpeed = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor", meta = (ClampMin = "0.02", ForceUnits = "s"))
    float MovementUpdateInterval = 0.033f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor")
    bool bClosedLoop = false;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Conveyor|Route")
    TObjectPtr<ACMConveyorSegmentActor> FirstSegment;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Route", meta = (ClampMin = "0.1", ForceUnits = "cm"))
    float ConnectionTolerance = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Route", meta = (ClampMin = "0.0", ClampMax = "90.0", ForceUnits = "deg"))
    float ConnectionAngleTolerance = 10.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Chimera|Conveyor|Route")
    TArray<TObjectPtr<ACMConveyorSegmentActor>> RouteSegments;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor")
    bool bAlignPartsToPath = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor")
    bool bPreserveInitialPartPlacement = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor")
    FRotator PartRotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor", meta = (ForceUnits = "cm"))
    float PartHeightOffset = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Detection", meta = (ClampMin = "1.0", ForceUnits = "cm"))
    float BeltWidth = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Detection", meta = (ClampMin = "1.0", ForceUnits = "cm"))
    float DetectionHeight = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Detection", meta = (ClampMin = "10.0", ForceUnits = "cm"))
    float DetectionSegmentLength = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Detection", meta = (ClampMin = "0.0", ForceUnits = "cm"))
    float DetectionOverlapPadding = 5.0f;

private:
    struct FTrackedPartState
    {
        float DistanceAlongRoute = 0.0f;
        FVector InitialPathLocalOffset = FVector::ZeroVector;
        FQuat InitialPathRelativeRotation = FQuat::Identity;
        int32 OverlapCount = 0;
        bool bHasInitialPlacement = false;
    };

    UFUNCTION()
    void OnRep_Direction();

    UFUNCTION()
    void HandleDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void HandleDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

    void RebuildDetectionVolumes();
    void BuildSegmentChain();
    void DestroyDetectionVolumes();
    void SetDetectionEnabled(bool bEnabled);
    void ScanInitialParts();
    void RegisterPart(ACMPartActorBase* PartActor, int32 AddedOverlapCount = 1);
    bool IsPartInsideDetection(const ACMPartActorBase* PartActor) const;
    void UpdateMovementTimer();
    void StopMovementTimer();
    void UpdateTrackedParts();
    void ApplyPartTransform(ACMPartActorBase* PartActor, const FTrackedPartState& State) const;
    void GetActivePaths(TArray<USplineComponent*>& OutPaths) const;
    float GetRouteLength() const;
    float FindClosestRouteDistance(const FVector& WorldLocation) const;
    USplineComponent* GetPathAtRouteDistance(float DistanceAlongRoute, float& OutDistanceAlongPath) const;
    static float CalculateNextDistance(float CurrentDistance, float SplineLength, float SignedTravelDistance, bool bLoop, bool& bOutReachedEnd);

    UPROPERTY(ReplicatedUsing = OnRep_Direction, EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor", meta = (AllowPrivateAccess = "true"))
    ECMConveyorDirection Direction = ECMConveyorDirection::Forward;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBoxComponent>> DetectionVolumes;

    TMap<TWeakObjectPtr<ACMPartActorBase>, FTrackedPartState> TrackedParts;
    FTimerHandle MovementTimerHandle;
    double LastMovementUpdateTime = 0.0;

    friend class FCMConveyorSplineDistanceTest;
};
