#pragma once

#include "CoreMinimal.h"
#include "Stage/CMStageElementBase.h"

#include "CMStageDeviceBase.generated.h"

UCLASS(Abstract, Blueprintable)
// 문, 다리, 엘리베이터처럼 명령을 받아 상태가 바뀌는 스테이지 장치의 공통 기반
class CHIMERA_API ACMStageDeviceBase : public ACMStageElementBase
{
    GENERATED_BODY()

public:
    // 장치를 활성 상태로 전환
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Device")
    void ActivateDevice() { ActivateElement(); }

    // 장치를 비활성 상태로 전환
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Device")
    void DeactivateDevice() { DeactivateElement(); }

    // 장치를 레벨 시작 상태로 복원
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Device")
    void ResetDevice() { ResetElement(); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Device")
    bool IsDeviceActive() const { return IsElementActive(); }
};
