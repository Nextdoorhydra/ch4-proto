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
// 활성 키메라 몸통 하나 이상이 영역에 들어오고 뒤쪽 문이 닫히면 룸 진입을 확정
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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Stage|Room Streaming")
    TObjectPtr<UBoxComponent> EntryVolume;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Stage|Room Streaming")
    FName RoomId;

    // 첫 룸은 비워두고 이후 룸은 같은 서브레벨의 뒤쪽 차단 문 지정
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Stage|Room Streaming")
    TObjectPtr<ACMStageDoorBase> EntryBlockerDoor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Stage|Room Streaming")
    bool bWaitForDoorClosed = true;

private:
    UFUNCTION()
    void HandleBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleDoorTransitionFinished(bool bIsOpen);

    void TryCommitForChimera(ACMChimera* Chimera);
    void CommitRoom();

    TWeakObjectPtr<ACMRoomStreamingController> StreamingController;
    bool bCommitStarted = false;
    bool bCommitFinished = false;
};
