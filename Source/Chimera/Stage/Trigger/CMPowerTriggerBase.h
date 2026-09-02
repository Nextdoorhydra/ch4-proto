#pragma once

#include "CoreMinimal.h"
#include "Stage/Trigger/CMStageTriggerBase.h"

#include "CMPowerTriggerBase.generated.h"

class UCMPowerSocketComponent;

/** Activates the normal stage trigger when all required sockets are powered. */
UCLASS()
class CHIMERA_API ACMPowerTriggerBase : public ACMStageTriggerBase
{
    GENERATED_BODY()

public:
    ACMPowerTriggerBase();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Power")
    TArray<TObjectPtr<UCMPowerSocketComponent>> RequiredSockets;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Power")
    bool bRequireAllSockets = true;

private:
    UFUNCTION()
    void HandleSocketConnectionChanged(bool bConnected);

    void EvaluatePowerState();

    bool bPendingInitialEvaluation = false;
};
