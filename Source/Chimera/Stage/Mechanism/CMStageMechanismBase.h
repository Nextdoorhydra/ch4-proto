#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "CMStageMechanismBase.generated.h"

class USceneComponent;
class UCMStageElementComponent;

UCLASS(Abstract, Blueprintable)
// 문과 버튼 등 스테이지 장치의 공통 활성화, 비활성화, 초기화 흐름 담당
class CHIMERA_API ACMStageMechanismBase : public AActor
{
    GENERATED_BODY()

public:
    ACMStageMechanismBase();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 장치를 활성 상태로 전환
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism")
    void ActivateMechanism();

    // 장치를 비활성 상태로 전환
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism")
    void DeactivateMechanism();

    // 장치를 레벨 시작 상태로 되돌림
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism")
    void ResetMechanism();

    UFUNCTION(BlueprintPure, Category = "Chimera|Mechanism")
    bool IsMechanismActive() const { return bMechanismActive; }

protected:
    virtual void BeginPlay() override;

    // 활성 상태 변경을 구체적인 장치 동작으로 확장
    UFUNCTION(BlueprintNativeEvent, Category = "Chimera|Mechanism")
    void HandleMechanismActiveChanged(bool bIsActive);
    virtual void HandleMechanismActiveChanged_Implementation(bool bIsActive);

    // 초기화 요청을 구체적인 장치 동작으로 확장
    UFUNCTION(BlueprintNativeEvent, Category = "Chimera|Mechanism")
    void HandleMechanismReset();
    virtual void HandleMechanismReset_Implementation();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Stage")
    TObjectPtr<UCMStageElementComponent> StageElement;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism")
    bool bStartActive = true;

private:
    UFUNCTION()
    void HandleStageCommand(FGameplayTag CommandTag, UObject* CommandInstigator);

    UFUNCTION()
    void OnRep_MechanismActive();

    void SetMechanismActive(bool bNewActive);

    UPROPERTY(ReplicatedUsing = OnRep_MechanismActive)
    bool bMechanismActive = true;
};
