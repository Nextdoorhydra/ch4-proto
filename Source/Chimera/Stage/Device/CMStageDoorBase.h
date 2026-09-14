#pragma once

#include "CoreMinimal.h"
#include "Stage/Device/CMStageDeviceBase.h"

#include "CMStageDoorBase.generated.h"

class UCameraShakeBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMDoorTransitionFinishedSignature,
    bool, bIsOpen);

UCLASS(Blueprintable)
// Mechanism 활성 상태를 문의 열림과 닫힘 표현으로 연결
class CHIMERA_API ACMStageDoorBase : public ACMStageDeviceBase
{
    GENERATED_BODY()

public:
    ACMStageDoorBase();

    // 문 BP가 열림 또는 닫힘 애니메이션과 충돌 전환을 마친 시점에 호출
    UFUNCTION(BlueprintCallable, Category = "Chimera|Mechanism|Door")
    void NotifyDoorTransitionFinished(bool bIsOpen);

    /** Toggles this door while running PIE for quick presentation testing. */
    UFUNCTION(CallInEditor, Category = "Chimera|Mechanism|Door|Test")
    void TestToggleDoor();

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Mechanism|Door")
    FCMDoorTransitionFinishedSignature OnDoorTransitionFinished;

protected:
    virtual void HandleElementActiveChanged_Implementation(bool bIsActive) override;
    virtual void HandleElementReset_Implementation() override;

    // 열림 상태 변경 시 문 이동과 충돌 처리를 구현
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Door")
    void OnDoorOpenStateChanged(bool bIsOpen);

    // 초기 위치와 문 애니메이션을 레벨 시작 상태로 복원
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Door")
    void OnDoorReset();

    /** Sound played once whenever the door starts moving in either direction. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Door|Presentation")
    FGameplayTag MovementSoundTag;

    /** Optional shake played when this door starts opening. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Door|Presentation")
    TSubclassOf<UCameraShakeBase> OpeningCameraShake;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Door|Presentation",
        meta = (ClampMin = "0.0"))
    float CameraShakeInnerRadius = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Mechanism|Door|Presentation",
        meta = (ClampMin = "0.0"))
    float CameraShakeOuterRadius = 2500.0f;

private:
    bool bHasObservedOpenState = false;
    bool bLastObservedOpenState = false;
};
