#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMForceZoneComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCMForceZoneTargetSignature, AActor*, TargetActor, FVector, WorldForceDirection);

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 컨베이어와 바람 구역의 방향 및 세기를 보관하고 대상 감지를 전달
class CHIMERA_API UCMForceZoneComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMForceZoneComponent();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|ForceZone")
    void NotifyTargetEntered(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|ForceZone")
    void NotifyTargetExited(AActor* TargetActor);

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

    // TODO: 서버에서 키메라 몸통 마디의 PrimitiveComponent에 실제 힘 적용
};
