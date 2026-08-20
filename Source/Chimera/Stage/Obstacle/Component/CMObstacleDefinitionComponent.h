#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AsyncLoadCompleteMessage.h"

#include "CMObstacleDefinitionComponent.generated.h"

class UCMObstacleDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMObstacleDefinitionReadySignature,
    UCMObstacleDefinition*, LoadedDefinition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCMObstacleDefinitionFailedSignature);

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 배치된 장애물의 Soft Definition을 LoadGroup 완료 결과와 연결
class CHIMERA_API UCMObstacleDefinitionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMObstacleDefinitionComponent();

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle|Definition")
    bool HasDefinition() const { return !Definition.IsNull(); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle|Definition")
    bool IsDefinitionReady() const { return bDefinitionReady; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle|Definition")
    bool HasDefinitionFailed() const { return bDefinitionFailed; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle|Definition")
    UCMObstacleDefinition* GetLoadedDefinition() const;

    // 현재 LoadGroup 상태와 메모리 준비 상태를 다시 확인
    UFUNCTION(BlueprintCallable, Category = "Chimera|Obstacle|Definition")
    void RefreshDefinitionState();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Definition")
    TSoftObjectPtr<UCMObstacleDefinition> Definition;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Definition")
    FName LoadGroupId;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Obstacle|Definition")
    FCMObstacleDefinitionReadySignature OnDefinitionReady;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Obstacle|Definition")
    FCMObstacleDefinitionFailedSignature OnDefinitionFailed;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleLoadGroupFinished(
        FName FinishedLoadGroupId,
        EAsyncLoadResult Result,
        bool bReleasedImmediately);

    bool TryResolveLoadedDefinition();
    void MarkDefinitionFailed(const TCHAR* Reason);

    bool bDefinitionReady = false;
    bool bDefinitionFailed = false;
};
