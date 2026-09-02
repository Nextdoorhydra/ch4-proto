#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "CMPowerSubsystem.generated.h"

class UCMPowerSocketComponent;
class UCMPowerSourceComponent;

/** Runtime registry for power endpoints in the current world. */
UCLASS()
class CHIMERA_API UCMPowerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    void RegisterSocket(UCMPowerSocketComponent* Socket);
    void UnregisterSocket(UCMPowerSocketComponent* Socket);
    void RegisterSource(UCMPowerSourceComponent* Source);
    void UnregisterSource(UCMPowerSourceComponent* Source);
    void RefreshPowerState();

    const TArray<TWeakObjectPtr<UCMPowerSocketComponent>>& GetSockets()
    {
        CleanupInvalidEntries();
        return Sockets;
    }

    const TArray<TWeakObjectPtr<UCMPowerSourceComponent>>& GetSources()
    {
        CleanupInvalidEntries();
        return Sources;
    }

private:
    void CleanupInvalidEntries();

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<UCMPowerSocketComponent>> Sockets;

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<UCMPowerSourceComponent>> Sources;
};
