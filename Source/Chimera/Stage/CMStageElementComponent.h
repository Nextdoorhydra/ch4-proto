#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "CMStageElementComponent.generated.h"

class ACMStageDirector;

UENUM(BlueprintType)
enum class ECMStageCommandExecutionPolicy : uint8
{
    AuthorityOnly, // 충돌·게임 규칙처럼 서버에서만 처리
    AllMachines    // 조명·사운드·연출처럼 각 머신에서 처리
};

UCLASS(Blueprintable, ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 레벨에 배치된 장애물·퍼즐·연출 액터를 StageDirector 등록소에 연결
class CHIMERA_API UCMStageElementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMStageElementComponent();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Stage")
    FName PlacementId;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Stage")
    FGameplayTagContainer GroupTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Stage")
    ECMStageCommandExecutionPolicy ExecutionPolicy = ECMStageCommandExecutionPolicy::AuthorityOnly;

    UFUNCTION(BlueprintNativeEvent, Category = "Chimera|Stage")
    void ExecuteStageCommand(FGameplayTag CommandTag, UObject* CommandInstigator);

    // 장애물 동작 완료 신호를 현재 StageDirector에 전달
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void BroadcastStageEvent(FGameplayTag EventTag);

    bool CanExecuteOnCurrentMachine() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<ACMStageDirector> RegisteredDirector;
};
