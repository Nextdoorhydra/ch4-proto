#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMRoomEntryTrigger.generated.h"

class ACMChimera;
class ACMRoomStreamingController;
class ACMStageDoorBase;
class UBoxComponent;
class UPrimitiveComponent;

UCLASS(Blueprintable)
// 활성 키메라 몸통 하나 이상이 영역에 들어오면 문 상태와 무관하게 룸 진입 확정
class CHIMERA_API ACMRoomEntryTrigger : public AActor
{
    GENERATED_BODY()

public:
    ACMRoomEntryTrigger();

    UFUNCTION(BlueprintPure, Category = "Chimera|Stage|Room Streaming")
    FName GetRoomId() const { return RoomId; }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(
        FDataValidationContext& Context) const override;
#endif

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Stage|Room Streaming")
    TObjectPtr<UBoxComponent> EntryVolume;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Stage|Room Streaming")
    FName RoomId;

    // 선택 사항: 진입 시 닫기 명령을 보낼 문. 닫힘 완료는 기다리지 않음
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Stage|Room Streaming")
    TObjectPtr<ACMStageDoorBase> EntryBlockerDoor;

private:
    UFUNCTION()
    void HandleBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    void TryCommitForChimera(ACMChimera* Chimera);
    void CommitRoom();

    TWeakObjectPtr<ACMRoomStreamingController> StreamingController;
    bool bCommitStarted = false;
    bool bCommitFinished = false;
};
