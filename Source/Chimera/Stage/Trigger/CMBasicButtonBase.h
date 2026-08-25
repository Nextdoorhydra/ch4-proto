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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button")
    TObjectPtr<UBoxComponent> HitVolume;

    // 타격할 때마다 눌림과 해제를 번갈아 실행해 대상 명령을 토글
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Basic Button")
    bool bToggleOnHit = false;

private:
    UFUNCTION()
    void HandleHitVolumeBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);
};
