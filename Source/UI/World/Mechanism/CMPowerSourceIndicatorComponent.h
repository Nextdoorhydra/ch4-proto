#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"

#include "CMPowerSourceIndicatorComponent.generated.h"

class UCMPowerSourceComponent;
class UCMPowerSocketComponent;
class UNiagaraSystem;

/** Displays connection and powered state at a power source or socket endpoint. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class UI_API UCMPowerSourceIndicatorComponent : public UWidgetComponent
{
    GENERATED_BODY()

public:
    UCMPowerSourceIndicatorComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source|View",
        meta = (ClampMin = "0.0", Units = "cm"))
    float FadeStartDistance = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source|View",
        meta = (ClampMin = "0.0", Units = "cm"))
    float FadeEndDistance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source|View",
        meta = (ClampMin = "0.02", Units = "s"))
    float DistanceUpdateInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source|Effects")
    TSoftObjectPtr<UNiagaraSystem> ConnectionBurstEffect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|World UI|Power Source|Effects",
        meta = (ClampMin = "0.0", Units = "cm"))
    float ConnectionBurstOffset = 20.0f;

private:
    UFUNCTION()
    void HandleSourceStateChanged(bool bState);

    UCMPowerSourceComponent* ResolvePowerSource() const;
    UCMPowerSocketComponent* ResolvePowerSocket() const;
    void ApplySourceState(bool bAnimate);
    void SpawnConnectionBurst();
    void UpdateDistanceFade();

    TWeakObjectPtr<UCMPowerSourceComponent> PowerSource;
    TWeakObjectPtr<UCMPowerSocketComponent> PowerSocket;
    float DistanceUpdateElapsed = 0.0f;
    bool bHasAppliedState = false;
    bool bLastPoweredConnection = false;
    bool bInsideFadeRange = false;
    FTimerHandle ConnectionBurstTimerHandle;
};
