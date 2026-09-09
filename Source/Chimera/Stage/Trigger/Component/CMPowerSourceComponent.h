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

    bool TryConnectCable(
        ACMPowerCableActor* Cable,
        bool bIgnoreConnectionRadius = false
    );
    void DisconnectCable(ACMPowerCableActor* Cable);

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool IsPhysicallyConnected() const { return !ConnectedCables.IsEmpty(); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool IsProvidingPower() const { return bPowerEnabled; }

    UFUNCTION(BlueprintCallable, Category = "Chimera|Power")
    void SetPowerEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    ACMPowerCableActor* GetConnectedCable() const
    {
        return ConnectedCables.IsEmpty() ? nullptr : ConnectedCables[0];
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    int32 GetConnectedCableCount() const { return ConnectedCables.Num(); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    bool CanDisconnectCable() const { return bAllowCableDisconnect; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    FName GetPowerChannel() const { return PowerChannel; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Power")
    float GetConnectionRadius() const { return ConnectionRadius; }

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Power")
    FCMPowerSourceConnectionChanged OnConnectionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Power")
    FCMPowerSourceConnectionChanged OnPowerStateChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power")
    FName PowerChannel = TEXT("DefaultPower");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power",
        meta = (ClampMin = "1"))
    int32 MaxConnectedCables = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power")
    bool bAllowCableDisconnect = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Power",
        meta = (ClampMin = "0.0"))
    float ConnectionRadius = 300.0f;

    UPROPERTY(ReplicatedUsing = OnRep_ConnectedCables, VisibleInstanceOnly,
        BlueprintReadOnly, Category = "Chimera|Power")
    TArray<TObjectPtr<ACMPowerCableActor>> ConnectedCables;

    UFUNCTION()
    void OnRep_ConnectedCables();

    UFUNCTION()
    void OnRep_PowerEnabled();

    void NotifyPowerStateChanged();

    UPROPERTY(ReplicatedUsing = OnRep_PowerEnabled, EditAnywhere,
        BlueprintReadOnly, Category = "Chimera|Power")
    bool bPowerEnabled = true;

    friend class ACMPowerCableActor;
};
