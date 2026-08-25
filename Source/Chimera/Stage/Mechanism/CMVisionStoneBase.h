#pragma once

#include "CoreMinimal.h"
#include "Stage/Mechanism/CMStageMechanismBase.h"

#include "CMVisionStoneBase.generated.h"

class USceneComponent;

UENUM(BlueprintType)
enum class ECMVisionStoneMode : uint8
{
    RequireWatchingPlayers,
    RequireNoWatchingPlayers
};

UCLASS(Blueprintable)
class CHIMERA_API ACMVisionStoneBase : public ACMStageMechanismBase
{
    GENERATED_BODY()

public:
    ACMVisionStoneBase();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void HandleMechanismActiveChanged_Implementation(
        bool bIsActive) override;

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

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Vision Stone|Target")
    FName TargetPlacementId;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Vision Stone|Target")
    FGameplayTag TargetGroup;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Vision Stone|Target")
    FGameplayTag TargetCommandTag;

    UPROPERTY(BlueprintReadOnly, Category = "Chimera|Vision Stone")
    int32 WatchingPlayerCount = 0;

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Vision Stone")
    void OnVisionStoneStateChanged(bool bIsActive, int32 InWatchingPlayerCount);

private:
    void EvaluateVisionCondition();

    FTimerHandle EvaluationTimerHandle;
};
