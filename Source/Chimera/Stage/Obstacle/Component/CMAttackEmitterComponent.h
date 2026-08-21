#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMAttackEmitterComponent.generated.h"

UENUM(BlueprintType)
enum class ECMAttackDeliveryMode : uint8
{
    Hitscan,   // 발사 순간 Line Trace로 결과 확정
    Projectile // 서버가 실제 충돌 판정용 Actor 생성
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMAttackResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bHit = false;

    UPROPERTY(BlueprintReadOnly)
    FVector StartLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector EndLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FHitResult HitResult;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMAttackResolvedSignature,
    const FCMAttackResult&, Result);

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 서버에서 Hitscan 결과를 확정하거나 실제 Projectile Actor를 생성
class CHIMERA_API UCMAttackEmitterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMAttackEmitterComponent();

    // 시작 위치와 방향으로 현재 공격 방식을 서버에서 실행
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|Attack")
    bool Fire(const FVector& StartLocation, const FVector& Direction);

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Obstacle|Attack")
    FCMAttackResolvedSignature OnAttackResolved;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Attack")
    ECMAttackDeliveryMode DeliveryMode = ECMAttackDeliveryMode::Hitscan;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Attack",
        meta = (ClampMin = "1.0"))
    float MaxDistance = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Attack")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Attack",
        meta = (EditCondition = "DeliveryMode == ECMAttackDeliveryMode::Projectile", EditConditionHides))
    TSoftClassPtr<AActor> ProjectileClass;

private:
    bool FireHitscan(const FVector& StartLocation, const FVector& SafeDirection);
    bool FireProjectile(const FVector& StartLocation, const FVector& SafeDirection);
};
