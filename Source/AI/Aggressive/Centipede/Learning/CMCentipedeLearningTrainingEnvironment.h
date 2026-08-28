#pragma once

#include "CoreMinimal.h"
#include "LearningAgentsTrainingEnvironment.h"

#include "Aggressive/Common/Learning/CMAggressiveLearningTrainingEnvironment.h"

#include "CMCentipedeLearningTrainingEnvironment.generated.h"

class ACMCentipedePawn;
class ULearningAgentsManager;

/** Centipede AI의 진행, 관절 추종, 전복과 에피소드 물리 초기화를 전담한다. */
UCLASS(BlueprintType)
class AI_API UCMCentipedeLearningTrainingEnvironment : public ULearningAgentsTrainingEnvironment
{
    GENERATED_BODY()

public:
    static UCMCentipedeLearningTrainingEnvironment*
    MakeCentipedeTrainingEnvironment(ULearningAgentsManager*& InManager, FCMAggressiveLearningRewardSettings InRewardSettings, FCMAggressiveLearningGoalSettings InGoalSettings, float InJointTrackingPenaltyScale, FName Name = TEXT("CentipedeTrainingEnvironment"));

    virtual void OnAgentsAdded_Implementation(const TArray<int32>& AgentIds) override;
    virtual void OnAgentsRemoved_Implementation(const TArray<int32>& AgentIds) override;
    virtual void GatherAgentReward_Implementation(float& OutReward, int32 AgentId) override;
    virtual void GatherAgentCompletion_Implementation(ELearningAgentsCompletion& OutCompletion, int32 AgentId) override;
    virtual void ResetAgentEpisode_Implementation(int32 AgentId) override;

    int64 GetCompletedEpisodeCount() const
    {
        return TotalCompletedEpisodeCount;
    }
    int64 GetSuccessfulEpisodeCount() const
    {
        return TotalSuccessfulEpisodeCount;
    }
    float GetSuccessRate() const;

private:
    ACMCentipedePawn* GetCentipedeAgent(int32 AgentId);
    void CaptureInitialState(int32 AgentId);
    void RecordPendingEpisodeResult(int32 AgentId);
    void SetNextGoal(int32 AgentId);
    void UpdatePreviousState(int32 AgentId);

    FCMAggressiveLearningRewardSettings RewardSettings;
    FCMAggressiveLearningGoalSettings GoalSettings;
    float JointTrackingPenaltyScale = 0.04f;
    TArray<FTransform> InitialHeadTransforms;
    TArray<FVector> PreviousGoalLocations;
    TArray<float> PreviousGoalDistances;
    TArray<int32> NextDirectionIndices;
    TArray<int32> EpisodeNumbers;
    TArray<ECMAggressiveLearningEpisodeEndReason> PendingEndReasons;
    TBitArray<> HasInitialState;
    TBitArray<> HasPreviousState;
    int64 TotalCompletedEpisodeCount = 0;
    int64 TotalSuccessfulEpisodeCount = 0;
};
