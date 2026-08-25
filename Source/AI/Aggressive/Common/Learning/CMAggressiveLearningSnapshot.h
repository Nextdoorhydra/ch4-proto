#pragma once

#include "CoreMinimal.h"

class ULearningAgentsCritic;
class ULearningAgentsPolicy;

/** 공용 스냅샷 구현에서 사용할 AI 정책 프로필이다. */
enum class ECMAggressiveLearningSnapshotProfile : uint8
{
    Tetra,
    Ripper,
    Centipede
};

namespace CMAggressiveLearningSnapshot
{
    /** 기존 학습 결과와 호환되는 정책 버전의 루트 절대 경로를 반환한다. */
    AI_API FString GetRootDirectory(ECMAggressiveLearningSnapshotProfile Profile);

    /** 이어서 학습하고 추론에 사용할 최신 네트워크 폴더를 반환한다. */
    AI_API FString GetLatestDirectory(ECMAggressiveLearningSnapshotProfile Profile);

    /** 이전 버전 정책으로 초기화할 폴더를 반환한다. 프로필에 없으면 빈 문자열이다. */
    AI_API FString GetBootstrapDirectory(ECMAggressiveLearningSnapshotProfile Profile);

    /** 덮어쓰지 않는 학습 체크포인트의 루트 폴더를 반환한다. */
    AI_API FString GetCheckpointRootDirectory(ECMAggressiveLearningSnapshotProfile Profile);

    /** 전체 판단 수로 구분되는 체크포인트 폴더를 반환한다. */
    AI_API FString GetCheckpointDirectory(ECMAggressiveLearningSnapshotProfile Profile, int64 TotalDecisionCount);

    /** 지정 폴더에 정책과 보조 네트워크 네 개를 저장한다. */
    AI_API bool SaveTrainingNetworks(ECMAggressiveLearningSnapshotProfile Profile, ULearningAgentsPolicy& Policy, ULearningAgentsCritic& Critic, const FString& Directory);

    /** 지정 폴더에 스냅샷 파일이 하나 이상 존재하는지 반환한다. */
    AI_API bool HasAnyTrainingSnapshots(ECMAggressiveLearningSnapshotProfile Profile, const FString& Directory);

    /** 지정 폴더에 학습 네트워크 네 개가 모두 존재하는지 반환한다. */
    AI_API bool HasCompleteTrainingSnapshots(ECMAggressiveLearningSnapshotProfile Profile, const FString& Directory);

    /** 지정 폴더의 학습 네트워크 네 개를 호환성 검사 후 불러온다. */
    AI_API bool LoadTrainingNetworks(ECMAggressiveLearningSnapshotProfile Profile, ULearningAgentsPolicy& Policy, ULearningAgentsCritic& Critic, const FString& Directory);

    /** 최신 폴더의 추론 네트워크 세 개를 호환성 검사 후 불러온다. */
    AI_API bool LoadInferenceNetworks(ECMAggressiveLearningSnapshotProfile Profile, ULearningAgentsPolicy& Policy);
}
