#pragma once

#include "CoreMinimal.h"
#include "Stage/Trigger/CMStageButtonBase.h"

#include "CMBasicButtonBase.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class ACMArmPart;

UCLASS(Blueprintable)
// 팔 스윙 탐지가 타격 영역을 감지하면 작동하는 기본 버튼
class CHIMERA_API ACMBasicButtonBase : public ACMStageButtonBase
{
    GENERATED_BODY()

public:
    ACMBasicButtonBase();

    // 서버의 팔 스윙 탐지에서만 호출하며 같은 공격은 버튼당 한 번만 처리
    void NotifySwingHit(ACMArmPart* ArmPart, UPrimitiveComponent* HitComponent);
    UBoxComponent* GetHitVolume() const { return HitVolume; }

protected:
    virtual void BeginPlay() override;
    virtual ECMStageTriggerSignal ResolveTriggerSignal(
        bool bActivated) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button")
    TObjectPtr<UBoxComponent> HitVolume;

    // 타격할 때마다 눌림과 해제를 번갈아 실행하고 Activated/Deactivated 신호 전달
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button")
    bool bToggleOnHit = false;

private:
    TMap<TWeakObjectPtr<ACMArmPart>, FGuid> LastSwingAttackIds;

    // 유효한 팔 타격에 반복/일회성 규칙 적용
    void HandleValidButtonInput(AActor* TriggeringActor);
};
