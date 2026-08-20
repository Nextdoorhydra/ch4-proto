#include "Parts/Arm/CMSpringArmHookProjectile.h"

#include "Parts/Arm/CMSpringArmPart.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ACMSpringArmHookProjectile::ACMSpringArmHookProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(
        TEXT("CollisionComponent")
    );
    SetRootComponent(CollisionComponent);
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
    CollisionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    CollisionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    CollisionComponent->SetGenerateOverlapEvents(false);
    CollisionComponent->OnComponentHit.AddDynamic(
        this,
        &ACMSpringArmHookProjectile::HandleHit
    );

    HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
    HookMesh->SetupAttachment(CollisionComponent);
    HookMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(
        TEXT("ProjectileMovement")
    );
    ProjectileMovement->UpdatedComponent = CollisionComponent;
    ProjectileMovement->ProjectileGravityScale = 0.0f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;
    ProjectileMovement->OnProjectileStop.AddDynamic(
        this,
        &ACMSpringArmHookProjectile::HandleProjectileStop
    );
}

void ACMSpringArmHookProjectile::InitializeHook(
    ACMSpringArmPart* InSourcePart,
    const FVector& Direction,
    float Speed,
    float Radius,
    float MaxRange,
    const TArray<AActor*>& IgnoredActors
)
{
    if (!HasAuthority() || !InSourcePart || Speed <= 0.0f)
    {
        Destroy();
        return;
    }

    SourcePart = InSourcePart;
    CollisionComponent->SetSphereRadius(FMath::Max(Radius, 1.0f));
    for (AActor* IgnoredActor : IgnoredActors)
    {
        if (IgnoredActor)
        {
            CollisionComponent->IgnoreActorWhenMoving(IgnoredActor, true);
        }
    }

    const FVector SafeDirection = Direction.GetSafeNormal();
    ProjectileMovement->InitialSpeed = Speed;
    ProjectileMovement->MaxSpeed = Speed;
    ProjectileMovement->Velocity = SafeDirection * Speed;
    SetLifeSpan(FMath::Max(MaxRange / Speed, 0.01f));
}

void ACMSpringArmHookProjectile::HandleHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    FVector NormalImpulse,
    const FHitResult& Hit
)
{
    ResolveImpact(Hit);
}

void ACMSpringArmHookProjectile::HandleProjectileStop(
    const FHitResult& Hit
)
{
    ResolveImpact(Hit);
}

void ACMSpringArmHookProjectile::ResolveImpact(const FHitResult& Hit)
{
    if (!HasAuthority() || bResolved)
    {
        return;
    }

    bResolved = true;
    if (SourcePart.IsValid())
    {
        SourcePart->ResolveHookHit(Hit);
    }
    Destroy();
}
