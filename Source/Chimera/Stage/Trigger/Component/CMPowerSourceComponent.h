#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "CMPowerSourceComponent.generated.h"

class ACMPowerCableActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMPowerSourceConnectionChanged,
    bool,
    bConnected
);

/** Output connection for an actor that supplies power to a cable. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMPowerSourceComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UCMPowerSourceComponent();

    bool TryConnectCable(ACMPowerCableActor* Cable);
    void DisconnectCable(ACMPowerCableActor* Cable);

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool IsConnected() const { return ConnectedCable != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    ACMPowerCableActor* GetConnectedCable() const { return ConnectedCable; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    FName GetPowerChannel() const { return PowerChannel; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    float GetConnectionRadius() const { return ConnectionRadius; }

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Power")
    FCMPowerSourceConnectionChanged OnConnectionChanged;

protected:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power")
    FName PowerChannel = TEXT("DefaultPower");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power",
        meta = (ClampMin = "0.0"))
    float ConnectionRadius = 150.0f;

    UPROPERTY(ReplicatedUsing = OnRep_ConnectedCable, VisibleInstanceOnly,
        BlueprintReadOnly, Category = "Chimera|Power")
    TObjectPtr<ACMPowerCableActor> ConnectedCable;

    UFUNCTION()
    void OnRep_ConnectedCable();

    friend class ACMPowerCableActor;
};
