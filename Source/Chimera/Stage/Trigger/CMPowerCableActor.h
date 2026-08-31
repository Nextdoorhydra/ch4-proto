#pragma once

#include "CoreMinimal.h"
#include "AsyncLoadCompleteMessage.h"
#include "GameFramework/Actor.h"
#include "Parts/Arm/CMArmHoldTarget.h"

#include "CMPowerCableActor.generated.h"

class UCMPowerSocketComponent;
class UCMPowerCableDefinition;
class UBoxComponent;
class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMPowerCableConnectionChanged,
    bool,
    bConnected
);

/** Movable cable that can be connected to one matching power socket. */
UCLASS()
class CHIMERA_API ACMPowerCableActor : public AActor, public ICMArmHoldTarget
{
    GENERATED_BODY()

public:
    ACMPowerCableActor();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    bool BeginGrab(AActor* Grabber);

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    void ReleaseGrab();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    bool TryConnectToSocket(UCMPowerSocketComponent* Socket);

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    void Disconnect();

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool IsGrabbed() const { return Grabber != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool IsConnected() const { return ConnectedSocket != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    FName GetPowerChannel() const { return PowerChannel; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    FVector GetCableEndLocation() const;

    virtual bool QueryArmHold_Implementation(
        ACMArmPart* ArmPart,
        FCMArmHoldSpec& OutSpec
    ) const override;

    virtual bool BeginArmHold_Implementation(
        ACMArmPart* ArmPart
    ) override;

    virtual void EndArmHold_Implementation(
        ACMArmPart* ArmPart
    ) override;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Power")
    FCMPowerCableConnectionChanged OnConnectionChanged;

    void SetConnectedSocket(UCMPowerSocketComponent* Socket);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Power")
    TObjectPtr<USplineComponent> CableSpline;

    /** Query-only volume used by the arm hold trace to find the cable. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Power")
    TObjectPtr<UBoxComponent> GrabVolume;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Visual")
    TSoftObjectPtr<UCMPowerCableDefinition> CableDefinition;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Visual")
    FName LoadGroupId = TEXT("Stage.Entry.Power");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power")
    FName PowerChannel = NAME_None;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Power")
    TObjectPtr<AActor> Grabber;

    UPROPERTY(ReplicatedUsing = OnRep_ConnectedSocket, VisibleInstanceOnly,
        BlueprintReadOnly, Category = "Chimera|Power")
    TObjectPtr<UCMPowerSocketComponent> ConnectedSocket;

    UPROPERTY(ReplicatedUsing = OnRep_CableStartLocation,
        VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Power")
    FVector CableStartLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USplineMeshComponent>> CableMeshes;

    UFUNCTION()
    void OnRep_ConnectedSocket();

    UFUNCTION()
    void OnRep_CableStartLocation();

    UFUNCTION()
    void HandleLoadGroupFinished(
        FName FinishedLoadGroupId,
        EAsyncLoadResult Result,
        bool bReleasedImmediately
    );

    void RefreshCableVisualState();
    bool TryBuildCableVisual();
    void UpdateCableVisual();
    void UpdateGrabVolume();
    void EnsureCableMeshCount(int32 DesiredCount, UStaticMesh* Mesh);
    void InitializeRope();
    void SimulateRope(float DeltaSeconds);
    void ExtendRopeTo(float RequestedLength);
    void UpdateRopeNodeCount();
    int32 GetVisualSegmentCount() const;
    float GetCableSag() const;
    float GetCableThicknessScale() const;
    float GetInitialCableLength() const;
    float GetRopeNodeSpacing() const;
    float GetRopeGravityScale() const;
    float GetRopeDamping() const;
    int32 GetRopeConstraintIterations() const;
    float GetRopeCollisionRadius() const;
    float GetRopeSleepMovementThreshold() const;
    int32 GetRopeSleepFrameCount() const;

    bool bCableVisualReady = false;
    bool bCableVisualFailed = false;
    bool bCableHasBeenMoved = false;
    bool bCableStartLocationInitialized = false;
    bool bHasCachedVisualEndpoint = false;
    FVector CachedVisualEndpoint = FVector::ZeroVector;
    bool bRopeInitialized = false;
    bool bRopeSleeping = false;
    int32 RopeStableFrameCount = 0;
    float SimulatedRopeLength = 0.0f;
    TArray<FVector> RopePositions;
    TArray<FVector> RopePreviousPositions;
};
