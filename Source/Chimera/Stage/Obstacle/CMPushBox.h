#pragma once

#include "CoreMinimal.h"
#include "Combat/CMCombatHitTarget.h"
#include "GameFramework/Actor.h"

#include "CMPushBox.generated.h"

class UCMMechanismWeightComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

/** 플레이어의 몸통 충돌과 기본 팔 공격으로만 이동하는 키네마틱 상자다. */
UCLASS(Blueprintable)
class CHIMERA_API ACMPushBox : public AActor, public ICMCombatHitTarget
{
    GENERATED_BODY()

public:
    ACMPushBox();

    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual bool ReceiveCombatHit_Implementation(const FCMCombatHitRequest& Request) override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Push Box")
    TObjectPtr<UStaticMeshComponent> BoxMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Push Box")
    TObjectPtr<UCMMechanismWeightComponent> MechanismWeight;

    /** 압력판 등 게임플레이 무게 판정에 사용한다. 이동량에는 영향을 주지 않는다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Push Box", meta = (ClampMin = "0.0", Units = "kg"))
    float WeightInKg = 40.0f;

    /** 플레이어 충돌과 팔 공격으로 움직일 때의 일정한 속도다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Push Box", meta = (ClampMin = "0.0", Units = "cm/s"))
    float PushSpeed = 300.0f;

    /** 한 번의 유효한 충돌 또는 공격으로 이동하는 최대 거리다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Push Box", meta = (ClampMin = "0.0", Units = "cm"))
    float PushDistance = 100.0f;

    /** 정지 상태의 단순 접촉을 밀기로 처리하지 않기 위한 최소 플레이어 속도다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Push Box", meta = (ClampMin = "0.0", Units = "cm/s"))
    float MinimumPlayerImpactSpeed = 10.0f;

private:
    UFUNCTION()
    void HandleBoxHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

    void ApplyEditorSettings();
    FVector ResolveCardinalPushDirection(const FVector& ImpactPoint, const FVector& FallbackDirection) const;
    bool StartPush(FVector WorldDirection);
    void StopPush();

    FVector PushDirection = FVector::ZeroVector;
    float RemainingPushDistance = 0.0f;
    FGuid LastAcceptedAttackId;
};
