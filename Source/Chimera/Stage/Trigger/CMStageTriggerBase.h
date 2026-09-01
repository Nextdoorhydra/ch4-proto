#pragma once

#include "CoreMinimal.h"
#include "Stage/CMStageElementBase.h"
#include "Stage/Trigger/CMTriggerPresentationState.h"

#include "CMStageTriggerBase.generated.h"

class UCMActivationTriggerComponent;
class ACMStageTriggerBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMTriggerPresentationSignature, const FCMTriggerPresentationState&, State);

UENUM(BlueprintType)
enum class ECMStageTriggerSignal : uint8
{
    Pulse,       // 버튼처럼 누를 때마다 발생하는 순간 입력
    Activated,   // 압력판이나 레버의 조건이 충족된 상태
    Deactivated  // 유지 조건이 해제된 상태
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FCMStageTriggerSignalSignature,
    ACMStageTriggerBase*, Trigger,
    ECMStageTriggerSignal, Signal);

UCLASS(Abstract, Blueprintable)
// 조건 충족과 해제를 StageDirector 대상 명령으로 변환하는 공통 트리거
class CHIMERA_API ACMStageTriggerBase : public ACMStageElementBase
{
    GENERATED_BODY()

public:
    ACMStageTriggerBase();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Trigger|Presentation")
    FCMTriggerPresentationState GetPresentationState() const { return PresentationState; }

    // Server updates and client replication both notify this read-only UI event.
    UPROPERTY(BlueprintAssignable, Category = "Chimera|Trigger|Presentation")
    FCMTriggerPresentationSignature OnPresentationStateChanged;

    // 하위 트리거가 조건을 충족했을 때 서버에서 호출
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism|Trigger")
    bool ActivateTrigger(AActor* TriggeringActor);

    // 유지형 트리거의 조건이 해제되었을 때 서버에서 호출
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Mechanism|Trigger")
    bool DeactivateTrigger(AActor* TriggeringActor);

    // PuzzleController 등록 시 기존 TargetActor/Group 명령의 중복 실행을 차단
    void SetDirectTargetCommandEnabled(bool bEnabled);

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Mechanism|Trigger")
    FCMStageTriggerSignalSignature OnTriggerSignal;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void FillPresentationState(FCMTriggerPresentationState& State) const;
    void RefreshPresentationState();
    virtual void HandleElementActiveChanged_Implementation(bool bIsActive) override;
    virtual void HandleElementReset_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMActivationTriggerComponent> ActivationTrigger;

    // 에디터에서 제어할 액터를 직접 선택하면 해당 StageElement ID를 자동 사용
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera Trigger")
    TObjectPtr<AActor> TargetActor;

    // TargetActor를 사용하지 않을 때 직접 지정하는 호환용 대상 ID
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera Trigger")
    FName TargetPlacementId;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera Trigger")
    FGameplayTag TargetGroup;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera Trigger")
    FGameplayTag TargetCommandTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera Trigger")
    FGameplayTag ReleaseCommandTag;

    // 트리거 표현을 하위 C++ 또는 블루프린트에서 구현
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Trigger")
    void OnTriggerActivated(AActor* TriggeringActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Trigger")
    void OnTriggerDeactivated(AActor* TriggeringActor);

    // 기본 트리거는 상태 신호, 일반 버튼은 양쪽 상태 변화를 Pulse로 변환
    virtual ECMStageTriggerSignal ResolveTriggerSignal(bool bActivated) const;

private:
    UFUNCTION()
    void OnRep_PresentationState();

    UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
    FCMTriggerPresentationState PresentationState;

    UFUNCTION()
    void HandleTriggerActivated(AActor* TriggeringActor);

    UFUNCTION()
    void HandleTriggerDeactivated(AActor* TriggeringActor);

    FName ResolveTargetPlacementId() const;

    bool bDirectTargetCommandEnabled = true;
};
