#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "CMStageElementBase.generated.h"

class USceneComponent;
class UCMStageElementComponent;
class UCMPowerSocketComponent;

UCLASS(Abstract, Blueprintable)
// 스테이지 요소의 공통 활성화, 비활성화, 초기화, 네트워크 복제 흐름 담당
class CHIMERA_API ACMStageElementBase : public AActor
{
    GENERATED_BODY()

public:
    ACMStageElementBase();
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 요소를 작동 요청 상태로 전환
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage|Element")
    void ActivateElement();

    // 요소의 작동 요청을 해제
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage|Element")
    void DeactivateElement();

    // 요소의 현재 작동 요청 상태를 반대로 전환
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage|Element")
    void ToggleElement();

    // 요소를 레벨 시작 상태로 복원
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage|Element")
    void ResetElement();

    UFUNCTION(BlueprintPure, Category = "Chimera|Stage|Element")
    bool IsElementActive() const { return bElementActive; }

protected:
    virtual void BeginPlay() override;

    // Definition 준비 등 하위 요소의 추가 활성 조건 검사
    virtual bool CanActivateElement() const;

    // 추가 활성 조건이 바뀐 하위 요소가 실제 상태를 다시 계산할 때 호출
    void RefreshElementActiveState();

    // Activate 요청 자체에 반응해야 하는 단발 장치용 훅
    virtual void HandleElementActivationRequested() {}

    // 활성 상태 변경을 하위 C++와 블루프린트 표현에 전달
    UFUNCTION(BlueprintNativeEvent, Category = "Chimera|Stage|Element")
    void HandleElementActiveChanged(bool bIsActive);
    virtual void HandleElementActiveChanged_Implementation(bool bIsActive);

    // 초기화 요청을 하위 C++와 블루프린트 표현에 전달
    UFUNCTION(BlueprintNativeEvent, Category = "Chimera|Stage|Element")
    void HandleElementReset();
    virtual void HandleElementReset_Implementation();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UCMStageElementComponent> StageElement;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Stage|Element")
    bool bStartActive = true;

    // Optional power gate. Unassigned means existing behavior is preserved.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Power")
    TObjectPtr<UCMPowerSocketComponent> PowerSocket;

private:
    UFUNCTION()
    void HandleStageCommand(FGameplayTag CommandTag, UObject* CommandInstigator);

    UFUNCTION()
    void HandlePowerStateChanged(bool bPowered);

    UFUNCTION()
    void OnRep_ElementActive();

    void SetElementActive(bool bNewActive);

    UPROPERTY(ReplicatedUsing = OnRep_ElementActive)
    bool bElementActive = false;

    bool bActivationRequested = false;
};
