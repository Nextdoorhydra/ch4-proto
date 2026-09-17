#pragma once

#include "CoreMinimal.h"
#include "Stage/Trigger/CMStageTriggerBase.h"

#include "CMStageButtonBase.generated.h"

UCLASS(Blueprintable)
// 버튼 입력을 공통 Stage Trigger의 활성과 해제로 변환
class CHIMERA_API ACMStageButtonBase : public ACMStageTriggerBase
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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 버튼 작동 표현을 C++ 또는 블루프린트에서 구현
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Button")
    void OnButtonPressed(AActor* PressingActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Button")
    void OnButtonReleased(AActor* ReleasingActor);

private:
    UFUNCTION()
    void HandleButtonActivated(AActor* TriggeringActor);

    UFUNCTION()
    void HandleButtonDeactivated(AActor* TriggeringActor);

};
