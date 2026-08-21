#pragma once

#include "CoreMinimal.h"
#include "Stage/Obstacle/CMStageObstacleBase.h"

#include "CMLaserObstacleBase.generated.h"

class UBoxComponent;
class UCMHazardComponent;
class UCMLaserBeamComponent;
class USceneComponent;
class UPrimitiveComponent;

UCLASS(Blueprintable)
// 시작점에서 벽까지 서버 Trace를 수행하는 지속형 고정 레이저 장애물
class CHIMERA_API ACMLaserObstacleBase : public ACMStageObstacleBase
{
    GENERATED_BODY()

public:
    ACMLaserObstacleBase();
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 현재 벽 배치에 맞춰 서버 끝점과 Collision을 다시 계산
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Laser Trace")
    void RefreshLaser();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void HandleObstacleActiveStateChanged(bool bIsActive) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<USceneComponent> LaserStart;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UBoxComponent> BeamCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UCMHazardComponent> Hazard;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UCMLaserBeamComponent> BeamPresentation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Laser Trace",
        meta = (ClampMin = "1.0"))
    float MaxDistance = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Laser Trace")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Laser Trace")
    bool bTraceComplex = false;

    // 0이면 활성화 순간 한 번만 계산하고 양수면 움직이는 벽을 주기적으로 다시 검사
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Laser Trace",
        meta = (ClampMin = "0.0"))
    float RefreshInterval = 0.0f;

private:
    UFUNCTION()
    void OnRep_LaserEndLocation();

    UFUNCTION()
    void HandleBeamBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleBeamEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);

    void ApplyLaserGeometry();
    void UpdateRefreshTimer(bool bShouldRun);

    UPROPERTY(ReplicatedUsing = OnRep_LaserEndLocation)
    FVector_NetQuantize100 LaserEndLocation = FVector::ZeroVector;

    FTimerHandle RefreshTimerHandle;
};
