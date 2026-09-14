#include "Aggressive/Ripper/CMRipperPawn.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Aggressive/Common/Animation/CMAIProceduralLegComponent.h"
#include "Aggressive/Common/Behavior/CMAggressiveBehaviorComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "Aggressive/Common/Movement/CMAIFixedLegActuatorComponent.h"
#include "Aggressive/Common/Movement/CMGroundPlacementBoxComponent.h"
#include "Aggressive/Common/Perception/CMAggressiveSightComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace CMRipperBody
{
    constexpr float CubeHalfExtent = 50.0f;
    constexpr float RearCubeX = -100.0f;
    constexpr float RearCubeY = 50.0f;
    constexpr float GroundContactHeight = -100.0f;
    constexpr float LegHalfHeight = 25.0f;
    constexpr int32 LeftLegIndex = 1;
    constexpr float BalancedYawLeverArm = 110.0f;
} // namespace CMRipperBody

// T자 몸통과 전방·좌측·우측 다리 및 학습 이동 컴포넌트를 생성한다.
ACMRipperPawn::ACMRipperPawn()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;
    SetReplicateMovement(true);
    ConfiguredKnockbackDistanceCm = 100.0f;

    Behavior = CreateDefaultSubobject<UCMAggressiveBehaviorComponent>(TEXT("Behavior"));
    Behavior->ConfigureProfile(ECMAggressiveBehaviorProfile::Ripper);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));

    UCMGroundPlacementBoxComponent* GroundedPhysicsRoot = CreateDefaultSubobject<UCMGroundPlacementBoxComponent>(TEXT("FrontBodyCollision"));
    PhysicsRoot = GroundedPhysicsRoot;
    SetRootComponent(PhysicsRoot);
    PhysicsRoot->SetBoxExtent(FVector(CMRipperBody::CubeHalfExtent));
    GroundedPhysicsRoot->SetGroundContactHeight(CMRipperBody::GroundContactHeight);
    PhysicsRoot->SetCollisionProfileName(TEXT("CMRipperBody"));
    PhysicsRoot->SetSimulatePhysics(true);
    PhysicsRoot->SetIsReplicated(true);
    PhysicsRoot->SetCanEverAffectNavigation(false);
    BodyCollisions.Add(PhysicsRoot);

    LegActuator = CreateDefaultSubobject<UCMAIFixedLegActuatorComponent>(TEXT("FixedLegActuator"));
    MovementCommand = CreateDefaultSubobject<UCMAggressiveMovementCommandComponent>(TEXT("MovementCommand"));
    PathMovement = CreateDefaultSubobject<UCMAggressiveOmnidirectionalPathComponent>(TEXT("PathMovement"));
    PathMovement->SetPolicyControlEnabled(true);
    PathMovement->SetNavigationAgentName(TEXT("RipperAI"));
    PathMovement->SetIntermediatePathPointTolerances(130.0f, 220.0f);
    PathMovement->SetMaximumPathSegmentLength(600.0f);
    PathMovement->SetRebuildPathWhenIntermediatePointPassed(true);

    Sight = CreateDefaultSubobject<UCMAggressiveSightComponent>(TEXT("Sight"));
    Sight->SetupAttachment(PhysicsRoot);
    Sight->SetSightDefaults(1000.0f, 70.0f, 180.0f);
    Sight->SetVisionIndicatorAlwaysVisible(true);

    LegActuationSettings.ImpulseMagnitude = 20000.0f;
    LegActuationSettings.CooldownSeconds = 0.08f;

    AddBodyCube(TEXT("Front"), FVector::ZeroVector, CubeMeshAsset.Object);
    AddBodyCube(TEXT("RearLeft"), FVector(CMRipperBody::RearCubeX, -CMRipperBody::RearCubeY, 0.0f), CubeMeshAsset.Object);
    AddBodyCube(TEXT("RearRight"), FVector(CMRipperBody::RearCubeX, CMRipperBody::RearCubeY, 0.0f), CubeMeshAsset.Object);

    AddLeg(TEXT("Front"), FVector(60.0f, 0.0f, CMRipperBody::GroundContactHeight), CubeMeshAsset.Object);
    AddLeg(TEXT("Left"), FVector(-160.0f, -CMRipperBody::RearCubeY, CMRipperBody::GroundContactHeight), CubeMeshAsset.Object);
    AddLeg(TEXT("Right"), FVector(CMRipperBody::RearCubeX, 110.0f, CMRipperBody::GroundContactHeight), CubeMeshAsset.Object);
}

// 게임 시작 시 물리값과 다리 쿨다운을 초기화한다.
void ACMRipperPawn::BeginPlay()
{
    Super::BeginPlay();
    ApplyPeerCollisionProfile();
    LegActuator->ResolveInitialGroundPenetration(PhysicsRoot, LegContactPoints, LegActuationSettings);
    ApplyBodySettings();
    LegActuator->InitializeLegs(LegContactPoints.Num());
}

// Blueprint 기본값과 무관하게 모든 물리 형상이 Ripper 상호 충돌을 무시하게 한다.
void ACMRipperPawn::ApplyPeerCollisionProfile()
{
    for (UBoxComponent* Collision : BodyCollisions)
    {
        if (Collision)
        {
            Collision->SetCollisionProfileName(TEXT("CMRipperBody"));
        }
    }
    for (UBoxComponent* Collision : LegCollisions)
    {
        if (Collision)
        {
            Collision->SetCollisionProfileName(TEXT("CMRipperBody"));
        }
    }
}

// 지정한 다리 하나를 전방 임펄스로 구동하고 비대칭 Yaw 회전력만 보정한다.
bool ACMRipperPawn::ActivateLeg(int32 LegIndex)
{
    if (!HasAuthority() || !LegContactPoints.IsValidIndex(LegIndex) || !LegActuator)
    {
        return false;
    }

    FCMAIFixedLegActuationResult Result;
    const bool bActivated = LegActuator->TryActivateLeg(LegIndex, PhysicsRoot, LegContactPoints[LegIndex], FVector::ForwardVector, LegActuationSettings, Result);
    if (bActivated)
    {
        BalanceLegYawImpulse(LegIndex, Result);
        LegActuator->LimitPlanarSpeed(PhysicsRoot, MaxPlanarSpeed);
    }
    return bActivated;
}

// 정책이 선택한 여러 다리를 같은 판단 시점에 독립적으로 구동한다.
int32 ACMRipperPawn::ActivateLegs(const TArray<int32>& LegIndices)
{
    if (!HasAuthority() || !LegActuator)
        return 0;

    int32 ActivatedCount = 0;
    for (const int32 LegIndex : LegIndices)
    {
        if (!LegContactPoints.IsValidIndex(LegIndex))
            continue;

        FCMAIFixedLegActuationResult Result;
        if (LegActuator->TryActivateLeg(LegIndex, PhysicsRoot, LegContactPoints[LegIndex], FVector::ForwardVector, LegActuationSettings, Result))
        {
            BalanceLegYawImpulse(LegIndex, Result);
            ++ActivatedCount;
        }
    }

    if (ActivatedCount > 0)
        LegActuator->LimitPlanarSpeed(PhysicsRoot, MaxPlanarSpeed);

    return ActivatedCount;
}

// 왼쪽 다리의 부족한 Yaw 각운동량만 보충해 좌우 회전력을 맞춘다.
void ACMRipperPawn::BalanceLegYawImpulse(int32 LegIndex, const FCMAIFixedLegActuationResult& Result)
{
    if (LegIndex != CMRipperBody::LeftLegIndex || !PhysicsRoot)
        return;

    const float TargetYawAngularImpulse = LegActuationSettings.ImpulseMagnitude * CMRipperBody::BalancedYawLeverArm;
    const float MissingYawAngularImpulse = FMath::Max(TargetYawAngularImpulse - FMath::Abs(Result.YawAngularImpulse), 0.0f);
    if (MissingYawAngularImpulse <= 0.0f)
        return;

    const float DirectionSign = Result.YawAngularImpulse < 0.0f ? -1.0f : 1.0f;
    PhysicsRoot->AddAngularImpulseInRadians(FVector(0.0f, 0.0f, DirectionSign * MissingYawAngularImpulse), NAME_None, false);
}

// NavMesh 경로가 요구하는 방향을 정책 목표로 사용해 목적지 이동을 시작한다.
bool ACMRipperPawn::StartPathMoveToLocation(FVector WorldGoal, float AcceptanceRadius)
{
    if (!PathMovement || !MovementCommand)
        return false;
    PathMovement->SetPolicyControlEnabled(true);

    return PathMovement->StartPathMove(WorldGoal, AcceptanceRadius);
}

// 실행 중인 경로와 이동 목표를 취소한다.
void ACMRipperPawn::StopPathMove()
{
    if (PathMovement)
        PathMovement->StopPathMove();
    if (MovementCommand)
        MovementCommand->ClearMovementGoal();
}

void ACMRipperPawn::StopAggressiveMovementForReaction()
{
    StopPathMove();
}

// 정책 행동 스키마에 사용할 고정 다리 수를 반환한다.
int32 ACMRipperPawn::GetLegCount() const
{
    return LegContactPoints.Num();
}

// 공용 이동 계약에 복합 몸체의 물리 루트를 제공한다.
UPrimitiveComponent* ACMRipperPawn::GetAggressiveMovementBody() const
{
    return PhysicsRoot;
}

// 경로 이동과 정책 도달 판정에 공통으로 사용할 물리 루트 위치를 반환한다.
FVector ACMRipperPawn::GetAggressiveNavigationReferenceLocation() const
{
    return PhysicsRoot ? PhysicsRoot->GetComponentLocation() : GetActorLocation();
}

// 경로 완료 결과를 블루프린트 구독자에게 전달한다.
void ACMRipperPawn::HandleAggressivePathMoveCompleted(ECMAggressivePathMoveResult Result)
{
    if (MovementCommand)
        MovementCommand->ClearMovementGoal();
    OnPathMoveCompleted.Broadcast(Result);
}

// Ripper AI의 물리 이동 기준 컴포넌트를 반환한다.
UBoxComponent* ACMRipperPawn::GetPhysicsRoot() const
{
    return PhysicsRoot;
}

// Ripper AI의 고정 임펄스 다리 구동기를 반환한다.
UCMAIFixedLegActuatorComponent* ACMRipperPawn::GetLegActuator() const
{
    return LegActuator;
}

// Ripper AI의 학습 이동 목표 컴포넌트를 반환한다.
UCMAggressiveMovementCommandComponent* ACMRipperPawn::GetMovementCommand() const
{
    return MovementCommand;
}

// Ripper AI의 전용 NavMesh 경로 컴포넌트를 반환한다.
UCMAggressiveOmnidirectionalPathComponent* ACMRipperPawn::GetPathMovement() const
{
    return PathMovement;
}

// T자 몸통을 구성하는 충돌 박스 배열을 반환한다.
const TArray<TObjectPtr<UBoxComponent>>& ACMRipperPawn::GetBodyCollisions() const
{
    return BodyCollisions;
}

// 지면에서 몸통을 띄우는 임시 고정 다리 충돌체 배열을 반환한다.
const TArray<TObjectPtr<UBoxComponent>>& ACMRipperPawn::GetLegCollisions() const
{
    return LegCollisions;
}

// T자 몸통을 표시하는 메시 배열을 반환한다.
const TArray<TObjectPtr<UStaticMeshComponent>>& ACMRipperPawn::GetBodyMeshes() const
{
    return BodyMeshes;
}

// 전방·좌측·우측 다리 메시 배열을 반환한다.
const TArray<TObjectPtr<UStaticMeshComponent>>& ACMRipperPawn::GetLegMeshes() const
{
    return LegMeshes;
}

// 새 학습 에피소드가 즉시 다리를 사용할 수 있도록 쿨다운을 초기화한다.
void ACMRipperPawn::ResetLegActuation()
{
    if (LegActuator)
        LegActuator->ResetCooldowns();
}

// 물리 몸통 큐브 하나와 같은 위치의 시각 메시를 추가한다.
void ACMRipperPawn::AddBodyCube(const TCHAR* Name, const FVector& RelativeLocation, UStaticMesh* CubeMesh)
{
    UBoxComponent* Collision = PhysicsRoot;
    if (!RelativeLocation.IsNearlyZero())
    {
        Collision = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("%sBodyCollision"), Name));
        Collision->SetupAttachment(PhysicsRoot);
        Collision->SetRelativeLocation(RelativeLocation);
        Collision->SetBoxExtent(FVector(CMRipperBody::CubeHalfExtent));
        Collision->SetCollisionProfileName(TEXT("CMRipperBody"));
        Collision->SetSimulatePhysics(false);
        Collision->SetCanEverAffectNavigation(false);
        Collision->BodyInstance.bAutoWeld = true;
        BodyCollisions.Add(Collision);
    }

    UStaticMeshComponent* BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("%sBodyMesh"), Name));
    BodyMesh->SetupAttachment(PhysicsRoot);
    BodyMesh->SetStaticMesh(CubeMesh);
    BodyMesh->SetRelativeLocation(RelativeLocation);
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyMesh->SetCanEverAffectNavigation(false);
    BodyMeshes.Add(BodyMesh);
}

// 지정한 외곽 위치에 시각 다리와 지면 접촉점을 추가한다.
void ACMRipperPawn::AddLeg(const TCHAR* Name, const FVector& RelativeLocation, UStaticMesh* CubeMesh)
{
    const FVector LegCenterLocation = RelativeLocation + FVector(0.0f, 0.0f, CMRipperBody::LegHalfHeight);
    UBoxComponent* LegCollision = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("%sLegCollision"), Name));
    LegCollision->SetupAttachment(PhysicsRoot);
    LegCollision->SetRelativeLocation(LegCenterLocation);
    LegCollision->SetBoxExtent(FVector(10.0f, 10.0f, CMRipperBody::LegHalfHeight));
    LegCollision->SetCollisionProfileName(TEXT("CMRipperBody"));
    LegCollision->SetSimulatePhysics(false);
    LegCollision->SetCanEverAffectNavigation(false);
    LegCollision->BodyInstance.bAutoWeld = true;
    LegCollisions.Add(LegCollision);

    UStaticMeshComponent* LegMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("%sLegMesh"), Name));
    LegMesh->SetupAttachment(PhysicsRoot);
    LegMesh->SetStaticMesh(CubeMesh);
    LegMesh->SetRelativeLocation(LegCenterLocation);
    LegMesh->SetRelativeScale3D(FVector(0.2f, 0.2f, 0.5f));
    LegMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LegMesh->SetCanEverAffectNavigation(false);
    LegMesh->SetHiddenInGame(true);
    LegMeshes.Add(LegMesh);

    USceneComponent* ContactPoint = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("%sLegContact"), Name));
    ContactPoint->SetupAttachment(PhysicsRoot);
    ContactPoint->SetRelativeLocation(RelativeLocation);
    LegContactPoints.Add(ContactPoint);

    FVector OutwardDirection(RelativeLocation.X, RelativeLocation.Y, 0.0f);
    OutwardDirection = OutwardDirection.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    UCMAIProceduralLegComponent* ProceduralLeg = CreateDefaultSubobject<UCMAIProceduralLegComponent>(*FString::Printf(TEXT("%sProceduralLeg"), Name));
    ProceduralLeg->SetupAttachment(PhysicsRoot);
    ProceduralLeg->SetRelativeLocation(RelativeLocation - OutwardDirection * 30.0f + FVector::UpVector * 80.0f);
    ProceduralLeg->Configure(ContactPoint, OutwardDirection, static_cast<float>(ProceduralLegMeshes.Num()) / 3.0f, 1.8f);
    ProceduralLegMeshes.Add(ProceduralLeg);
}

// 빠른 가속과 Yaw 회전이 가능하도록 Ripper AI 몸통 물리 설정을 적용한다.
void ACMRipperPawn::ApplyBodySettings()
{
    if (!PhysicsRoot)
        return;

    PhysicsRoot->SetEnableGravity(bUseGravity);
    PhysicsRoot->SetLinearDamping(FMath::Max(LinearDamping, 0.0f));
    PhysicsRoot->SetAngularDamping(FMath::Max(AngularDamping, 0.0f));
    PhysicsRoot->SetMassOverrideInKg(NAME_None, FMath::Max(BodyMassKg, 0.1f), true);

    FBodyInstance& BodyInstance = PhysicsRoot->BodyInstance;
    BodyInstance.bLockXRotation = true;
    BodyInstance.bLockYRotation = true;
    BodyInstance.bLockZRotation = false;
    BodyInstance.SetDOFLock(EDOFMode::SixDOF);
}
