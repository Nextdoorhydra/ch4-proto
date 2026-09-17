#include "Stage/Obstacle/Component/CMAttackEmitterComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AsyncLoad/CMStageLoadLog.h"

UCMAttackEmitterComponent::UCMAttackEmitterComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 공격 방식에 따라 서버 판정 경로 선택
bool UCMAttackEmitterComponent::Fire(
    const FVector& StartLocation,
    const FVector& Direction)
{
    AActor* Owner = GetOwner();
    const FVector SafeDirection = Direction.GetSafeNormal();
    if (!Owner || !Owner->HasAuthority() || SafeDirection.IsNearlyZero())
    {
        return false;
    }

    return DeliveryMode == ECMAttackDeliveryMode::Projectile
        ? FireProjectile(StartLocation, SafeDirection)
        : FireHitscan(StartLocation, SafeDirection);
}

// 서버 Line Trace 결과와 실제 표현 끝점을 이벤트로 전달
bool UCMAttackEmitterComponent::FireHitscan(
    const FVector& StartLocation,
    const FVector& SafeDirection)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    FCMAttackResult Result;
    Result.StartLocation = StartLocation;
    const FVector TraceEnd = StartLocation + SafeDirection * MaxDistance;

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMAttackHitscan), false, GetOwner());
    Result.bHit = World->LineTraceSingleByChannel(
        Result.HitResult,
        StartLocation,
        TraceEnd,
        TraceChannel,
        QueryParams);
    Result.EndLocation = Result.bHit ? Result.HitResult.ImpactPoint : TraceEnd;

    // TODO: Hit Actor 또는 Hit Component의 피격 계약이 확정되면 GE나 부위 데미지 적용
    OnAttackResolved.Broadcast(Result);
    return true;
}

// 비동기 준비된 Projectile Class만 서버에서 생성하고 방향은 Spawn Rotation으로 전달
bool UCMAttackEmitterComponent::FireProjectile(
    const FVector& StartLocation,
    const FVector& SafeDirection)
{
    UWorld* World = GetWorld();
    UClass* LoadedProjectileClass = ProjectileClass.Get();
    if (!World || !LoadedProjectileClass)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Projectile Class was not prepared before fire. Owner=%s Asset=%s"),
            *GetNameSafe(GetOwner()),
            *ProjectileClass.ToSoftObjectPath().ToString());
        return false;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.Instigator = GetOwner() ? GetOwner()->GetInstigator() : nullptr;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Projectile = World->SpawnActor<AActor>(
        LoadedProjectileClass,
        StartLocation,
        SafeDirection.Rotation(),
        SpawnParams);
    return IsValid(Projectile);
}
