#pragma once

#include "CoreMinimal.h"
#include "AsyncLoadCompleteMessage.h"
#include "GameFramework/Actor.h"

#include "CMPowerCableActor.generated.h"

class UCMPowerSocketComponent;
class UCMPowerCableDefinition;
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
class CHIMERA_API ACMPowerCableActor : public AActor
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Visual")
    TSoftObjectPtr<UCMPowerCableDefinition> CableDefinition;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Visual")
    FName LoadGroupId = TEXT("Stage.Entry.Power");

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Visual", meta = (ClampMin = "1"))
    int32 VisualSegmentCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Visual", meta = (ClampMin = "0.0"))
    float CableSag = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Visual", meta = (ClampMin = "0.01"))
    float CableThicknessScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Power|Visual", meta = (ClampMin = "0.0"))
    float InitialCableLength = 100.0f;

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

    bool bCableVisualReady = false;
    bool bCableVisualFailed = false;
    bool bCableHasBeenMoved = false;
    bool bCableStartLocationInitialized = false;
    bool bHasCachedVisualEndpoint = false;
    FVector CachedVisualEndpoint = FVector::ZeroVector;
};
