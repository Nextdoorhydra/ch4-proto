#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"

#include "CMStageSequenceComponent.generated.h"

class ACMStageDirector;
struct FCMStageEventMessage;

USTRUCT(BlueprintType)
struct FCMStageSequenceRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName StageId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 StepOrder = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName StepId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ActionOrder = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag StartEvent;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName TargetPlacementId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag TargetGroup;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag CommandTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
    float DelaySeconds = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName PreloadGroupId;
};

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// Stage Event를 DataTable 행과 연결해 배치 액터 명령을 순서대로 실행
class CHIMERA_API UCMStageSequenceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMStageSequenceComponent();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Stage")
    TObjectPtr<UDataTable> SequenceTable;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void HandleStageEvent(FGameplayTag Channel, const FCMStageEventMessage& Message);
    void ExecuteRow(FCMStageSequenceRow Row, UObject* EventInstigator);

    TArray<FCMStageSequenceRow> OrderedRows;
    FGameplayMessageListenerHandle EventListenerHandle;
};
