#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "CMStageElementComponent.generated.h"

class ACMStageDirector;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FCMStageCommandReceivedSignature,
    FGameplayTag, CommandTag,
    UObject*, CommandInstigator);

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

    // PuzzleController 직접 참조만 사용하면 비워도 된다.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Stage",
        meta = (ToolTip = "Optional. Required only when addressed by StageDirector, Trigger TargetActor, or Sequence TargetPlacementId."))
    FName PlacementId;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Stage",
        meta = (ToolTip = "Optional. Add tags only when this element must receive StageDirector group commands."))
    FGameplayTagContainer GroupTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Stage")
    ECMStageCommandExecutionPolicy ExecutionPolicy = ECMStageCommandExecutionPolicy::AuthorityOnly;

    UFUNCTION(BlueprintNativeEvent, Category = "Chimera|Stage")
    void ExecuteStageCommand(FGameplayTag CommandTag, UObject* CommandInstigator);

    // StageDirector에서 전달된 명령을 소유 액터와 블루프린트에 알림
    UPROPERTY(BlueprintAssignable, Category = "Chimera|Stage")
    FCMStageCommandReceivedSignature OnStageCommandReceived;

    // 장애물 동작 완료 신호를 현재 StageDirector에 전달
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void BroadcastStageEvent(FGameplayTag EventTag);

    // 같은 StageDirector에 등록된 PlacementId 또는 GroupTag 대상으로 명령 전달
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void RequestStageCommand(
        FName TargetPlacementId,
        FGameplayTag TargetGroup,
        FGameplayTag CommandTag);

    bool CanExecuteOnCurrentMachine() const;

    ACMStageDirector* GetRegisteredDirector() const
    {
        return RegisteredDirector.Get();
    }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<ACMStageDirector> RegisteredDirector;
};
