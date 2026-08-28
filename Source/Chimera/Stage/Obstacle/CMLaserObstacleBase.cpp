#include "Stage/Obstacle/CMLaserObstacleBase.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Player/CMChimera.h"
#include "Stage/Obstacle/Component/CMHazardComponent.h"
#include "Stage/Obstacle/Component/CMLaserBeamComponent.h"
#include "TimerManager.h"

ACMLaserObstacleBase::ACMLaserObstacleBase()
{
    // 레이저 PrimaryMesh는 고체 장치가 아니라 빔 표현이므로 통과 가능해야 한다.
    PrimaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    LaserStart = CreateDefaultSubobject<USceneComponent>(TEXT("LaserStart"));
    LaserStart->SetupAttachment(SceneRoot);

    BeamCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BeamCollision"));
    BeamCollision->SetupAttachment(SceneRoot);
    BeamCollision->SetMobility(EComponentMobility::Movable);
    BeamCollision->SetCollisionProfileName(TEXT("CMHazardOverlap"));
    BeamCollision->SetGenerateOverlapEvents(true);
    BeamCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Hazard = CreateDefaultSubobject<UCMHazardComponent>(TEXT("Hazard"));
    BeamPresentation = CreateDefaultSubobject<UCMLaserBeamComponent>(TEXT("BeamPresentation"));
}

// 복제된 끝점으로 모든 클라이언트가 같은 Beam 표현 사용
void ACMLaserObstacleBase::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, LaserEndLocation);
    DOREPLIFETIME(ThisClass, bPlayerImpactActive);
}

// 공용 표현과 Overlap을 연결하고 활성 상태를 다시 반영
void ACMLaserObstacleBase::BeginPlay()
{
    Super::BeginPlay();

    BeamPresentation->ConfigurePresentation(PrimaryMesh, PrimaryEffect);
    BeamCollision->OnComponentBeginOverlap.AddUniqueDynamic(
        this, &ThisClass::HandleBeamBeginOverlap);
    BeamCollision->OnComponentEndOverlap.AddUniqueDynamic(
        this, &ThisClass::HandleBeamEndOverlap);

    HandleObstacleActiveStateChanged(IsObstacleActive());
}

// 레이저 갱신 Timer와 Collision Delegate 정리
void ACMLaserObstacleBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UpdateRefreshTimer(false);
    BeamCollision->OnComponentBeginOverlap.RemoveAll(this);
    BeamCollision->OnComponentEndOverlap.RemoveAll(this);
    Super::EndPlay(EndPlayReason);
}

// 서버가 벽까지 실제 끝점을 계산하고 복제한 뒤 로컬 표현 즉시 갱신
void ACMLaserObstacleBase::RefreshLaser()
{
    if (!HasAuthority() || !LaserStart || !IsObstacleActive())
    {
        return;
    }

    const FVector StartLocation = LaserStart->GetComponentLocation();
    const FVector TraceEnd = StartLocation
        + LaserStart->GetForwardVector() * FMath::Max(MaxDistance, 1.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(CMStaticLaserTrace),
        bTraceComplex,
        this);
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        StartLocation,
        TraceEnd,
        TraceChannel,
        QueryParams);

    LaserEndLocation = bHit ? HitResult.ImpactPoint : TraceEnd;
    bPlayerImpactActive = bHit && IsPlayerImpactTarget(HitResult.GetActor());
    ApplyLaserGeometry();
    ForceNetUpdate();
}

// 부모의 복제 활성 상태에 맞춰 표현, 서버 Collision, 재계산 Timer 통일
void ACMLaserObstacleBase::HandleObstacleActiveStateChanged(bool bIsActive)
{
    if (!BeamPresentation || !BeamCollision)
    {
        return;
    }

    BeamPresentation->SetBeamVisible(bIsActive);
    if (!bIsActive)
    {
        bPlayerImpactActive = false;
        BeamPresentation->SetPlayerImpactActive(false);
    }
    BeamCollision->SetCollisionEnabled(
        bIsActive && HasAuthority()
            ? ECollisionEnabled::QueryOnly
            : ECollisionEnabled::NoCollision);

    if (bIsActive && HasAuthority())
    {
        RefreshLaser();
    }
    UpdateRefreshTimer(bIsActive && HasAuthority() && RefreshInterval > 0.0f);
}

// 서버에서 복제받은 끝점으로 Collision 없이 표현만 갱신
void ACMLaserObstacleBase::OnRep_LaserEndLocation()
{
    ApplyLaserGeometry();
}

// 서버가 판정한 플레이어 타격 상태를 로컬 Niagara 스파크에 반영
void ACMLaserObstacleBase::OnRep_PlayerImpactActive()
{
    if (BeamPresentation)
    {
        BeamPresentation->SetPlayerImpactActive(bPlayerImpactActive);
    }
}

// 지속형 레이저 진입을 기존 Hazard 이벤트로 전달
void ACMLaserObstacleBase::HandleBeamBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (Hazard)
    {
        Hazard->NotifyTargetEntered(OtherActor, OtherComponent);
    }
}

// 지속형 레이저 이탈을 기존 Hazard 이벤트로 전달
void ACMLaserObstacleBase::HandleBeamEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex)
{
    if (Hazard)
    {
        Hazard->NotifyTargetExited(OtherActor, OtherComponent);
    }
}

// 복제된 두 점으로 Beam 표현과 서버 판정 Box를 동일하게 정렬
void ACMLaserObstacleBase::ApplyLaserGeometry()
{
    if (!LaserStart || !BeamPresentation || !BeamCollision)
    {
        return;
    }

    const FVector StartLocation = LaserStart->GetComponentLocation();
    const FVector Delta = FVector(LaserEndLocation) - StartLocation;
    const float BeamLength = Delta.Size();
    if (BeamLength <= UE_KINDA_SMALL_NUMBER)
    {
        BeamPresentation->SetBeamVisible(false);
        BeamCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return;
    }

    BeamPresentation->ApplyBeam(StartLocation, LaserEndLocation);
    BeamPresentation->SetPlayerImpactActive(bPlayerImpactActive);
    BeamPresentation->SetBeamVisible(IsObstacleActive());

    BeamCollision->SetWorldLocationAndRotation(
        FMath::Lerp(StartLocation, FVector(LaserEndLocation), 0.5f),
        Delta.Rotation());
    BeamCollision->SetWorldScale3D(FVector::OneVector);
    BeamCollision->SetBoxExtent(FVector(
        BeamLength * 0.5f,
        BeamPresentation->BeamThickness,
        BeamPresentation->BeamThickness));
}

// 키메라 몸통 또는 현재 슬롯에 장착된 파츠만 플레이어 타격으로 판정
bool ACMLaserObstacleBase::IsPlayerImpactTarget(const AActor* HitActor) const
{
    if (!IsValid(HitActor))
    {
        return false;
    }

    if (HitActor->IsA<ACMChimera>())
    {
        return true;
    }

    const ACMPartActorBase* PartActor = Cast<ACMPartActorBase>(HitActor);
    return PartActor && PartActor->IsAttached();
}

// 움직이는 차폐물이 있는 레이저만 선택적으로 서버 재계산
void ACMLaserObstacleBase::UpdateRefreshTimer(bool bShouldRun)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(RefreshTimerHandle);
    if (bShouldRun)
    {
        World->GetTimerManager().SetTimer(
            RefreshTimerHandle,
            this,
            &ThisClass::RefreshLaser,
            FMath::Max(RefreshInterval, 0.02f),
            true);
    }
}
