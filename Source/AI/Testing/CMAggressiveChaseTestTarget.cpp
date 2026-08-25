#include "Testing/CMAggressiveChaseTestTarget.h"

#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMAggressiveChaseTest, Log, All);

namespace
{
    constexpr int32 TeleportSearchAttemptCount = 12;
}

// NavMesh 후보가 평면거리와 높이 제한을 모두 만족하는지 반환한다.
bool CMAggressiveChaseTest::IsTeleportCandidateWithinBounds(const FVector& OriginNavLocation, const FVector& CandidateNavLocation, float MinimumDistance, float MaximumDistance, float MaximumHeightDifference)
{
    const float ClampedMinimumDistance = FMath::Max(MinimumDistance, 0.0f);
    const float ClampedMaximumDistance = FMath::Max(MaximumDistance, ClampedMinimumDistance);
    const float PlanarDistance = FVector::Dist2D(OriginNavLocation, CandidateNavLocation);
    const float HeightDifference = FMath::Abs(CandidateNavLocation.Z - OriginNavLocation.Z);
    return PlanarDistance >= ClampedMinimumDistance && PlanarDistance <= ClampedMaximumDistance && HeightDifference <= FMath::Max(MaximumHeightDifference, 0.0f);
}

// 접촉 상자와 시각 메시를 만들고 Tick을 비활성화한다.
ACMAggressiveChaseTestTarget::ACMAggressiveChaseTestTarget()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;
    SetReplicateMovement(true);

    ContactTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ContactTrigger"));
    SetRootComponent(ContactTrigger);
    ContactTrigger->SetBoxExtent(FVector(50.0f));
    ContactTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ContactTrigger->SetCollisionObjectType(ECC_WorldDynamic);
    ContactTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    ContactTrigger->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    ContactTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    ContactTrigger->SetGenerateOverlapEvents(true);
    ContactTrigger->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
    TargetMesh->SetupAttachment(ContactTrigger);
    TargetMesh->SetStaticMesh(CubeMeshAsset.Object);
    TargetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TargetMesh->SetCanEverAffectNavigation(false);
}

// 서버에서 공격적 AI 접촉 이벤트를 구독한다.
void ACMAggressiveChaseTestTarget::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
        ContactTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleContact);
}

// 현재 위치에서 도달 가능하고 장애물과 겹치지 않는 NavMesh 위치로 순간이동한다.
bool ACMAggressiveChaseTestTarget::TeleportToRandomReachableLocation()
{
    return TeleportToRandomReachableLocationUsingAgent(NavigationAgentName);
}

bool ACMAggressiveChaseTestTarget::TeleportToRandomReachableLocationUsingAgent(FName InNavigationAgentName)
{
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavigationSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    if (!HasAuthority() || !World || !NavigationSystem || !ContactTrigger)
    {
        UE_LOG(LogCMAggressiveChaseTest, Warning, TEXT("추격 테스트 목표가 순간이동할 월드 또는 NavMesh를 찾지 못했습니다."));
        return false;
    }

    ANavigationData* NavigationData = nullptr;
    for (const FNavDataConfig& AgentConfig : NavigationSystem->GetSupportedAgents())
    {
        if (AgentConfig.Name != InNavigationAgentName)
            continue;
        NavigationData = NavigationSystem->GetNavDataForProps(AgentConfig);
        break;
    }
    if (!NavigationData)
    {
        UE_LOG(LogCMAggressiveChaseTest, Warning, TEXT("추격 테스트 목표가 사용할 전용 NavMesh를 찾지 못했습니다: %s"), *InNavigationAgentName.ToString());
        return false;
    }

    FNavLocation OriginNavLocation;
    if (!NavigationSystem->ProjectPointToNavigation(GetActorLocation(), OriginNavLocation, FVector(100.0f, 100.0f, 200.0f), NavigationData))
    {
        UE_LOG(LogCMAggressiveChaseTest, Warning, TEXT("추격 테스트 목표의 현재 위치를 NavMesh에 투영하지 못했습니다."));
        return false;
    }

    for (int32 AttemptIndex = 0; AttemptIndex < TeleportSearchAttemptCount; ++AttemptIndex)
    {
        FNavLocation CandidateNavLocation;
        if (!NavigationSystem->GetRandomReachablePointInRadius(OriginNavLocation.Location, FMath::Max(TeleportRadius, 0.0f), CandidateNavLocation, NavigationData))
            continue;
        if (!CMAggressiveChaseTest::IsTeleportCandidateWithinBounds(OriginNavLocation.Location, CandidateNavLocation.Location, MinimumTeleportDistance, TeleportRadius, MaximumHeightDifference))
            continue;

        const FVector CandidateLocation = CandidateNavLocation.Location + FVector(0.0f, 0.0f, ContactTrigger->GetScaledBoxExtent().Z);
        if (!IsTeleportLocationClear(CandidateLocation))
            continue;
        if (!SetActorLocation(CandidateLocation, false, nullptr, ETeleportType::TeleportPhysics))
            continue;

        UE_LOG(LogCMAggressiveChaseTest, Display, TEXT("추격 테스트 목표가 안전한 NavMesh 위치로 순간이동했습니다. 위치: %s"), *CandidateLocation.ToCompactString());
        return true;
    }

    UE_LOG(LogCMAggressiveChaseTest, Warning, TEXT("5m 안에서 장애물과 겹치지 않는 추격 테스트 목표 위치를 찾지 못했습니다."));
    return false;
}

// 공격적 이동 계약을 구현한 AI와 접촉했을 때만 추격 테스트 목표를 순간이동한다.
void ACMAggressiveChaseTestTarget::HandleContact(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (bTeleporting || !Cast<ICMAggressiveMovementAgent>(OtherActor))
        return;

    bTeleporting = true;
    const UCMAggressiveOmnidirectionalPathComponent* PathMovement = OtherActor->FindComponentByClass<UCMAggressiveOmnidirectionalPathComponent>();
    TeleportToRandomReachableLocationUsingAgent(PathMovement ? PathMovement->GetNavigationAgentName() : NavigationAgentName);
    bTeleporting = false;
}

// 순간이동 후보에 Pawn 크기의 장애물 충돌이 없는지 반환한다.
bool ACMAggressiveChaseTestTarget::IsTeleportLocationClear(const FVector& CandidateLocation) const
{
    if (!GetWorld() || !ContactTrigger)
        return false;

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMAggressiveChaseTestTeleport), false, this);
    const FVector ClearanceExtent = ContactTrigger->GetScaledBoxExtent() * 0.9f;
    return !GetWorld()->OverlapBlockingTestByChannel(CandidateLocation, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeBox(ClearanceExtent), QueryParams);
}

// 공격적 AI 접촉을 감지하는 상자 컴포넌트를 반환한다.
UBoxComponent* ACMAggressiveChaseTestTarget::GetContactTrigger() const
{
    return ContactTrigger;
}

// NavMesh 순간이동 후보를 찾을 최대 반경을 반환한다.
float ACMAggressiveChaseTestTarget::GetTeleportRadius() const
{
    return TeleportRadius;
}

// 순간이동이 눈에 보이도록 요구하는 최소 평면거리를 반환한다.
float ACMAggressiveChaseTestTarget::GetMinimumTeleportDistance() const
{
    return MinimumTeleportDistance;
}

// 장애물 윗면을 제외하기 위한 최대 NavMesh 높이 차를 반환한다.
float ACMAggressiveChaseTestTarget::GetMaximumHeightDifference() const
{
    return MaximumHeightDifference;
}
