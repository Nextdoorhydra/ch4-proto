#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "CMPowerSocketComponent.generated.h"

class ACMPowerCableActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMPowerSocketConnectionChanged,
    bool,
    bConnected
);

/** A server-authoritative socket that accepts one matching power cable. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMPowerSocketComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UCMPowerSocketComponent();

    bool TryConnectCable(ACMPowerCableActor* Cable);
    void DisconnectCable(ACMPowerCableActor* Cable);

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool IsConnected() const { return ConnectedCable != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    ACMPowerCableActor* GetConnectedCable() const
    {
        return ConnectedCable;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    FName GetPowerChannel() const { return PowerChannel; }

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Power")
    FCMPowerSocketConnectionChanged OnConnectionChanged;

protected:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power")
    FName PowerChannel = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power",
        meta = (ClampMin = "0.0"))
    float ConnectionRadius = 75.0f;

    UPROPERTY(ReplicatedUsing = OnRep_ConnectedCable, VisibleInstanceOnly,
        BlueprintReadOnly, Category = "Chimera|Power")
    TObjectPtr<ACMPowerCableActor> ConnectedCable;

    UFUNCTION()
    void OnRep_ConnectedCable();

    friend class ACMPowerCableActor;
    friend class ACMPowerTriggerBase;
};
