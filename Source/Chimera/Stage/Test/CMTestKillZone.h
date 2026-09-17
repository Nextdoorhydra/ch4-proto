#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMTestKillZone.generated.h"

class ACMTestAreaManager;
class UBoxComponent;
class UPrimitiveComponent;

UCLASS()
// 키메라가 닿으면 자신이 속한 Test Sublevel 시작점으로 즉시 복귀
class CHIMERA_API ACMTestKillZone : public AActor
{
    GENERATED_BODY()

public:
    ACMTestKillZone();

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void HandleBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    ACMTestAreaManager* FindTestAreaManager() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Test Area",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> KillVolume;

    UPROPERTY(Transient)
    TObjectPtr<ACMTestAreaManager> TestAreaManager;

    bool bReturnRequested = false;
};
