#pragma once

#include "CoreMinimal.h"
#include "Stage/CMStageElementBase.h"

#include "CMStageTriggerBase.generated.h"

class UCMActivationTriggerComponent;

UCLASS(Abstract, Blueprintable)
// 조건 충족과 해제를 StageDirector 대상 명령으로 변환하는 공통 트리거
class CHIMERA_API ACMStageTriggerBase : public ACMStageElementBase
{
    GENERATED_BODY()

public:
    ACMStageTriggerBase();

    // 하위 트리거가 조건을 충족했을 때 서버에서 호출
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism|Trigger")
    bool ActivateTrigger(AActor* TriggeringActor);

    // 유지형 트리거의 조건이 해제되었을 때 서버에서 호출
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism|Trigger")
    bool DeactivateTrigger(AActor* TriggeringActor);

protected:
    virtual void BeginPlay() override;
    virtual void HandleElementActiveChanged_Implementation(bool bIsActive) override;
    virtual void HandleElementReset_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Trigger")
    TObjectPtr<UCMActivationTriggerComponent> ActivationTrigger;

    // 에디터에서 제어할 액터를 직접 선택하면 해당 StageElement ID를 자동 사용
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Mechanism|Trigger|Target")
    TObjectPtr<AActor> TargetActor;

    // TargetActor를 사용하지 않을 때 직접 지정하는 호환용 대상 ID
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Mechanism|Trigger|Target")
    FName TargetPlacementId;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Mechanism|Trigger|Target")
    FGameplayTag TargetGroup;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Trigger|Target")
    FGameplayTag TargetCommandTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Trigger|Target")
    FGameplayTag ReleaseCommandTag;

    // 트리거 표현을 하위 C++ 또는 블루프린트에서 구현
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Trigger")
    void OnTriggerActivated(AActor* TriggeringActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Trigger")
    void OnTriggerDeactivated(AActor* TriggeringActor);

private:
    UFUNCTION()
    void HandleTriggerActivated(AActor* TriggeringActor);

    UFUNCTION()
    void HandleTriggerDeactivated(AActor* TriggeringActor);

    FName ResolveTargetPlacementId() const;
};
