#pragma once

#include "CoreMinimal.h"
#include "LearningAgentsTrainingEnvironment.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"

#include "CMAggressiveLearningTrainingEnvironment.generated.h"

class AActor;
class UCMAggressiveMovementCommandComponent;
class ULearningAgentsManager;
class UPrimitiveComponent;

/** 공격적 AI 이동 학습의 보상 크기와 에피소드 종료 조건이다. */
USTRUCT(BlueprintType)
struct AI_API FCMAggressiveLearningRewardSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI|Learning", meta = (ClampMin = "0.001"))
    float ProgressDistanceScale = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI|Learning", meta = (ClampMin = "0.0"))
    float ArrivalReward = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI|Learning", meta = (ClampMin = "0.0"))
    float MaximumFastArrivalReward = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI|Learning", meta = (ClampMin = "0.0"))
    float YawAngularVelocityPenaltyScale = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI|Learning", meta = (ClampMin = "0.0"))
    float FacingProgressRewardScale = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI|Learning", meta = (ClampMin = "0.0"))
    float StepPenalty = 0.001f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI|Learning", meta = (ClampMin = "0.1"))
    float MaxEpisodeSeconds = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI|Learning", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float MinimumUprightDot = 0.5f;
};

/** 8방향 이동 학습에 사용할 목표 거리와 도착 허용 반경이다. */
USTRUCT(BlueprintType)
struct AI_API FCMAggressiveLearningGoalSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI|Learning", meta = (ClampMin = "1.0"))
    float GoalDistance = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggressive AI|Learning", meta = (ClampMin = "0.0"))
    float AcceptanceRadius = 100.0f;
};

/** 공격적 AI 학습 에피소드가 종료된 원인이다. */
enum class ECMAggressiveLearningEpisodeEndReason : uint8
{
    None,
    Arrival,
    Overturned,
    Timeout,
    PolicyUpdate
};

namespace CMAggressiveLearningReward
{
    /** 이전과 현재 목표 거리로 한 판단 단계의 이동 보상을 계산한다. */
    AI_API float CalculateMovementReward(float PreviousDistance, float CurrentDistance, float YawAngularVelocity, bool bReachedGoal, float EpisodeTime, const FCMAggressiveLearningRewardSettings& Settings);

    /** 이전보다 목표 방향을 더 바라본 정도에 대한 회전 진행 보상을 계산한다. */
    AI_API float CalculateFacingProgressReward(float PreviousAlignment, float CurrentAlignment, const FCMAggressiveLearningRewardSettings& Settings);

    /** 도착, 전복, 제한시간을 기준으로 에피소드 완료 상태를 결정한다. */
    AI_API ELearningAgentsCompletion ResolveEpisodeCompletion(bool bReachedGoal, float UprightDot, float EpisodeTime, const FCMAggressiveLearningRewardSettings& Settings);
}

namespace CMAggressiveLearningGoal
{
    /** 시작 자세를 기준으로 지정한 로컬 방향의 월드 목표 위치를 계산한다. */
    AI_API FVector ResolveWorldGoalLocation(const FTransform& StartTransform, ECMAggressiveMoveDirection Direction, float GoalDistance);
}

namespace CMAggressiveLearningEpisode
{
    /** 현재 물리 상태와 경과시간으로 에피소드 종료 원인을 판정한다. */
    AI_API ECMAggressiveLearningEpisodeEndReason ResolveEndReason(bool bReachedGoal, float UprightDot, float EpisodeTime, const FCMAggressiveLearningRewardSettings& Settings);

    /** 최근 자연 종료 에피소드 기록에서 목표 도착 비율을 계산한다. */
    AI_API float CalculateArrivalRate(const TArray<uint8>& ArrivalHistory);
}

/** 공격적 AI 이동 정책의 보상, 완료 판정, 물리 상태 초기화를 담당한다. */
UCLASS(BlueprintType)
class AI_API UCMAggressiveLearningTrainingEnvironment : public ULearningAgentsTrainingEnvironment
{
    GENERATED_BODY()

public:
    /** 지정한 보상 설정으로 공격적 AI 학습 환경을 생성한다. */
    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Learning")
    static UCMAggressiveLearningTrainingEnvironment* MakeAggressiveTrainingEnvironment(UPARAM(ref) ULearningAgentsManager*& InManager, FCMAggressiveLearningRewardSettings InRewardSettings, FCMAggressiveLearningGoalSettings InGoalSettings, FName Name = TEXT("AggressiveMovementTrainingEnvironment"));

    virtual void OnAgentsAdded_Implementation(const TArray<int32>& AgentIds) override;
    virtual void OnAgentsRemoved_Implementation(const TArray<int32>& AgentIds) override;
    virtual void GatherAgentReward_Implementation(float& OutReward, int32 AgentId) override;
    virtual void GatherAgentCompletion_Implementation(ELearningAgentsCompletion& OutCompletion, int32 AgentId) override;
    virtual void ResetAgentEpisode_Implementation(int32 AgentId) override;

private:
    bool TryGetAgentComponents(int32 AgentId, AActor*& OutAgent, UPrimitiveComponent*& OutBody, UCMAggressiveMovementCommandComponent*& OutMovementCommand);
    void CaptureInitialAgentState(int32 AgentId);
    void LogEpisodeSummary(int32 AgentId, UPrimitiveComponent& Body, UCMAggressiveMovementCommandComponent& MovementCommand);
    void ResetEpisodeStatistics(int32 AgentId);
    void SetNextTrainingGoal(int32 AgentId, UCMAggressiveMovementCommandComponent& MovementCommand);
    void UpdatePreviousGoalState(int32 AgentId, UPrimitiveComponent& Body, UCMAggressiveMovementCommandComponent& MovementCommand);

    UPROPERTY(VisibleAnywhere, Category = "Aggressive AI|Learning")
    FCMAggressiveLearningRewardSettings RewardSettings;

    UPROPERTY(VisibleAnywhere, Category = "Aggressive AI|Learning")
    FCMAggressiveLearningGoalSettings GoalSettings;

    TArray<FTransform> InitialBodyTransforms;
    TArray<FVector> PreviousGoalLocations;
    TArray<float> PreviousGoalDistances;
    TArray<float> PreviousFacingAlignments;
    TArray<int32> NextGoalDirectionIndices;
    TArray<int64> EpisodeNumbers;
    TArray<ECMAggressiveMoveDirection> EpisodeStartDirections;
    TArray<float> EpisodeCumulativeRewards;
    TArray<float> EpisodeMinimumDistances;
    TArray<int32> EpisodeStepCounts;
    TArray<ECMAggressiveLearningEpisodeEndReason> PendingEndReasons;
    TArray<TArray<uint8>> RecentArrivalHistories;
    TArray<int64> TotalNaturalEpisodeCounts;
    TArray<int64> TotalArrivalCounts;
    TBitArray<> HasInitialBodyTransform;
    TBitArray<> HasPreviousGoalState;
};
