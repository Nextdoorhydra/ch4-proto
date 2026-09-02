#pragma once

#include "CoreMinimal.h"
#include "AsyncLoadCompleteMessage.h"
#include "GameFramework/Actor.h"
#include "Parts/Arm/CMArmHoldTarget.h"

#include "CMPowerCableActor.generated.h"

class UCMPowerSocketComponent;
class UCMPowerSourceComponent;
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

/** Movable cable with one source endpoint and one powered socket endpoint. */
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
    bool TryConnectToSocket(
        UCMPowerSocketComponent* Socket,
        bool bIgnoreConnectionRadius = false
    );

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    bool TryConnectToSource(
        UCMPowerSourceComponent* Source,
        bool bIgnoreConnectionRadius = false
    );

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    bool TryConnectToPoweredSocket(
        UCMPowerSocketComponent* Socket,
        bool bIgnoreConnectionRadius = false
    );

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    bool TryConnectToSocketSource(
        UCMPowerSocketComponent* Socket,
        bool bIgnoreConnectionRadius = false
    );

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    void Disconnect();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    void DisconnectFromSocket();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    void DisconnectFromSource();

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool IsGrabbed() const { return Grabber != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool IsFullyConnected() const
    {
        return (ConnectedSource != nullptr || ConnectedSourceSocket != nullptr)
            && ConnectedSocket != nullptr;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool HasConnectedSource() const { return ConnectedSource != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool HasConnectedSocket() const { return ConnectedSocket != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool HasConnectedSourceSocket() const
    {
        return ConnectedSourceSocket != nullptr;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool IsTransmittingPower() const;
    bool IsTransmittingPower(TSet<const UCMPowerSocketComponent*>& VisitedSockets) const;
    bool CanConnectEndpointWithinLength(const FVector& EndpointLocation) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    FName GetPowerChannel() const { return PowerChannel; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    FVector GetCableEndLocation() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    FVector GetCableStartLocation() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    FVector GetClosestFreeEndpointLocation(const FVector& Location) const;

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
    void SetConnectedSource(UCMPowerSourceComponent* Source);
    void SetConnectedSourceSocket(UCMPowerSocketComponent* Socket);

    void NotifyPowerStateChanged();
    void NotifyPowerStateChanged(TSet<const UCMPowerSocketComponent*>& VisitedSockets);

    bool IsSourceAtStart() const { return bSourceAtStart; }
    bool IsSocketAtStart() const { return bSocketAtStart; }

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
    FName PowerChannel = TEXT("DefaultPower");

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Power|Spawn")
    bool bAutoConnectOnSpawn = false;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Power|Spawn",
        meta = (EditCondition = "bAutoConnectOnSpawn", FormerlySerializedAs = "SpawnConnectionActorA"))
    TObjectPtr<AActor> InputActor;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Power|Spawn",
        meta = (EditCondition = "bAutoConnectOnSpawn", FormerlySerializedAs = "SpawnConnectionActorB"))
    TObjectPtr<AActor> OutputActor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Override")
    bool bOverrideDefinitionSettings = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Override",
        meta = (EditCondition = "bOverrideDefinitionSettings"))
    int32 OverrideVisualSegmentCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Override",
        meta = (EditCondition = "bOverrideDefinitionSettings"))
    float OverrideCableThicknessScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Override",
        meta = (EditCondition = "bOverrideDefinitionSettings"))
    float OverrideInitialCableLength = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Override",
        meta = (EditCondition = "bOverrideDefinitionSettings"))
    float OverrideRopeNodeSpacing = 35.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Override",
        meta = (EditCondition = "bOverrideDefinitionSettings"))
    float OverrideRopeGravityScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Override",
        meta = (EditCondition = "bOverrideDefinitionSettings"))
    float OverrideRopeDamping = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Override",
        meta = (EditCondition = "bOverrideDefinitionSettings"))
    int32 OverrideRopeConstraintIterations = 8;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Override",
        meta = (EditCondition = "bOverrideDefinitionSettings"))
    float OverrideRopeCollisionRadius = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Override",
        meta = (EditCondition = "bOverrideDefinitionSettings"))
    float OverrideInitialCoilRadius = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Override",
        meta = (EditCondition = "bOverrideDefinitionSettings"))
    bool bOverrideStartCoiled = true;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Power")
    TObjectPtr<AActor> Grabber;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Power")
    bool bGrabAtStart = false;

    UPROPERTY(ReplicatedUsing = OnRep_ConnectedSocket, VisibleInstanceOnly,
        BlueprintReadOnly, Category = "Chimera|Power")
    TObjectPtr<UCMPowerSocketComponent> ConnectedSocket;

    UPROPERTY(ReplicatedUsing = OnRep_EndpointOrientation, VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Chimera|Power")
    bool bSocketAtStart = false;

    UPROPERTY(ReplicatedUsing = OnRep_ConnectedSource, VisibleInstanceOnly,
        BlueprintReadOnly, Category = "Chimera|Power")
    TObjectPtr<UCMPowerSourceComponent> ConnectedSource;

    UPROPERTY(ReplicatedUsing = OnRep_EndpointOrientation, VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Chimera|Power")
    bool bSourceAtStart = true;

    UPROPERTY(ReplicatedUsing = OnRep_ConnectedSourceSocket,
        VisibleInstanceOnly, BlueprintReadOnly, Category = "Chimera|Power")
    TObjectPtr<UCMPowerSocketComponent> ConnectedSourceSocket;

    UPROPERTY(ReplicatedUsing = OnRep_CableStartLocation,
        VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Power")
    FVector CableStartLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USplineMeshComponent>> CableMeshes;

    UFUNCTION()
    void OnRep_ConnectedSocket();

    UFUNCTION()
    void OnRep_ConnectedSource();

    UFUNCTION()
    void OnRep_ConnectedSourceSocket();

    UFUNCTION()
    void OnRep_EndpointOrientation();

    bool HasAnyEndpointConnected() const
    {
        return ConnectedSource != nullptr
            || ConnectedSourceSocket != nullptr
            || ConnectedSocket != nullptr;
    }

    FVector GetRopeStartTarget() const;
    FVector GetRopeEndTarget() const;
    bool IsRopeEndFixed() const;

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
    void ResolveRopeGroundContact(bool bEndIsFixed);
    void WakeRopeSimulation();
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
    float GetInitialCoilRadius() const;
    bool ShouldStartCoiled() const;

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
    TArray<FVector> RopeConstraintStartPositions;
    TArray<uint8> CollisionLockedNodes;
};
