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

USTRUCT(BlueprintType)
struct CHIMERA_API FCMVisionStonePresentationState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bReady = false;
    UPROPERTY(BlueprintReadOnly) bool bConditionMet = false;
    UPROPERTY(BlueprintReadOnly) ECMVisionStoneMode Mode =
        ECMVisionStoneMode::RequireWatchingPlayers;
    UPROPERTY(BlueprintReadOnly) int32 WatchingPlayerCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 RequiredWatchingPlayers = 1;

    bool operator==(const FCMVisionStonePresentationState& Other) const
    {
        return bReady == Other.bReady
            && bConditionMet == Other.bConditionMet
            && Mode == Other.Mode
            && WatchingPlayerCount == Other.WatchingPlayerCount
            && RequiredWatchingPlayers == Other.RequiredWatchingPlayers;
    }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMVisionStonePresentationSignature,
    const FCMVisionStonePresentationState&,
    State);

UCLASS(Blueprintable)
class CHIMERA_API ACMVisionStoneBase : public ACMStageTriggerBase
{
    GENERATED_BODY()

public:
    ACMVisionStoneBase();
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision Stone|Presentation")
    FCMVisionStonePresentationState GetVisionPresentationState() const
    {
        return VisionPresentationState;
    }

    UPROPERTY(BlueprintAssignable,
        Category = "Chimera|Vision Stone|Presentation")
    FCMVisionStonePresentationSignature OnVisionPresentationStateChanged;

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
    void OnRep_VisionPresentationState();

    UFUNCTION()
    void HandleVisionStoneActivated(AActor* TriggeringActor);

    UFUNCTION()
    void HandleVisionStoneDeactivated(AActor* TriggeringActor);

    void EvaluateVisionCondition();
    void RefreshVisionPresentationState(bool bConditionMet);

    UPROPERTY(ReplicatedUsing = OnRep_VisionPresentationState)
    FCMVisionStonePresentationState VisionPresentationState;

    FTimerHandle EvaluationTimerHandle;
};
