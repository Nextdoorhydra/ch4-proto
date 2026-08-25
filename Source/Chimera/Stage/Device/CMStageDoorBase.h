#pragma once

#include "CoreMinimal.h"
#include "Stage/Device/CMStageDeviceBase.h"

#include "CMStageDoorBase.generated.h"

UCLASS(Blueprintable)
// Mechanism 활성 상태를 문의 열림과 닫힘 표현으로 연결
class CHIMERA_API ACMStageDoorBase : public ACMStageDeviceBase
{
    GENERATED_BODY()

public:
    ACMStageDoorBase();

protected:
    virtual void HandleElementActiveChanged_Implementation(bool bIsActive) override;
    virtual void HandleElementReset_Implementation() override;

    // 열림 상태 변경 시 문 이동과 충돌 처리를 구현
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Door")
    void OnDoorOpenStateChanged(bool bIsOpen);

    // 초기 위치와 문 애니메이션을 레벨 시작 상태로 복원
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Door")
    void OnDoorReset();
};
