#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMStageDestination.generated.h"

class ACMChimera;
class ACMStageDirector;
class UBoxComponent;
class UPrimitiveComponent;

UCLASS()
// 활성 몸통 마디 하나 이상이 영역에 들어오면 현재 스테이지 완료 보고
class CHIMERA_API ACMStageDestination : public AActor
{
    GENERATED_BODY()

public:
    ACMStageDestination();

protected:
    virtual void BeginPlay() override;

private:
    // 몸통 컴포넌트가 영역에 들어올 때 활성 마디 겹침 여부 재검사
    UFUNCTION()
    void HandleDestinationBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    // 현재 공용 키메라와 StageDirector를 찾아 완료 조건을 한 번만 보고
    void TryCompleteStage(ACMChimera* Chimera);

    // 현재 월드에 배치된 유일한 StageDirector 검색
    ACMStageDirector* FindStageDirector() const;

    UPROPERTY(VisibleAnywhere, Category = "Chimera|Stage")
    TObjectPtr<UBoxComponent> DestinationVolume;

    UPROPERTY(Transient)
    TObjectPtr<ACMStageDirector> StageDirector;

    bool bCompletionReported = false;
};
