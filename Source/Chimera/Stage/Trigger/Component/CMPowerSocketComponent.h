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

    bool TryConnectCable(
        ACMPowerCableActor* Cable,
        bool bIgnoreConnectionRadius = false
    );
    void DisconnectCable(ACMPowerCableActor* Cable);

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool IsPhysicallyConnected() const { return !ConnectedCables.IsEmpty(); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool IsPowered() const;
    bool IsPowered(TSet<const UCMPowerSocketComponent*>& VisitedSockets) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    int32 GetConnectedCableCount() const { return ConnectedCables.Num(); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool CanDisconnectCable() const { return bAllowCableDisconnect; }

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    bool CanProvidePower() const { return IsPowered(); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    ACMPowerCableActor* GetConnectedCable() const
    {
        return ConnectedCables.IsEmpty() ? nullptr : ConnectedCables[0];
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    FName GetPowerChannel() const { return PowerChannel; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    float GetConnectionRadius() const { return ConnectionRadius; }

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Power")
    FCMPowerSocketConnectionChanged OnConnectionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Power")
    FCMPowerSocketConnectionChanged OnPowerStateChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power")
    FName PowerChannel = TEXT("DefaultPower");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power",
        meta = (ClampMin = "0.0"))
    float ConnectionRadius = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power",
        meta = (ClampMin = "1"))
    int32 MaxConnectedCables = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power")
    bool bAllowCableDisconnect = true;

    UPROPERTY(ReplicatedUsing = OnRep_ConnectedCables, VisibleInstanceOnly,
        BlueprintReadOnly, Category = "Chimera|Power")
    TArray<TObjectPtr<ACMPowerCableActor>> ConnectedCables;

    UFUNCTION()
    void OnRep_ConnectedCables();

    void NotifyPowerStateChanged();
    void NotifyPowerStateChanged(TSet<const UCMPowerSocketComponent*>& VisitedSockets);

    void AddPowerOutputCable(ACMPowerCableActor* Cable);
    void RemovePowerOutputCable(ACMPowerCableActor* Cable);

    UPROPERTY(Transient)
    TArray<TObjectPtr<ACMPowerCableActor>> PowerOutputCables;

    friend class ACMPowerCableActor;
    friend class ACMPowerTriggerBase;
    friend class UCMPowerSubsystem;
};
