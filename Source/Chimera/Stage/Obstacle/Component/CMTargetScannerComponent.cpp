#include "Stage/Obstacle/Component/CMTargetScannerComponent.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

UCMTargetScannerComponent::UCMTargetScannerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 서버 Timer로 일정 주기 대상 탐색 시작
void UCMTargetScannerComponent::StartScanning()
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !Owner->HasAuthority() || !World
        || World->GetTimerManager().IsTimerActive(ScanTimerHandle))
    {
        return;
    }

    ScanNow();
    World->GetTimerManager().SetTimer(
        ScanTimerHandle,
        this,
        &ThisClass::ScanNow,
        FMath::Max(ScanInterval, 0.02f),
        true);
}

// 탐색 Timer를 해제하고 대상 상실 이벤트 전달
void UCMTargetScannerComponent::StopScanning()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ScanTimerHandle);
    }
    SetCurrentTarget(nullptr);
}

// 범위 안 후보 중 시야각과 벽 가림을 통과한 가장 가까운 Actor 선택
void UCMTargetScannerComponent::ScanNow()
{
    AActor* Owner = GetOwner();
    USceneComponent* OriginComponent = ResolveScanOrigin();
    if (!Owner || !Owner->HasAuthority() || !OriginComponent
        || TargetObjectTypes.IsEmpty())
    {
        SetCurrentTarget(nullptr);
        return;
    }

    const FVector OriginLocation = OriginComponent->GetComponentLocation();
    const FVector Forward = OriginComponent->GetForwardVector();
    const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(DetectionHalfAngle));

    TArray<AActor*> IgnoredActors = { Owner };
    TArray<AActor*> Candidates;
    UKismetSystemLibrary::SphereOverlapActors(
        this,
        OriginLocation,
        DetectionDistance,
        TargetObjectTypes,
        TargetClass,
        IgnoredActors,
        Candidates);

    AActor* BestTarget = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    for (AActor* Candidate : Candidates)
    {
        if (!IsValid(Candidate)
            || (!RequiredActorTag.IsNone() && !Candidate->ActorHasTag(RequiredActorTag)))
        {
            continue;
        }

        const FVector ToTarget = Candidate->GetActorLocation() - OriginLocation;
        const float DistanceSquared = ToTarget.SizeSquared();
        if (DistanceSquared <= UE_KINDA_SMALL_NUMBER
            || FVector::DotProduct(Forward, ToTarget.GetSafeNormal()) < MinimumDot
            || !HasLineOfSight(OriginLocation, Candidate))
        {
            continue;
        }

        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Candidate;
        }
    }

    SetCurrentTarget(BestTarget);
}

// Timer와 대상 참조를 월드 종료 전에 정리
void UCMTargetScannerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopScanning();
    Super::EndPlay(EndPlayReason);
}

// 지정된 Scene Component가 없으면 소유 Actor Root를 기준으로 사용
USceneComponent* UCMTargetScannerComponent::ResolveScanOrigin() const
{
    if (AActor* Owner = GetOwner())
    {
        if (UActorComponent* Component = ScanOrigin.GetComponent(Owner))
        {
            return Cast<USceneComponent>(Component);
        }
        return Owner->GetRootComponent();
    }
    return nullptr;
}

// 벽 가림 Trace가 대상 자신을 처음 맞힌 경우에만 시야 확보로 판단
bool UCMTargetScannerComponent::HasLineOfSight(
    const FVector& OriginLocation,
    const AActor* Candidate) const
{
    UWorld* World = GetWorld();
    if (!World || !Candidate)
    {
        return false;
    }

    FHitResult HitResult;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMTargetScannerLOS), false, GetOwner());
    const bool bBlocked = World->LineTraceSingleByChannel(
        HitResult,
        OriginLocation,
        Candidate->GetActorLocation(),
        LineOfSightTraceChannel,
        QueryParams);
    return !bBlocked || HitResult.GetActor() == Candidate;
}

// 대상이 실제로 바뀐 경우에만 상실과 획득 이벤트 순서대로 전달
void UCMTargetScannerComponent::SetCurrentTarget(AActor* NewTarget)
{
    if (CurrentTarget == NewTarget)
    {
        return;
    }

    AActor* PreviousTarget = CurrentTarget;
    CurrentTarget = NewTarget;
    if (PreviousTarget)
    {
        OnTargetLost.Broadcast(PreviousTarget);
    }
    if (CurrentTarget)
    {
        OnTargetAcquired.Broadcast(CurrentTarget);
    }
}
