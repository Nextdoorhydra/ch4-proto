#pragma once

#include "CoreMinimal.h"
#include "Stage/Trigger/CMStageButtonBase.h"

#include "CMPressurePlateBase.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

UCLASS(Blueprintable)
// 영역 안의 데이터 무게 합계가 기준을 넘으면 눌리고 낮아지면 해제되는 감압판
class CHIMERA_API ACMPressurePlateBase : public ACMStageButtonBase
{
    GENERATED_BODY()

public:
    ACMPressurePlateBase();

    UFUNCTION(BlueprintPure, Category = "Chimera|Mechanism|Pressure Plate")
    float GetCurrentWeight() const { return CurrentWeight; }

protected:
    virtual void BeginPlay() override;
    virtual void HandleElementReset_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Pressure Plate")
    TObjectPtr<UBoxComponent> PressureVolume;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Pressure Plate",
        meta = (ClampMin = "0.0"))
    float RequiredWeight = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Pressure Plate",
        meta = (ClampMin = "0.0"))
    float ReleaseWeight = 90.0f;

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Pressure Plate")
    void OnPressureChanged(float NewWeight, bool bIsPressed);

private:
    UFUNCTION()
    void HandlePressureBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandlePressureEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);

    void RecalculatePressure(AActor* ChangedActor);
    float ResolveMechanismWeight(const AActor* Actor) const;

    TMap<TWeakObjectPtr<AActor>, int32> OverlapCounts;
    float CurrentWeight = 0.0f;
};
