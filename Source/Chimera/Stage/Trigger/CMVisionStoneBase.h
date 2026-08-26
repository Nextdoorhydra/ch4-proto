#pragma once

#include "CoreMinimal.h"
#include "Stage/Trigger/CMStageTriggerBase.h"

#include "CMVisionStoneBase.generated.h"

class USceneComponent;

UENUM(BlueprintType)
enum class ECMVisionStoneMode : uint8
{
    RequireWatchingPlayers,
    RequireNoWatchingPlayers
};

UCLASS(Blueprintable)
class CHIMERA_API ACMVisionStoneBase : public ACMStageTriggerBase
{
    GENERATED_BODY()

public:
    ACMVisionStoneBase();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Vision Stone")
    TObjectPtr<USceneComponent> VisionPoint;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Vision Stone")
    ECMVisionStoneMode VisionStoneMode =
        ECMVisionStoneMode::RequireWatchingPlayers;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Vision Stone",
        meta = (ClampMin = "1"))
    int32 RequiredWatchingPlayers = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Vision Stone",
        meta = (ClampMin = "0.02"))
    float EvaluationInterval = 0.1f;

    UPROPERTY(BlueprintReadOnly, Category = "Chimera|Vision Stone")
    int32 WatchingPlayerCount = 0;

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Vision Stone")
    void OnVisionStoneStateChanged(
        bool bIsActive,
        int32 InWatchingPlayerCount);

private:
    UFUNCTION()
    void HandleVisionStoneActivated(AActor* TriggeringActor);

    UFUNCTION()
    void HandleVisionStoneDeactivated(AActor* TriggeringActor);

    void EvaluateVisionCondition();

    FTimerHandle EvaluationTimerHandle;
};
