#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"

#include "CMAggressiveOmnidirectionalPathComponent.generated.h"

class ANavigationData;
class ICMAggressiveMovementAgent;
class UCMAggressiveMovementCommandComponent;
class UNavigationSystemV1;
struct FNavDataConfig;

namespace CMAggressiveOmnidirectionalPath
{
    /** 몸체가 경로점 반경에 도착했거나 가까운 중간 경로점을 통과했는지 반환한다. */
    AI_API bool ShouldAdvancePathPoint(const FVector& BodyLocation, const FVector& SegmentStart, const FVector& PathPoint, float AcceptanceRadius, float PassDetectionRadius, bool bFinalPathPoint);

} // namespace CMAggressiveOmnidirectionalPath

/** 전용 NavMesh 경로점을 따라가며 코드 또는 정책에 로컬 8방향을 제공하는 컴포넌트다. */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class AI_API UCMAggressiveOmnidirectionalPathComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMAggressiveOmnidirectionalPathComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Path Movement")
    bool StartPathMove(FVector WorldGoal, float AcceptanceRadius = 50.0f);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Path Movement")
    void StopPathMove();

    void PausePathMoveForRecovery();

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Path Movement")
    bool IsPathMoving() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Path Movement")
    int32 GetActivePathPointIndex() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Path Movement")
    int32 GetPathPointCount() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Path Movement")
    float GetPathUpdateInterval() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Path Movement")
    float GetIntermediateAcceptanceRadius() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Path Movement")
    float GetIntermediatePathPointJitterRadius() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Path Movement")
    float GetMinimumPathPointSpacing() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Path Movement")
    FName GetNavigationAgentName() const;

    /** 지정한 시작점에서 목적지까지 완전한 NavMesh 경로 길이를 계산한다. */
    bool CalculateNavigationPathLength(FVector StartLocation, FVector WorldGoal, float& OutPathLength);

    /** 목표가 허용 반경 안의 NavMesh에 투영되고 현재 위치에서 완전한 경로로 연결되는지 반환한다. */
    bool IsNavigationGoalReachable(FVector WorldGoal, float GoalTolerance) const;

    /** 이 AI에 지정된 전용 NavMesh에서 도달 가능한 임의 위치를 찾는다. */
    bool FindRandomReachableLocation(FVector Origin, float Radius, FVector& OutLocation) const;

    /** 현재 위치가 전용 NavMesh 밖이면 가장 가까운 복귀 위치를 반환한다. */
    bool FindNavigationRecoveryLocation(FVector& OutLocation);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Path Movement")
    void SetPolicyControlEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Path Movement")
    bool IsPolicyControlEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Path Movement")
    ECMAggressiveMoveDirection GetRequestedMoveDirection() const;

    void SetNavigationAgentName(FName InNavigationAgentName);
    void SetIntermediatePathPointTolerances(float AcceptanceRadius, float PassDetectionRadius);
    void SetMaximumPathSegmentLength(float MaximumSegmentLength);
    void SetIntermediatePathPointJitterRadius(float JitterRadius);
    void SetIntermediatePathPointWallClearance(float WallClearance);
    void SetIntermediatePathPointNavigationClearance(float NavigationClearance);
    void SetMinimumPathPointSpacing(float MinimumSpacing);
    void SetRebuildPathWhenIntermediatePointPassed(bool bEnabled);
    void SetLearningRequestedMoveDirection(ECMAggressiveMoveDirection Direction);
    void RefreshPathDebug();

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement", meta = (ClampMin = "0.01"))
    float PathUpdateInterval = 0.05f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement", meta = (ClampMin = "0.0"))
    float IntermediateAcceptanceRadius = 40.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement", meta = (ClampMin = "0.0"))
    float IntermediatePassDetectionRadius = 80.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement", meta = (ClampMin = "0.0"))
    float MaximumPathSegmentLength = 0.0f;

    /** 시작점과 최종 목표를 제외한 경유지에 적용할 NavMesh 내부 평면 지터 반경이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement", meta = (ClampMin = "0.0"))
    float IntermediatePathPointJitterRadius = 0.0f;

    /** 중간 경유지를 정적 벽 표면에서 밀어낼 목표 거리다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement", meta = (ClampMin = "0.0"))
    float IntermediatePathPointWallClearance = 0.0f;

    /** 중간 경유점을 전용 NavMesh 경계에서 안쪽으로 밀어낼 추가 거리다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement", meta = (ClampMin = "0.0"))
    float IntermediatePathPointNavigationClearance = 0.0f;

    /** NavMesh 직선 통과가 가능한 가까운 경유지를 병합할 최소 간격이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement", meta = (ClampMin = "0.0"))
    float MinimumPathPointSpacing = 0.0f;

    /** 중간 경유지를 허용 반경 밖으로 지나치면 현재 위치에서 전체 경로를 다시 계산한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement")
    bool bRebuildPathWhenIntermediatePointPassed = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement")
    FVector NavigationProjectionExtent = FVector(200.0f, 200.0f, 200.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement", meta = (ClampMin = "0.0"))
    float NavigationContainmentTolerance = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement")
    FVector NavigationRecoveryProjectionExtent = FVector(1000.0f, 1000.0f, 500.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement")
    FName NavigationAgentName = NAME_None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement")
    bool bPolicyControlEnabled = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Path Movement")
    ECMAggressiveMoveDirection RequestedMoveDirection = ECMAggressiveMoveDirection::None;

private:
    bool BuildNavigationPath(FVector WorldGoal);
    bool AdjustPathPointsAwayFromBoundaries(UWorld& World, UNavigationSystemV1& NavigationSystem, const ANavigationData& NavigationData);
    bool HasPathClearance(UWorld& World, const ANavigationData& NavigationData) const;
    bool CalculateStaticObstacleRepulsion(UWorld& World, FVector Location, FVector& OutRepulsion) const;
    bool CalculateNavigationBoundaryRepulsion(const ANavigationData& NavigationData, FVector Location, FVector& OutRepulsion) const;
    bool FindNavigationAgent(const UNavigationSystemV1& NavigationSystem, FNavDataConfig& OutAgentConfig, const ANavigationData*& OutNavigationData) const;
    bool UpdateMovementDirection();
    void UpdatePathMove();
    void FinishPathMove(ECMAggressivePathMoveResult Result, bool bBroadcastResult);
    void DrawPathDebug();
    void ClearPathDebug();

    TArray<FVector> ActivePathPoints;
    FVector ActiveWorldGoal = FVector::ZeroVector;
    FVector LastValidNavigationLocation = FVector::ZeroVector;
    int32 ActivePathPointIndex = INDEX_NONE;
    float FinalAcceptanceRadius = 50.0f;
    bool bPathMoving = false;
    bool bHasLastValidNavigationLocation = false;
    uint32 PathDebugBatchId = 0;
    const ANavigationData* ActiveNavigationData = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UCMAggressiveMovementCommandComponent> MovementCommand;

    ICMAggressiveMovementAgent* CachedMovementAgent = nullptr;
    FTimerHandle PathUpdateTimerHandle;
};
