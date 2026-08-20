#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMTestAreaDestination.generated.h"

class ACMChimera;
class ACMTestAreaManager;
class UBoxComponent;
class UPrimitiveComponent;

UCLASS()
// 모든 활성 몸통 마디가 들어오면 다음 Test Area 시작점으로 순환 이동
class CHIMERA_API ACMTestAreaDestination : public AActor
{
    GENERATED_BODY()

public:
    ACMTestAreaDestination();

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

    UFUNCTION()
    void HandleEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);

    ACMTestAreaManager* FindTestAreaManager() const;

    UPROPERTY(VisibleAnywhere, Category = "Chimera|Test Area")
    TObjectPtr<UBoxComponent> DestinationVolume;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Chimera|Test Area",
        meta = (AllowPrivateAccess = "true"))
    FName AreaId;

    UPROPERTY(Transient)
    TObjectPtr<ACMTestAreaManager> TestAreaManager;

    bool bAdvanceRequested = false;
};
