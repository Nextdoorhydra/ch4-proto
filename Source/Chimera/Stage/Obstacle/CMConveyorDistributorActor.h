#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMConveyorDistributorActor.generated.h"

class ACMPartActorBase;
class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
class USplineComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ECMConveyorDistributorDirection : uint8
{
    Forward,
    Reverse
};

UENUM(BlueprintType)
enum class ECMConveyorDistributorRoutingMode : uint8
{
    Straight,
    Cross
};

UENUM(BlueprintType)
enum class ECMConveyorDistributorLane : uint8
{
    Left,
    Right
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMDistributorDirectionChangedSignature, ECMConveyorDistributorDirection, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMDistributorRoutingModeChangedSignature, ECMConveyorDistributorRoutingMode, RoutingMode);

/** Two-lane conveyor that can pass Parts straight through or switch them to the opposite lane. */
UCLASS(Blueprintable)
class CHIMERA_API ACMConveyorDistributorActor : public AActor
{
    GENERATED_BODY()

public:
    ACMConveyorDistributorActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Conveyor|Distributor")
    void SetDistributorDirection(ECMConveyorDistributorDirection NewDirection);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Conveyor|Distributor")
    void ReverseDistributorDirection();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Conveyor|Distributor")
    void SetRoutingMode(ECMConveyorDistributorRoutingMode NewRoutingMode);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Conveyor|Distributor")
    void ToggleRoutingMode();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Conveyor|Distributor")
    void SetSideMeshesSwapped(bool bNewSwapped);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Conveyor|Distributor")
    void SwapSideMeshes();

    UFUNCTION(BlueprintPure, Category = "Chimera|Conveyor|Distributor")
    ECMConveyorDistributorDirection GetDistributorDirection() const { return Direction; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Conveyor|Distributor")
    ECMConveyorDistributorRoutingMode GetRoutingMode() const { return RoutingMode; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Conveyor|Distributor")
    bool AreSideMeshesSwapped() const { return bSideMeshesSwapped; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Conveyor|Distributor")
    int32 GetTrackedPartCount() const { return TrackedParts.Num(); }

    void GetConnectionWorldTransforms(TArray<FTransform>& OutTransforms) const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Conveyor|Distributor")
    bool TryAcceptPart(ACMPartActorBase* PartActor);

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Conveyor|Distributor")
    FCMDistributorDirectionChangedSignature OnDirectionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Conveyor|Distributor")
    FCMDistributorRoutingModeChangedSignature OnRoutingModeChanged;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UStaticMeshComponent> LeftMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UStaticMeshComponent> RightMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<USplineComponent> LeftPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<USplineComponent> RightPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UBoxComponent> LeftDetection;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UBoxComponent> RightDetection;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor")
    TObjectPtr<UStaticMesh> AssemblyLineBox04;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor")
    TObjectPtr<UStaticMesh> AssemblyLineBox06;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor", meta = (ForceUnits = "cm"))
    float PathHeight = 123.18f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor|Placement", meta = (ClampMin = "10.0", ForceUnits = "cm"))
    float DistributorLength = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor|Placement", meta = (ClampMin = "10.0", ForceUnits = "cm"))
    float LaneSpacing = 220.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor|Placement", meta = (ClampMin = "0.0", ForceUnits = "cm"))
    float SmallMeshOutputOffset = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
    float MoveSpeed = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor", meta = (ClampMin = "0.02", ForceUnits = "s"))
    float MovementUpdateInterval = 0.033f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor")
    bool bPreserveInitialPartPlacement = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor|Detection", meta = (ClampMin = "1.0", ForceUnits = "cm"))
    float DetectionHalfWidth = 75.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor|Detection", meta = (ClampMin = "1.0", ForceUnits = "cm"))
    float DetectionHeight = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor|Detection", meta = (ClampMin = "0.0", ForceUnits = "cm"))
    float DetectionEndPadding = 5.0f;

private:
    struct FTrackedPartState
    {
        float Progress = 0.0f;
        FVector InitialPathLocalOffset = FVector::ZeroVector;
        FQuat InitialPathRelativeRotation = FQuat::Identity;
        int32 OverlapCount = 0;
        ECMConveyorDistributorLane Lane = ECMConveyorDistributorLane::Left;
        bool bHasInitialPlacement = false;
    };

    UFUNCTION()
    void HandleDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void HandleDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

    UFUNCTION()
    void OnRep_Direction();

    UFUNCTION()
    void OnRep_RoutingMode();

    UFUNCTION()
    void OnRep_SideMeshesSwapped();

    void RebuildLayout();
    void ConfigureLane(UStaticMeshComponent* MeshComponent, USplineComponent* Path, UBoxComponent* Detection, UStaticMesh* MeshAsset, bool bLeftLane);
    void ConfigureDetection(UBoxComponent* Detection);
    void ScanInitialParts();
    void RegisterPart(ACMPartActorBase* PartActor, int32 AddedOverlapCount = 1);
    bool IsPartInsideDetection(const ACMPartActorBase* PartActor) const;
    ECMConveyorDistributorLane FindClosestLane(const FVector& WorldLocation, float& OutProgress) const;
    USplineComponent* GetLanePath(ECMConveyorDistributorLane Lane) const;
    void ApplyPartTransform(ACMPartActorBase* PartActor, const FTrackedPartState& State) const;
    void UpdateMovementTimer();
    void StopMovementTimer();
    void UpdateTrackedParts();
    void ReleasePart(ACMPartActorBase* PartActor, bool bTryConveyorHandoff);
    bool TryHandoffToConveyor(ACMPartActorBase* PartActor) const;
    static float CalculateNextProgress(float CurrentProgress, float PathLength, float SignedTravelDistance, bool& bOutReachedEnd);
    static bool ShouldCrossLane(float PreviousProgress, float NewProgress, ECMConveyorDistributorRoutingMode CurrentRoutingMode);

    UPROPERTY(ReplicatedUsing = OnRep_Direction, EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor", meta = (AllowPrivateAccess = "true"))
    ECMConveyorDistributorDirection Direction = ECMConveyorDistributorDirection::Forward;

    UPROPERTY(ReplicatedUsing = OnRep_RoutingMode, EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor", meta = (AllowPrivateAccess = "true"))
    ECMConveyorDistributorRoutingMode RoutingMode = ECMConveyorDistributorRoutingMode::Straight;

    UPROPERTY(ReplicatedUsing = OnRep_SideMeshesSwapped, EditAnywhere, BlueprintReadOnly, Category = "Chimera|Conveyor|Distributor", meta = (AllowPrivateAccess = "true"))
    bool bSideMeshesSwapped = false;

    TMap<TWeakObjectPtr<ACMPartActorBase>, FTrackedPartState> TrackedParts;
    FTimerHandle MovementTimerHandle;
    double LastMovementUpdateTime = 0.0;

    friend class FCMConveyorDistributorTest;
};
