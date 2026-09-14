#include "Aggressive/Tetra/CMTetraPawn.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Aggressive/Common/Animation/CMAIProceduralLegComponent.h"
#include "Aggressive/Common/Behavior/CMAggressiveBehaviorComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveAccelerationMovementComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "Aggressive/Common/Movement/CMGroundPlacementBoxComponent.h"
#include "Aggressive/Common/Perception/CMAggressiveSightComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

namespace CMTetraBody
{
    constexpr float CubeHalfExtent = 50.0f;
    constexpr float BodyTurnSpeedDegrees = 600.0f;
}

// 단일 큐브 충돌 몸체와 절차적 애니메이션용 시각 다리 네 개를 생성한다.
ACMTetraPawn::ACMTetraPawn()
{
    using namespace CMTetraBody;

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bAllowTickOnDedicatedServer = false;
    bReplicates = true;
    SetReplicateMovement(true);
    ConfiguredKnockbackDistanceCm = 200.0f;

    Behavior = CreateDefaultSubobject<UCMAggressiveBehaviorComponent>(TEXT("Behavior"));
    Behavior->ConfigureProfile(ECMAggressiveBehaviorProfile::Tetra);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));

    UCMGroundPlacementBoxComponent* GroundedPhysicsRoot = CreateDefaultSubobject<UCMGroundPlacementBoxComponent>(TEXT("BodyCollision"));
    PhysicsRoot = GroundedPhysicsRoot;
    SetRootComponent(PhysicsRoot);
    PhysicsRoot->SetBoxExtent(FVector(CubeHalfExtent));
    GroundedPhysicsRoot->SetGroundContactHeight(-CubeHalfExtent);
    PhysicsRoot->SetCollisionProfileName(TEXT("PhysicsActor"));
    PhysicsRoot->SetSimulatePhysics(true);
    PhysicsRoot->SetIsReplicated(true);
    PhysicsRoot->SetCanEverAffectNavigation(false);

    VisualBodyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualBodyRoot"));
    VisualBodyRoot->SetupAttachment(PhysicsRoot);

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(VisualBodyRoot);
    BodyMesh->SetStaticMesh(CubeMeshAsset.Object);
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyMesh->SetCanEverAffectNavigation(false);

    AccelerationMovement = CreateDefaultSubobject<UCMAggressiveAccelerationMovementComponent>(TEXT("AccelerationMovement"));
    MovementCommand = CreateDefaultSubobject<UCMAggressiveMovementCommandComponent>(TEXT("MovementCommand"));
    PathMovement = CreateDefaultSubobject<UCMAggressiveOmnidirectionalPathComponent>(TEXT("PathMovement"));
    PathMovement->SetNavigationAgentName(TEXT("TetraAI"));
    PathMovement->SetRebuildPathWhenIntermediatePointPassed(true);

    Sight = CreateDefaultSubobject<UCMAggressiveSightComponent>(TEXT("Sight"));
    Sight->SetupAttachment(PhysicsRoot);
    Sight->SetSightDefaults(1000.0f, 100.0f, 180.0f);
    Sight->SetVisionIndicatorAlwaysVisible(true);

    AddVisualLeg(TEXT("FrontLeft"), FVector(35.0f, -45.0f, -50.0f), CubeMeshAsset.Object);
    AddVisualLeg(TEXT("FrontRight"), FVector(35.0f, 45.0f, -50.0f), CubeMeshAsset.Object);
    AddVisualLeg(TEXT("RearLeft"), FVector(-35.0f, -45.0f, -50.0f), CubeMeshAsset.Object);
    AddVisualLeg(TEXT("RearRight"), FVector(-35.0f, 45.0f, -50.0f), CubeMeshAsset.Object);
}

// 게임 시작 시 단일 몸통의 무회전 평면 물리 설정을 적용한다.
void ACMTetraPawn::BeginPlay()
{
    Super::BeginPlay();
    ApplyPlanarPhysicsSettings();
    if (BodyMesh)
        InitialBodyMeshRotation = BodyMesh->GetRelativeRotation().Quaternion();
    SetActorTickEnabled(GetNetMode() != NM_DedicatedServer);
}

// 복제된 시야 방향을 향해 바디 메시만 빠르게 회전하고 물리 몸체와 다리는 유지한다.
void ACMTetraPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!BodyMesh || !Sight)
        return;

    BodySightYaw = FMath::FixedTurn(BodySightYaw, Sight->GetRelativeRotation().Yaw, CMTetraBody::BodyTurnSpeedDegrees * DeltaSeconds);
    const FQuat SightRotation(FVector::UpVector, FMath::DegreesToRadians(BodySightYaw));
    BodyMesh->SetRelativeRotation(SightRotation * InitialBodyMeshRotation);
}

// 수동 시험 방향을 Tetra AI의 가속도 입력으로 변환한다.
bool ACMTetraPawn::SetManualMoveDirection(ECMAggressiveMoveDirection Direction)
{
    if (!AccelerationMovement || Direction == ECMAggressiveMoveDirection::None)
        return false;

    const FVector LocalDirection = CMAggressiveDirection::ToLocalUnitVector(Direction);
    const float BodyYawRadians = FMath::DegreesToRadians(PhysicsRoot ? PhysicsRoot->GetComponentRotation().Yaw : GetActorRotation().Yaw);
    AccelerationMovement->SetAccelerationInput(FQuat(FVector::UpVector, BodyYawRadians).RotateVector(LocalDirection));

    return true;
}

// 수동 시험의 평면 이동을 즉시 정지한다.
void ACMTetraPawn::StopManualMovement()
{
    if (AccelerationMovement)
        AccelerationMovement->StopMovement();
}

// 지정한 월드 목적지까지 Tetra AI용 NavMesh 경로 이동을 시작한다.
bool ACMTetraPawn::StartPathMoveToLocation(FVector WorldGoal, float AcceptanceRadius)
{
    return PathMovement && PathMovement->StartPathMove(WorldGoal, AcceptanceRadius);
}

// 실행 중인 Tetra AI의 NavMesh 경로 이동을 취소한다.
void ACMTetraPawn::StopPathMove()
{
    if (PathMovement)
        PathMovement->StopPathMove();
}

void ACMTetraPawn::StopAggressiveMovementForReaction()
{
    StopPathMove();
    StopManualMovement();
}

// Tetra AI 몸통을 구성하는 1m 큐브 수를 반환한다.
int32 ACMTetraPawn::GetBodyCubeCount() const
{
    return BodyMesh ? 1 : 0;
}

// 절차적 애니메이션에서 사용할 시각 다리 수를 반환한다.
int32 ACMTetraPawn::GetLegCount() const
{
    return LegMeshes.Num();
}

// 단일 큐브의 물리 이동 기준 컴포넌트를 반환한다.
UBoxComponent* ACMTetraPawn::GetPhysicsRoot() const
{
    return PhysicsRoot;
}

// 충돌과 분리된 단일 큐브 시각 메시를 반환한다.
UStaticMeshComponent* ACMTetraPawn::GetBodyMesh() const
{
    return BodyMesh;
}

// 추후 절차적 애니메이션이 사용할 시각 몸체 기준점을 반환한다.
USceneComponent* ACMTetraPawn::GetVisualBodyRoot() const
{
    return VisualBodyRoot;
}

// Tetra AI의 학습 가속 이동 컴포넌트를 반환한다.
UCMAggressiveAccelerationMovementComponent* ACMTetraPawn::GetAccelerationMovement() const
{
    return AccelerationMovement;
}

UCMAggressiveMovementCommandComponent* ACMTetraPawn::GetMovementCommand() const
{
    return MovementCommand;
}

// Tetra AI의 NavMesh 경로 방향 요청 컴포넌트를 반환한다.
UCMAggressiveOmnidirectionalPathComponent* ACMTetraPawn::GetPathMovement() const
{
    return PathMovement;
}

// 절차적 애니메이션용 시각 다리 메시 배열을 반환한다.
const TArray<TObjectPtr<UStaticMeshComponent>>& ACMTetraPawn::GetLegMeshes() const
{
    return LegMeshes;
}

// 공용 이동 계약에 단일 큐브 물리 몸통을 제공한다.
UPrimitiveComponent* ACMTetraPawn::GetAggressiveMovementBody() const
{
    return PhysicsRoot;
}

// 공용 이동 계약에 단일 큐브의 컴포넌트 위치를 제공한다.
FVector ACMTetraPawn::GetAggressiveNavigationReferenceLocation() const
{
    return PhysicsRoot ? PhysicsRoot->GetComponentLocation() : GetActorLocation();
}

// 경로 이동 완료 결과를 Tetra AI 블루프린트 구독자에게 전달한다.
void ACMTetraPawn::HandleAggressivePathMoveCompleted(ECMAggressivePathMoveResult Result)
{
    OnPathMoveCompleted.Broadcast(Result);
}

// 충돌 없는 시각 다리를 절차적 애니메이션용 개별 컴포넌트로 추가한다.
void ACMTetraPawn::AddVisualLeg(const TCHAR* Name, const FVector& RelativeContactLocation, UStaticMesh* CubeMesh)
{
    UStaticMeshComponent* LegMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("%sLegMesh"), Name));
    LegMesh->SetupAttachment(VisualBodyRoot);
    LegMesh->SetStaticMesh(CubeMesh);
    LegMesh->SetRelativeLocation(RelativeContactLocation + FVector(0.0f, 0.0f, 15.0f));
    LegMesh->SetRelativeScale3D(FVector(0.2f, 0.2f, 0.3f));
    LegMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LegMesh->SetCanEverAffectNavigation(false);
    LegMesh->SetHiddenInGame(true);
    LegMeshes.Add(LegMesh);

    USceneComponent* ContactPoint = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("%sLegContact"), Name));
    ContactPoint->SetupAttachment(VisualBodyRoot);
    ContactPoint->SetRelativeLocation(RelativeContactLocation);
    LegContactPoints.Add(ContactPoint);

    FVector OutwardDirection(RelativeContactLocation.X, RelativeContactLocation.Y, 0.0f);
    OutwardDirection = OutwardDirection.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    UCMAIProceduralLegComponent* ProceduralLeg = CreateDefaultSubobject<UCMAIProceduralLegComponent>(*FString::Printf(TEXT("%sProceduralLeg"), Name));
    ProceduralLeg->SetupAttachment(VisualBodyRoot);
    ProceduralLeg->SetRelativeLocation(RelativeContactLocation - OutwardDirection * 18.0f + FVector::UpVector * 55.0f);
    ProceduralLeg->Configure(ContactPoint, OutwardDirection, static_cast<float>(ProceduralLegMeshes.Num()) / 4.0f, 1.3f);
    ProceduralLegMeshes.Add(ProceduralLeg);
}

// 지면 착지는 허용하고 회전과 지면 마찰은 제거해 맵 재질과 무관하게 평면 가속도를 적용한다.
void ACMTetraPawn::ApplyPlanarPhysicsSettings()
{
    if (!PhysicsRoot)
        return;

    PhysicsRoot->SetSimulatePhysics(true);
    PhysicsRoot->SetEnableGravity(true);
    PhysicsRoot->SetRelativeRotation(FRotator::ZeroRotator);
    if (VisualBodyRoot)
        VisualBodyRoot->SetRelativeRotation(FRotator::ZeroRotator);

    if (!RuntimeGroundPhysicalMaterial)
    {
        RuntimeGroundPhysicalMaterial = NewObject<UPhysicalMaterial>(this, TEXT("TetraGroundPhysicalMaterial"));
        RuntimeGroundPhysicalMaterial->Friction = 0.0f;
        RuntimeGroundPhysicalMaterial->StaticFriction = 0.0f;
        RuntimeGroundPhysicalMaterial->bOverrideFrictionCombineMode = true;
        RuntimeGroundPhysicalMaterial->FrictionCombineMode = EFrictionCombineMode::Multiply;
    }
    PhysicsRoot->SetPhysMaterialOverride(RuntimeGroundPhysicalMaterial);

    FBodyInstance& BodyInstance = PhysicsRoot->BodyInstance;
    BodyInstance.bLockXRotation = true;
    BodyInstance.bLockYRotation = true;
    BodyInstance.bLockZRotation = true;
    BodyInstance.bLockZTranslation = false;
    BodyInstance.SetDOFLock(EDOFMode::SixDOF);
}
