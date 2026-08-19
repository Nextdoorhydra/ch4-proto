#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMClientStageLoadComponent.generated.h"

class ACMPlayGameState;
class UCMStageLoadCoordinatorSubsystem;
struct FCMStageLoadRequest;

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 로컬 PlayerController의 복제 요청 구독과 AsyncLoad 실행 담당
class CHIMERA_API UCMClientStageLoadComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMClientStageLoadComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    void TryBindPlayGameState();

    UFUNCTION()
    void HandleStageLoadRequestChanged(const FCMStageLoadRequest& Request);

    UFUNCTION()
    void HandleStageStartRequiredFinished(FGuid RequestId, bool bSucceeded);

    UPROPERTY(Transient)
    TObjectPtr<ACMPlayGameState> BoundPlayGameState;

    UPROPERTY(Transient)
    TObjectPtr<UCMStageLoadCoordinatorSubsystem> StageLoadCoordinator;

    FGuid LastHandledRequestId;
};
