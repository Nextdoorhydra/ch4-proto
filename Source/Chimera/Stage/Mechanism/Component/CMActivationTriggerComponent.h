#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMActivationTriggerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMActivationTriggerSignature, AActor*, TriggeringActor);

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 버튼 상호작용 요청의 사용 가능 여부와 일회성 작동 상태 관리
class CHIMERA_API UCMActivationTriggerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMActivationTriggerComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 서버에서 버튼 또는 상호작용 장치 작동 시도
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism|Trigger")
    bool TryActivate(AActor* TriggeringActor);

    // 작동 이력을 지우고 다시 사용할 수 있는 상태로 복원
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism|Trigger")
    void ResetTrigger();

    // 장치 활성 상태에 따라 새로운 작동 요청 허용 여부 변경
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism|Trigger")
    void SetTriggerEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Chimera|Mechanism|Trigger")
    bool CanActivate() const { return bTriggerEnabled && (!bOneShot || !bHasTriggered); }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Trigger")
    bool bOneShot = true;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Mechanism|Trigger")
    FCMActivationTriggerSignature OnActivated;

protected:
    // 복제된 작동 상태를 버튼 애니메이션 등에 전달
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Trigger")
    void OnTriggerStateChanged(bool bIsEnabled, bool bWasTriggered);

private:
    UFUNCTION()
    void OnRep_TriggerState();

    UPROPERTY(ReplicatedUsing = OnRep_TriggerState)
    bool bTriggerEnabled = true;

    UPROPERTY(ReplicatedUsing = OnRep_TriggerState)
    bool bHasTriggered = false;
};
