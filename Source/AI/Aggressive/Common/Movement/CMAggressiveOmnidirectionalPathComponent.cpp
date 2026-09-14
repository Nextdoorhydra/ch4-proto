#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"

#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"
#include "Common/CMAINavigationRules.h"
#include "Components/LineBatchComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"
#include "HAL/IConsoleManager.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMAggressiveOmnidirectionalPath, Log, All);

namespace
{
    constexpr float PathDebugHeight = 50.0f;
    constexpr float PathDebugLifetime = 3600.0f;

#if !UE_BUILD_SHIPPING
    bool bDrawAggressivePathDebug = false;

    void SetAggressivePathDebugDraw(const TArray<FString>& Args, UWorld* World)
    {
        if (Args.Num() != 1)
        {
            return;
        }

        const FString& Value = Args[0];
        if (Value.Equals(TEXT("on"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value == TEXT("1"))
        {
            bDrawAggressivePathDebug = true;
        }
        else if (Value.Equals(TEXT("off"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Value == TEXT("0"))
        {
            bDrawAggressivePathDebug = false;
        }
        else
        {
            UE_LOG(LogCMAggressiveOmnidirectionalPath, Warning, TEXT("Usage: CM.AI.AggressivePathDebug on|off"));

            return;
        }

        for (TObjectIterator<UCMAggressiveOmnidirectionalPathComponent> It; It; ++It)
        {
            if (It->GetWorld() == World)
            {
                It->RefreshPathDebug();
            }
        }

    }

    FAutoConsoleCommandWithWorldAndArgs AggressivePathDebugCommand(
        TEXT("CM.AI.AggressivePathDebug"),
        TEXT("Draw aggressive AI navigation path points. Usage: "
             "CM.AI.AggressivePathDebug on|off"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetAggressivePathDebugDraw)
    );
#endif

    bool HasPassedPathPoint(const FVector& BodyLocation, const FVector& SegmentStart, const FVector& PathPoint)
    {
        FVector SegmentDirection = PathPoint - SegmentStart;
        SegmentDirection.Z = 0.0f;
        if (SegmentDirection.IsNearlyZero())
            return false;

        FVector BodyOffset = BodyLocation - PathPoint;
        BodyOffset.Z = 0.0f;
        return FVector::DotProduct(BodyOffset, SegmentDirection) >= 0.0f;
    }
} // namespace

// 몸체가 반경 안에 있거나 가까운 중간 경로점의 진행 평면을 통과했는지 판정한다.
bool CMAggressiveOmnidirectionalPath::ShouldAdvancePathPoint(const FVector& BodyLocation, const FVector& SegmentStart, const FVector& PathPoint, float AcceptanceRadius, float PassDetectionRadius, bool bFinalPathPoint)
{
    const float DistanceSquared = FVector::DistSquared2D(BodyLocation, PathPoint);
    if (DistanceSquared <= FMath::Square(FMath::Max(AcceptanceRadius, 0.0f)))
        return true;
    if (bFinalPathPoint || DistanceSquared > FMath::Square(FMath::Max(PassDetectionRadius, 0.0f)))
        return false;

    FVector SegmentDirection = PathPoint - SegmentStart;
    SegmentDirection.Z = 0.0f;
    if (SegmentDirection.IsNearlyZero())
        return true;

    FVector BodyOffset = BodyLocation - PathPoint;
    BodyOffset.Z = 0.0f;
    return FVector::DotProduct(BodyOffset, SegmentDirection) >= 0.0f;
}

// Tick 없이 전용 NavMesh 경로를 확인하는 컴포넌트를 생성한다.
UCMAggressiveOmnidirectionalPathComponent::UCMAggressiveOmnidirectionalPathComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

// 게임 시작 시 이 컴포넌트만 사용하는 디버그 배치 식별자를 만든다.
void UCMAggressiveOmnidirectionalPathComponent::BeginPlay()
{
    Super::BeginPlay();
    PathDebugBatchId = PointerHash(this);
    CachedMovementAgent = Cast<ICMAggressiveMovementAgent>(GetOwner());
    MovementCommand = GetOwner() ? GetOwner()->FindComponentByClass<UCMAggressiveMovementCommandComponent>() : nullptr;
}

// 컴포넌트 종료 시 경로 타이머와 디버그 표시를 제거한다.
void UCMAggressiveOmnidirectionalPathComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FinishPathMove(ECMAggressivePathMoveResult::Cancelled, false);
    MovementCommand = nullptr;
    CachedMovementAgent = nullptr;
    Super::EndPlay(EndPlayReason);
}

// 지정한 목적지까지 완전한 전용 NavMesh 경로를 만들고 방향 요청을 시작한다.
bool UCMAggressiveOmnidirectionalPathComponent::StartPathMove(FVector WorldGoal, float AcceptanceRadius)
{
    AActor* Owner = GetOwner();
    ICMAggressiveMovementAgent* Agent = CachedMovementAgent;
    UWorld* World = GetWorld();
    if (!Owner || !Owner->HasAuthority() || !Agent || !World || !bPolicyControlEnabled)
    {
        return false;
    }

    FinishPathMove(ECMAggressivePathMoveResult::Cancelled, false);
    ActiveWorldGoal = WorldGoal;
    FinalAcceptanceRadius = FMath::Max(AcceptanceRadius, 0.0f);
    Agent->PrepareAggressivePathMove(WorldGoal);
    if (FVector::DistSquared2D(Agent->GetAggressiveNavigationReferenceLocation(), WorldGoal) <= FMath::Square(FinalAcceptanceRadius))
    {
        Agent->HandleAggressivePathMoveCompleted(ECMAggressivePathMoveResult::ReachedGoal);

        return true;
    }

    if (!BuildNavigationPath(WorldGoal))
    {
        Agent->HandleAggressivePathMoveCompleted(ECMAggressivePathMoveResult::Failed);

        return false;
    }

    bPathMoving = true;
    ActivePathPointIndex = 1;
    if (!UpdateMovementDirection())
    {
        FinishPathMove(ECMAggressivePathMoveResult::Failed, true);

        return false;
    }

    DrawPathDebug();
    World->GetTimerManager().SetTimer(PathUpdateTimerHandle, this, &ThisClass::UpdatePathMove, FMath::Max(PathUpdateInterval, 0.01f), true);

    return true;
}

// 실행 중인 경로 이동을 정지하고 취소 결과를 알린다.
void UCMAggressiveOmnidirectionalPathComponent::StopPathMove()
{
    if (bPathMoving)
        FinishPathMove(ECMAggressivePathMoveResult::Cancelled, true);
}

// 복구 중 기존 경로와 표시를 유지하면서 경로 갱신과 정책 목표만 일시 중지한다.
void UCMAggressiveOmnidirectionalPathComponent::PausePathMoveForRecovery()
{
    if (!bPathMoving)
        return;

    UWorld* World = GetWorld();
    if (World)
        World->GetTimerManager().ClearTimer(PathUpdateTimerHandle);

    if (MovementCommand)
        MovementCommand->ClearMovementGoal();
    RequestedMoveDirection = ECMAggressiveMoveDirection::None;
}

// 현재 NavMesh 경로 이동의 활성 여부를 반환한다.
bool UCMAggressiveOmnidirectionalPathComponent::IsPathMoving() const
{
    return bPathMoving;
}

// 현재 이동 목표인 경로점 인덱스를 반환한다.
int32 UCMAggressiveOmnidirectionalPathComponent::GetActivePathPointIndex() const
{
    return ActivePathPointIndex;
}

// 현재 NavMesh 원본 경로점 수를 반환한다.
int32 UCMAggressiveOmnidirectionalPathComponent::GetPathPointCount() const
{
    return ActivePathPoints.Num();
}

// 경로점 도착을 확인하는 타이머 간격을 반환한다.
float UCMAggressiveOmnidirectionalPathComponent::GetPathUpdateInterval() const
{
    return PathUpdateInterval;
}

// 최종 목적지 전 경로점에 사용하는 도착 반경을 반환한다.
float UCMAggressiveOmnidirectionalPathComponent::GetIntermediateAcceptanceRadius() const
{
    return IntermediateAcceptanceRadius;
}

float UCMAggressiveOmnidirectionalPathComponent::GetIntermediatePathPointJitterRadius() const
{
    return IntermediatePathPointJitterRadius;
}

float UCMAggressiveOmnidirectionalPathComponent::GetMinimumPathPointSpacing() const
{
    return MinimumPathPointSpacing;
}

// 경로 계산에 사용할 전방위 몸체 전용 에이전트 이름을 반환한다.
FName UCMAggressiveOmnidirectionalPathComponent::GetNavigationAgentName() const
{
    return NavigationAgentName;
}

// 기본 NavData에 의존하지 않고 이 몸체에 지정된 에이전트의 NavMesh에서 배회 목적지를 찾는다.
bool UCMAggressiveOmnidirectionalPathComponent::FindRandomReachableLocation(FVector Origin, float Radius, FVector& OutLocation) const
{
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavigationSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    if (!NavigationSystem)
        return false;

    FNavDataConfig AgentConfig;
    const ANavigationData* NavigationData = nullptr;
    if (!FindNavigationAgent(*NavigationSystem, AgentConfig, NavigationData))
        return false;

    FNavLocation ProjectedOrigin;
    if (!NavigationSystem->ProjectPointToNavigation(Origin, ProjectedOrigin, NavigationProjectionExtent, NavigationData))
        return false;

    FNavLocation RandomLocation;
    if (!NavigationSystem->GetRandomReachablePointInRadius(ProjectedOrigin.Location, FMath::Max(Radius, 0.0f), RandomLocation, const_cast<ANavigationData*>(NavigationData)))
        return false;

    OutLocation = RandomLocation.Location;
    return true;
}

// 전용 NavMesh 경계 안의 마지막 위치를 기억하고 이탈 시 가장 가까운 복귀 위치를 찾는다.
bool UCMAggressiveOmnidirectionalPathComponent::FindNavigationRecoveryLocation(FVector& OutLocation)
{
    OutLocation = FVector::ZeroVector;
    UWorld* World = GetWorld();
    ICMAggressiveMovementAgent* Agent = CachedMovementAgent;
    UNavigationSystemV1* NavigationSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    if (!World || !Agent || !NavigationSystem)
    {
        return false;
    }

    FNavDataConfig AgentConfig;
    const ANavigationData* NavigationData = nullptr;
    if (!FindNavigationAgent(*NavigationSystem, AgentConfig, NavigationData))
    {
        return false;
    }

    const FVector CurrentLocation = Agent->GetAggressiveNavigationReferenceLocation();
    const float ContainmentTolerance = FMath::Max(NavigationContainmentTolerance, 0.0f);
    FNavLocation ProjectedLocation;
    const FVector ContainmentExtent(ContainmentTolerance, ContainmentTolerance, NavigationProjectionExtent.Z);
    if (NavigationSystem->ProjectPointToNavigation(CurrentLocation, ProjectedLocation, ContainmentExtent, NavigationData)
        && FCMAINavigationRules::IsWithinProjectionTolerance(CurrentLocation, ProjectedLocation.Location, ContainmentTolerance))
    {
        LastValidNavigationLocation = CurrentLocation;
        bHasLastValidNavigationLocation = true;
        return false;
    }

    if (bHasLastValidNavigationLocation)
    {
        OutLocation = FVector(LastValidNavigationLocation.X, LastValidNavigationLocation.Y, CurrentLocation.Z);
        return true;
    }
    if (NavigationSystem->ProjectPointToNavigation(CurrentLocation, ProjectedLocation, NavigationRecoveryProjectionExtent, NavigationData))
    {
        OutLocation = FVector(ProjectedLocation.Location.X, ProjectedLocation.Location.Y, CurrentLocation.Z);
        return true;
    }
    return false;
}

// 지정한 시작점과 목적지 사이의 완전한 전용 NavMesh 경로 길이를 계산한다.
bool UCMAggressiveOmnidirectionalPathComponent::CalculateNavigationPathLength(FVector StartLocation, FVector WorldGoal, float& OutPathLength)
{
    OutPathLength = 0.0f;
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavigationSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    if (!Owner || !NavigationSystem)
        return false;

    FNavDataConfig AgentConfig;
    const ANavigationData* NavigationData = nullptr;
    if (!FindNavigationAgent(*NavigationSystem, AgentConfig, NavigationData))
        return false;

    FNavLocation ProjectedStart;
    FNavLocation ProjectedGoal;
    if (!NavigationSystem->ProjectPointToNavigation(StartLocation, ProjectedStart, NavigationProjectionExtent, NavigationData) || !NavigationSystem->ProjectPointToNavigation(WorldGoal, ProjectedGoal, NavigationProjectionExtent, NavigationData))
        return false;

    FPathFindingQuery Query(Owner, *NavigationData, ProjectedStart.Location, ProjectedGoal.Location);
    Query.SetAllowPartialPaths(false);
    Query.SetNavAgentProperties(AgentConfig);
    const FPathFindingResult PathResult = NavigationSystem->FindPathSync(AgentConfig, Query);
    if (!PathResult.IsSuccessful() || !PathResult.Path.IsValid() || PathResult.Path->IsPartial())
        return false;

    const TArray<FNavPathPoint>& PathPoints = PathResult.Path->GetPathPoints();
    if (PathPoints.IsEmpty())
        return false;
    for (int32 PointIndex = 1; PointIndex < PathPoints.Num(); ++PointIndex)
        OutPathLength += FVector::Dist2D(PathPoints[PointIndex - 1].Location, PathPoints[PointIndex].Location);

    return true;
}

// NavMesh 밖 목표는 허용 반경 안에서 접근할 수 있는 경우에만 추적 대상으로 인정한다.
bool UCMAggressiveOmnidirectionalPathComponent::IsNavigationGoalReachable(const FVector WorldGoal, const float GoalTolerance) const
{
    AActor* Owner = GetOwner();
    const ICMAggressiveMovementAgent* Agent = Cast<ICMAggressiveMovementAgent>(Owner);
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavigationSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    if (!Owner || !Agent || !NavigationSystem)
    {
        return false;
    }

    FNavDataConfig AgentConfig;
    const ANavigationData* NavigationData = nullptr;
    if (!FindNavigationAgent(*NavigationSystem, AgentConfig, NavigationData))
    {
        return false;
    }

    FNavLocation ProjectedStart;
    FNavLocation ProjectedGoal;
    if (!NavigationSystem->ProjectPointToNavigation(Agent->GetAggressiveNavigationReferenceLocation(), ProjectedStart, NavigationProjectionExtent, NavigationData)
        || !NavigationSystem->ProjectPointToNavigation(WorldGoal, ProjectedGoal, NavigationProjectionExtent, NavigationData))
    {
        return false;
    }
    const float ProjectionTolerance = FMath::Max(GoalTolerance, NavigationContainmentTolerance);
    if (!FCMAINavigationRules::IsWithinProjectionTolerance(WorldGoal, ProjectedGoal.Location, ProjectionTolerance))
    {
        return false;
    }

    FPathFindingQuery Query(Owner, *NavigationData, ProjectedStart.Location, ProjectedGoal.Location);
    Query.SetAllowPartialPaths(false);
    Query.SetNavAgentProperties(AgentConfig);
    const FPathFindingResult PathResult = NavigationSystem->FindPathSync(AgentConfig, Query);
    return PathResult.IsSuccessful() && PathResult.Path.IsValid() && !PathResult.Path->IsPartial();
}

// 정책 제어 여부를 바꾸고 비활성화할 때 실행 중인 경로 이동을 취소한다.
void UCMAggressiveOmnidirectionalPathComponent::SetPolicyControlEnabled(bool bEnabled)
{
    if (bPolicyControlEnabled == bEnabled)
        return;

    if (!bEnabled && bPathMoving)
        FinishPathMove(ECMAggressivePathMoveResult::Cancelled, true);
    bPolicyControlEnabled = bEnabled;
}

// 현재 경로 이동 방향을 정책이 결정하도록 설정했는지 반환한다.
bool UCMAggressiveOmnidirectionalPathComponent::IsPolicyControlEnabled() const
{
    return bPolicyControlEnabled;
}

// 현재 경로점이 요구하는 로컬 8방향 또는 정지를 반환한다.
ECMAggressiveMoveDirection UCMAggressiveOmnidirectionalPathComponent::GetRequestedMoveDirection() const
{
    return RequestedMoveDirection;
}

// 소유 AI 몸체 크기에 맞는 NavMesh 에이전트 이름을 설정한다.
void UCMAggressiveOmnidirectionalPathComponent::SetNavigationAgentName(FName InNavigationAgentName)
{
    if (!InNavigationAgentName.IsNone())
        NavigationAgentName = InNavigationAgentName;
}

// 몸체별 중간 경유지 도착 반경과 통과 판정 반경을 설정한다.
void UCMAggressiveOmnidirectionalPathComponent::SetIntermediatePathPointTolerances(float AcceptanceRadius, float PassDetectionRadius)
{
    IntermediateAcceptanceRadius = FMath::Max(AcceptanceRadius, 0.0f);
    IntermediatePassDetectionRadius = FMath::Max(PassDetectionRadius, IntermediateAcceptanceRadius);
}

// 긴 원본 NavMesh 선분만 나눌 최대 경로 구간 길이를 설정한다.
void UCMAggressiveOmnidirectionalPathComponent::SetMaximumPathSegmentLength(float MaximumSegmentLength)
{
    MaximumPathSegmentLength = FMath::Max(MaximumSegmentLength, 0.0f);
}

void UCMAggressiveOmnidirectionalPathComponent::SetIntermediatePathPointJitterRadius(float JitterRadius)
{
    IntermediatePathPointJitterRadius = FMath::Max(JitterRadius, 0.0f);
}

void UCMAggressiveOmnidirectionalPathComponent::SetIntermediatePathPointWallClearance(float WallClearance)
{
    IntermediatePathPointWallClearance = FMath::Max(WallClearance, 0.0f);
}

void UCMAggressiveOmnidirectionalPathComponent::SetMinimumPathPointSpacing(float MinimumSpacing)
{
    MinimumPathPointSpacing = FMath::Max(MinimumSpacing, 0.0f);
}

void UCMAggressiveOmnidirectionalPathComponent::SetRebuildPathWhenIntermediatePointPassed(bool bEnabled)
{
    bRebuildPathWhenIntermediatePointPassed = bEnabled;
}

// 경로를 만들지 않는 학습 에피소드가 사용할 요구 방향을 설정한다.
void UCMAggressiveOmnidirectionalPathComponent::SetLearningRequestedMoveDirection(ECMAggressiveMoveDirection Direction)
{
    if (bPolicyControlEnabled && !bPathMoving)
        RequestedMoveDirection = Direction;
}

// 전용 NavData에서 목적지까지 완전한 원본 NavMesh 경로를 생성한다.
bool UCMAggressiveOmnidirectionalPathComponent::BuildNavigationPath(FVector WorldGoal)
{
    ActiveNavigationData = nullptr;
    AActor* Owner = GetOwner();
    ICMAggressiveMovementAgent* Agent = Cast<ICMAggressiveMovementAgent>(Owner);
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavigationSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    if (!Owner || !Agent || !NavigationSystem)
    {
        return false;
    }

    FNavDataConfig AgentConfig;
    const ANavigationData* NavigationData = nullptr;
    if (!FindNavigationAgent(*NavigationSystem, AgentConfig, NavigationData))
        return false;

    FNavLocation ProjectedStart;
    FNavLocation ProjectedGoal;
    if (!NavigationSystem->ProjectPointToNavigation(Agent->GetAggressiveNavigationReferenceLocation(), ProjectedStart, NavigationProjectionExtent, NavigationData))
        return false;
    if (!NavigationSystem->ProjectPointToNavigation(WorldGoal, ProjectedGoal, NavigationProjectionExtent, NavigationData))
        return false;

    FPathFindingQuery Query(Owner, *NavigationData, ProjectedStart.Location, ProjectedGoal.Location);
    Query.SetAllowPartialPaths(false);
    Query.SetNavAgentProperties(AgentConfig);
    const FPathFindingResult PathResult = NavigationSystem->FindPathSync(AgentConfig, Query);
    if (!PathResult.IsSuccessful() || !PathResult.Path.IsValid() || PathResult.Path->IsPartial() || PathResult.Path->GetPathPoints().Num() < 2)
    {
        return false;
    }

    const TArray<FNavPathPoint>& NavigationPathPoints = PathResult.Path->GetPathPoints();
    // 긴 선분을 나눠 정책이 급격한 방향 변화 없이 중간 목표를 따라가게 한다.
    ActivePathPoints.Reset(NavigationPathPoints.Num());
    ActivePathPoints.Add(NavigationPathPoints[0].Location);
    for (int32 PathPointIndex = 1; PathPointIndex < NavigationPathPoints.Num(); ++PathPointIndex)
    {
        const FVector SegmentStart = NavigationPathPoints[PathPointIndex - 1].Location;
        const FVector SegmentEnd = NavigationPathPoints[PathPointIndex].Location;
        const float SegmentLength = FVector::Dist2D(SegmentStart, SegmentEnd);
        const int32 SegmentPartCount = MaximumPathSegmentLength > 0.0f ? FMath::Max(FMath::CeilToInt(SegmentLength / MaximumPathSegmentLength), 1) : 1;
        for (int32 SegmentPartIndex = 1; SegmentPartIndex <= SegmentPartCount; ++SegmentPartIndex)
            ActivePathPoints.Add(FMath::Lerp(SegmentStart, SegmentEnd, static_cast<float>(SegmentPartIndex) / static_cast<float>(SegmentPartCount)));
    }

    // 시작점과 최종 목적지는 보존하고 중간점만 흔들어 반복 경로의 편향을 줄인다.
    const float JitterRadius = FMath::Max(IntermediatePathPointJitterRadius, 0.0f);
    if (JitterRadius > 0.0f)
    {
        const FVector ProjectionExtent(JitterRadius, JitterRadius, NavigationProjectionExtent.Z);
        for (int32 PathPointIndex = 1; PathPointIndex < ActivePathPoints.Num() - 1; ++PathPointIndex)
        {
            const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
            const float Distance = FMath::FRandRange(0.0f, JitterRadius);
            const FVector Candidate = ActivePathPoints[PathPointIndex] + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
            FNavLocation ProjectedPoint;
            if (NavigationSystem->ProjectPointToNavigation(Candidate, ProjectedPoint, ProjectionExtent, NavigationData))
                ActivePathPoints[PathPointIndex] = ProjectedPoint.Location;
        }
    }

    // 연결 가능성을 검증하면서 몸체가 벽에 걸리지 않도록 중간점을 안쪽으로 민다.
    AdjustIntermediatePathPointsAwayFromWalls(*World, *NavigationSystem, *NavigationData);

    // 직선 연결이 막히지 않는 범위에서 지나치게 가까운 점을 제거한다.
    const float MinimumSpacing = FMath::Max(MinimumPathPointSpacing, 0.0f);
    if (MinimumSpacing > 0.0f && ActivePathPoints.Num() > 2)
    {
        TArray<FVector> SpacedPathPoints;
        SpacedPathPoints.Reserve(ActivePathPoints.Num());
        SpacedPathPoints.Add(ActivePathPoints[0]);
        for (int32 PathPointIndex = 1; PathPointIndex < ActivePathPoints.Num() - 1; ++PathPointIndex)
        {
            if (FVector::DistSquared2D(SpacedPathPoints.Last(), ActivePathPoints[PathPointIndex]) >= FMath::Square(MinimumSpacing))
            {
                SpacedPathPoints.Add(ActivePathPoints[PathPointIndex]);
                continue;
            }

            FVector HitLocation;
            const FVector& NextPathPoint = ActivePathPoints[PathPointIndex + 1];
            if (NavigationData->Raycast(SpacedPathPoints.Last(), NextPathPoint, HitLocation, NavigationData->GetDefaultQueryFilter(), GetOwner()))
                SpacedPathPoints.Add(ActivePathPoints[PathPointIndex]);
        }

        const FVector FinalPathPoint = ActivePathPoints.Last();
        while (SpacedPathPoints.Num() > 1 && FVector::DistSquared2D(SpacedPathPoints.Last(), FinalPathPoint) < FMath::Square(MinimumSpacing))
        {
            FVector HitLocation;
            if (NavigationData->Raycast(SpacedPathPoints[SpacedPathPoints.Num() - 2], FinalPathPoint, HitLocation, NavigationData->GetDefaultQueryFilter(), GetOwner()))
                break;
            SpacedPathPoints.Pop(EAllowShrinking::No);
        }
        SpacedPathPoints.Add(FinalPathPoint);
        ActivePathPoints = MoveTemp(SpacedPathPoints);
    }
    ActiveNavigationData = NavigationData;
    return true;
}

// 정적 장애물의 반발 방향으로 중간 경로점을 옮기되 양쪽 경로 연결을 보존한다.
void UCMAggressiveOmnidirectionalPathComponent::AdjustIntermediatePathPointsAwayFromWalls(UWorld& World, UNavigationSystemV1& NavigationSystem, const ANavigationData& NavigationData)
{
    const float WallClearance = FMath::Max(IntermediatePathPointWallClearance, 0.0f);
    if (WallClearance <= 0.0f || ActivePathPoints.Num() <= 2)
        return;

    FCollisionObjectQueryParams StaticObjects;
    StaticObjects.AddObjectTypesToQuery(ECC_WorldStatic);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMAggressivePathWallClearance), false, GetOwner());
    const FVector ProjectionExtent(WallClearance, WallClearance, NavigationProjectionExtent.Z);
    constexpr float CandidateScales[] = {1.0f, 0.5f, 0.25f};
    for (int32 PathPointIndex = 1; PathPointIndex < ActivePathPoints.Num() - 1; ++PathPointIndex)
    {
        const FVector OriginalPoint = ActivePathPoints[PathPointIndex];
        TArray<FOverlapResult> Overlaps;
        World.OverlapMultiByObjectType(Overlaps, OriginalPoint, FQuat::Identity, StaticObjects, FCollisionShape::MakeSphere(WallClearance), QueryParams);

        FVector Repulsion = FVector::ZeroVector;
        for (const FOverlapResult& Overlap : Overlaps)
        {
            const UPrimitiveComponent* Obstacle = Overlap.GetComponent();
            if (!Obstacle)
                continue;
            FVector ClosestPoint;
            const float SurfaceDistance = Obstacle->GetClosestPointOnCollision(OriginalPoint, ClosestPoint);
            FVector AwayFromSurface = OriginalPoint - ClosestPoint;
            AwayFromSurface.Z = 0.0f;
            const float PlanarDistance = AwayFromSurface.Size();
            if (SurfaceDistance < 0.0f || PlanarDistance <= UE_SMALL_NUMBER || PlanarDistance >= WallClearance)
                continue;
            Repulsion += AwayFromSurface / PlanarDistance * (WallClearance - PlanarDistance);
        }
        Repulsion.Z = 0.0f;
        Repulsion = Repulsion.GetClampedToMaxSize(WallClearance);
        if (Repulsion.IsNearlyZero())
            continue;

        for (const float CandidateScale : CandidateScales)
        {
            FNavLocation ProjectedPoint;
            if (!NavigationSystem.ProjectPointToNavigation(OriginalPoint + Repulsion * CandidateScale, ProjectedPoint, ProjectionExtent, &NavigationData))
                continue;
            FVector HitLocation;
            if (NavigationData.Raycast(ActivePathPoints[PathPointIndex - 1], ProjectedPoint.Location, HitLocation, NavigationData.GetDefaultQueryFilter(), GetOwner()) ||
                NavigationData.Raycast(ProjectedPoint.Location, ActivePathPoints[PathPointIndex + 1], HitLocation, NavigationData.GetDefaultQueryFilter(), GetOwner()))
                continue;
            ActivePathPoints[PathPointIndex] = ProjectedPoint.Location;
            break;
        }
    }
}

// 설정 이름과 일치하는 에이전트 구성 및 실제 NavData를 찾는다.
bool UCMAggressiveOmnidirectionalPathComponent::FindNavigationAgent(const UNavigationSystemV1& NavigationSystem, FNavDataConfig& OutAgentConfig, const ANavigationData*& OutNavigationData) const
{
    for (const FNavDataConfig& AgentConfig : NavigationSystem.GetSupportedAgents())
    {
        if (AgentConfig.Name != NavigationAgentName)
            continue;

        OutAgentConfig = AgentConfig;
        OutNavigationData = NavigationSystem.GetNavDataForProps(AgentConfig);

        return OutNavigationData != nullptr;
    }
    return false;
}

// 현재 경로점의 로컬 8방향과 학습 정책이 사용할 이동 목표를 갱신한다.
bool UCMAggressiveOmnidirectionalPathComponent::UpdateMovementDirection()
{
    AActor* Owner = GetOwner();
    ICMAggressiveMovementAgent* Agent = CachedMovementAgent;
    UPrimitiveComponent* MovementBody = Agent ? Agent->GetAggressiveMovementBody() : nullptr;
    if (!Owner || !Agent || !MovementBody || !MovementCommand || !bPolicyControlEnabled || !ActivePathPoints.IsValidIndex(ActivePathPointIndex))
    {
        return false;
    }

    const FVector BodyLocation = Agent->GetAggressiveNavigationReferenceLocation();
    const FVector WorldDirection = ActivePathPoints[ActivePathPointIndex] - BodyLocation;
    const ECMAggressiveMoveDirection MoveDirection = CMAggressiveDirection::QuantizeWorldDirection8(WorldDirection, MovementBody->GetComponentQuat());
    if (MoveDirection == ECMAggressiveMoveDirection::None)
        return false;
    RequestedMoveDirection = MoveDirection;
    const bool bFinalPathPoint = ActivePathPointIndex == ActivePathPoints.Num() - 1;
    MovementCommand->SetMovementGoal(ActivePathPoints[ActivePathPointIndex], bFinalPathPoint ? FinalAcceptanceRadius : IntermediateAcceptanceRadius);

    return true;
}

// 타이머 시점마다 경로점 도착과 다음 8방향 이동 명령을 갱신한다.
void UCMAggressiveOmnidirectionalPathComponent::UpdatePathMove()
{
    ICMAggressiveMovementAgent* Agent = CachedMovementAgent;
    if (!bPathMoving || !Agent || !ActivePathPoints.IsValidIndex(ActivePathPointIndex))
    {
        FinishPathMove(ECMAggressivePathMoveResult::Failed, true);

        return;
    }

    const FVector BodyLocation = Agent->GetAggressiveNavigationReferenceLocation();
    while (ActivePathPoints.IsValidIndex(ActivePathPointIndex))
    {
        const bool bFinalPathPoint = ActivePathPointIndex == ActivePathPoints.Num() - 1;
        const float AcceptanceRadius = bFinalPathPoint ? FinalAcceptanceRadius : IntermediateAcceptanceRadius;
        const FVector SegmentStart = ActivePathPointIndex > 0 ? ActivePathPoints[ActivePathPointIndex - 1] : BodyLocation;
        const FVector PathPoint = ActivePathPoints[ActivePathPointIndex];
        // 긴 몸체가 경유지를 비껴 지나가면 남은 점을 건너뛰지 않고 현재 위치에서 안전하게 재탐색한다.
        if (!bFinalPathPoint && bRebuildPathWhenIntermediatePointPassed && FVector::DistSquared2D(BodyLocation, PathPoint) > FMath::Square(FMath::Max(AcceptanceRadius, 0.0f)) && HasPassedPathPoint(BodyLocation, SegmentStart, PathPoint))
        {
            const FVector RebuildGoal = ActiveWorldGoal;
            const float RebuildAcceptanceRadius = FinalAcceptanceRadius;
            StartPathMove(RebuildGoal, RebuildAcceptanceRadius);

            return;
        }
        if (!CMAggressiveOmnidirectionalPath::ShouldAdvancePathPoint(BodyLocation, SegmentStart, PathPoint, AcceptanceRadius, IntermediatePassDetectionRadius, bFinalPathPoint))
            break;
        if (bFinalPathPoint)
        {
            FinishPathMove(ECMAggressivePathMoveResult::ReachedGoal, true);

            return;
        }
        ++ActivePathPointIndex;
        DrawPathDebug();
    }

    if (!UpdateMovementDirection())
        FinishPathMove(ECMAggressivePathMoveResult::Failed, true);
}

// 이동을 멈추고 경로 상태와 디버그 표시를 제거한 뒤 선택적으로 결과를 알린다.
void UCMAggressiveOmnidirectionalPathComponent::FinishPathMove(ECMAggressivePathMoveResult Result, bool bBroadcastResult)
{
    UWorld* World = GetWorld();
    if (World)
        World->GetTimerManager().ClearTimer(PathUpdateTimerHandle);

    ICMAggressiveMovementAgent* Agent = CachedMovementAgent;
    if (MovementCommand)
        MovementCommand->ClearMovementGoal();

    const bool bWasPathMoving = bPathMoving;
    bPathMoving = false;
    RequestedMoveDirection = ECMAggressiveMoveDirection::None;
    ActivePathPoints.Reset();
    ActivePathPointIndex = INDEX_NONE;
    ActiveNavigationData = nullptr;
    ClearPathDebug();
    if (bBroadcastResult && bWasPathMoving && Agent)
    {
        Agent->HandleAggressivePathMoveCompleted(Result);
    }
}

// 원본 경로와 현재 목표 경로점을 이 컴포넌트 전용 배치로 표시한다.
void UCMAggressiveOmnidirectionalPathComponent::DrawPathDebug()
{
#if !UE_BUILD_SHIPPING && ENABLE_DRAW_DEBUG
    ClearPathDebug();
    if (!bDrawAggressivePathDebug)
        return;

    UWorld* World = GetWorld();
    ULineBatchComponent* LineBatcher = World ? World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent) : nullptr;
    if (!LineBatcher)
        return;

    for (int32 PathPointIndex = 0; PathPointIndex < ActivePathPoints.Num(); ++PathPointIndex)
    {
        const FVector DebugLocation = ActivePathPoints[PathPointIndex] + FVector(0.0f, 0.0f, PathDebugHeight);
        const FLinearColor PointColor = PathPointIndex == ActivePathPointIndex ? FLinearColor::Red : FLinearColor::Green;
        LineBatcher->DrawSphere(DebugLocation, 20.0f, 12, PointColor, PathDebugLifetime, 0, 3.0f, PathDebugBatchId);
        if (PathPointIndex + 1 < ActivePathPoints.Num())
            LineBatcher->DrawLine(DebugLocation, ActivePathPoints[PathPointIndex + 1] + FVector(0.0f, 0.0f, PathDebugHeight), FLinearColor::Green, 0, 4.0f, PathDebugLifetime, PathDebugBatchId);
    }
#endif
}

void UCMAggressiveOmnidirectionalPathComponent::RefreshPathDebug()
{
    DrawPathDebug();
}

// 이 컴포넌트가 생성한 NavMesh 경로 디버그 표시만 제거한다.
void UCMAggressiveOmnidirectionalPathComponent::ClearPathDebug()
{
#if !UE_BUILD_SHIPPING && ENABLE_DRAW_DEBUG
    UWorld* World = GetWorld();
    ULineBatchComponent* LineBatcher = World ? World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent) : nullptr;
    if (LineBatcher && PathDebugBatchId != 0)
        LineBatcher->ClearBatch(PathDebugBatchId);
#endif
}
