#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMForceZoneComponent.generated.h"

class ACMChimera;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCMForceZoneTargetSignature, AActor*, TargetActor, FVector, WorldForceDirection);

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 컨베이어와 바람 구역의 방향 및 세기를 보관하고 대상 감지를 전달
class CHIMERA_API UCMForceZoneComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMForceZoneComponent();

    // 서버에서 구역 안의 키메라에게 매 프레임 지속적인 환경 가속도 적용
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|ForceZone")
    void NotifyTargetEntered(
        AActor* TargetActor,
        UPrimitiveComponent* TargetComponent = nullptr);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|ForceZone")
    void NotifyTargetExited(
        AActor* TargetActor,
        UPrimitiveComponent* TargetComponent = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Chimera|Obstacle|ForceZone")
    void SetZoneEnabled(bool bEnabled) { bZoneEnabled = bEnabled; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle|ForceZone")
    FVector GetWorldForceDirection() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|ForceZone")
    FVector LocalDirection = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|ForceZone", meta = (ClampMin = "0.0"))
    float ForceStrength = 1000.0f;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Obstacle|ForceZone")
    FCMForceZoneTargetSignature OnTargetEntered;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Obstacle|ForceZone")
    FCMForceZoneTargetSignature OnTargetExited;

private:
    bool bZoneEnabled = true;

    // 여러 몸통 마디가 같은 볼륨과 겹칠 수 있어 액터별 오버랩 횟수 추적
    TMap<TWeakObjectPtr<ACMChimera>, int32> OverlappingChimeras;

    // Body와 Hurtbox가 함께 겹쳐도 같은 마디에는 가속도를 한 번만 적용
    TMap<TWeakObjectPtr<ACMChimera>, TMap<int32, int32>> OverlappingSegments;
};
