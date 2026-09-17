#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMSpringArmHookProjectile.generated.h"

class ACMSpringArmPart;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Replicated, server-owned hook used by SpringArm.
 * ProjectileMovement performs the sweep, so this actor needs no custom Tick.
 */
UCLASS(Blueprintable)
class CHIMERA_API ACMSpringArmHookProjectile : public AActor
{
    GENERATED_BODY()

public:
    ACMSpringArmHookProjectile();

    void InitializeHook(
        ACMSpringArmPart* InSourcePart,
        const FVector& Direction,
        float Speed,
        float Radius,
        float MaxRange,
        const TArray<AActor*>& IgnoredActors
    );

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> CollisionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> HookMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

private:
    UFUNCTION()
    void HandleHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        FVector NormalImpulse,
        const FHitResult& Hit
    );

    UFUNCTION()
    void HandleProjectileStop(const FHitResult& Hit);

    void ResolveImpact(const FHitResult& Hit);

    TWeakObjectPtr<ACMSpringArmPart> SourcePart;
    bool bResolved = false;
};
