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

protected:
    virtual void BeginPlay() override;
    virtual void HandleMechanismActiveChanged_Implementation(bool bIsActive) override;
    virtual void HandleMechanismReset_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Button")
    TObjectPtr<UCMActivationTriggerComponent> ActivationTrigger;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Mechanism|Button|Target")
    FName TargetPlacementId;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Mechanism|Button|Target")
    FGameplayTag TargetGroup;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Button|Target")
    FGameplayTag TargetCommandTag;

    // 버튼 작동 표현을 C++ 또는 블루프린트에서 구현
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Button")
    void OnButtonPressed(AActor* PressingActor);

private:
    UFUNCTION()
    void HandleTriggerActivated(AActor* TriggeringActor);
};
