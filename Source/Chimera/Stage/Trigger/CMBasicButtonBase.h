#pragma once

#include "CoreMinimal.h"
#include "Stage/Trigger/CMStageButtonBase.h"

#include "CMBasicButtonBase.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

UCLASS(Blueprintable)
// 휘두르는 중인 팔 파츠가 타격 영역에 들어오면 작동하는 기본 버튼
class CHIMERA_API ACMBasicButtonBase : public ACMStageButtonBase
{
    GENERATED_BODY()

public:
    ACMBasicButtonBase();

protected:
    virtual void BeginPlay() override;
    virtual ECMStageTriggerSignal ResolveTriggerSignal(
        bool bActivated) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button")
    TObjectPtr<UBoxComponent> HitVolume;

    // 타격할 때마다 눌림과 해제를 번갈아 실행하고 Activated/Deactivated 신호 전달
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button")
    bool bToggleOnHit = false;

#if WITH_EDITORONLY_DATA
    // 팔 Sweep 구현 전 버튼 명령 경로를 몸통 Overlap으로 확인하는 에디터 테스트 옵션
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Testing")
    bool bAllowChimeraBodyOverlapForTesting = false;
#endif

private:
    // 유효한 팔 타격과 몸통 테스트 입력에 동일한 반복/일회성 규칙 적용
    void HandleValidButtonInput(AActor* TriggeringActor);

    UFUNCTION()
    void HandleHitVolumeBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);
};
