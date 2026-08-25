#pragma once

#include "CoreMinimal.h"
#include "Stage/Mechanism/CMStageMechanismBase.h"

#include "CMStageButtonBase.generated.h"

class UCMActivationTriggerComponent;

UCLASS(Blueprintable)
// 버튼 작동을 StageDirector 대상 명령으로 변환
class CHIMERA_API ACMStageButtonBase : public ACMStageMechanismBase
{
    GENERATED_BODY()

public:
    ACMStageButtonBase();

    // 상호작용 또는 Overlap 구현에서 호출하는 서버 버튼 작동 진입점
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism|Button")
    bool PressButton(AActor* PressingActor);

    // 유지형 버튼이 해제되었음을 서버 공통 트리거에 전달
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism|Button")
    bool ReleaseButton(AActor* ReleasingActor);

protected:
    virtual void BeginPlay() override;
    virtual void HandleMechanismActiveChanged_Implementation(bool bIsActive) override;
    virtual void HandleMechanismReset_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Button")
    TObjectPtr<UCMActivationTriggerComponent> ActivationTrigger;

    // 에디터에서 제어할 액터를 직접 선택하면 해당 StageElement ID를 자동 사용
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Mechanism|Button|Target")
    TObjectPtr<AActor> TargetActor;

    // TargetActor를 사용하지 않을 때 직접 지정하는 호환용 대상 ID
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Mechanism|Button|Target")
    FName TargetPlacementId;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Mechanism|Button|Target")
    FGameplayTag TargetGroup;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Button|Target")
    FGameplayTag TargetCommandTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Button|Target")
    FGameplayTag ReleaseCommandTag;

    // 버튼 작동 표현을 C++ 또는 블루프린트에서 구현
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Button")
    void OnButtonPressed(AActor* PressingActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Button")
    void OnButtonReleased(AActor* ReleasingActor);

private:
    UFUNCTION()
    void HandleTriggerActivated(AActor* TriggeringActor);

    UFUNCTION()
    void HandleTriggerDeactivated(AActor* TriggeringActor);

    FName ResolveTargetPlacementId() const;
};
