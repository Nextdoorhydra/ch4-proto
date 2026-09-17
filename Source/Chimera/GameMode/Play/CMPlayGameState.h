#pragma once

#include "CoreMinimal.h"
#include "GameMode/CMGameFlowTypes.h"
#include "GameMode/CMGameState.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "CMPlayGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraPlayStateChanged);

UENUM(BlueprintType)
enum class ECMStageLoadState : uint8
{
    None,       // 실행 중인 스테이지 로드 요청 없음
    Loading,    // 클라이언트의 로드 완료 보고 대기 중
    Ready,      // 현재 요청의 모든 대상 플레이어 로드 완료
    Failed,     // 클라이언트가 로드 실패를 보고함
    TimedOut    // 제한 시간 안에 전원이 완료하지 못함
};

UENUM(BlueprintType)
enum class ECMStageLoadFailureReason : uint8
{
    None,                   // 실패 없음
    ClientReportedFailure,  // 클라이언트 로컬 로더가 실패를 보고함
    TimedOut,               // 서버 로드 배리어 제한 시간 초과
    InvalidConfiguration    // 현재 스테이지의 Map 또는 Schedule 설정이 유효하지 않음
};

UENUM(BlueprintType)
enum class ECMStagePresentationState : uint8
{
    None,
    Starting,
    Result
};

USTRUCT(BlueprintType)
struct FCMStageLoadRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid RequestId;

    UPROPERTY(BlueprintReadOnly)
    FPrimaryAssetId ScheduleId;

    UPROPERTY(BlueprintReadOnly)
    bool bBlocking = false;

    bool IsValid() const
    {
        return RequestId.IsValid()
            && ScheduleId.IsValid();
    }
};

USTRUCT(BlueprintType)
struct FCMStageLoadSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FCMStageLoadRequest Request;
    UPROPERTY(BlueprintReadOnly)
    ECMStageLoadState State = ECMStageLoadState::None;
    UPROPERTY(BlueprintReadOnly)
    ECMStageLoadFailureReason FailureReason = ECMStageLoadFailureReason::None;
    UPROPERTY(BlueprintReadOnly)
    int32 ReadyPlayerCount = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 TargetPlayerCount = 0;
    UPROPERTY(BlueprintReadOnly)
    double StartServerTime = 0.0;
    UPROPERTY(BlueprintReadOnly)
    float TimeoutDuration = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    int32 Revision = 0;
};

USTRUCT(BlueprintType)
struct FCMRetryVoteSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bActive = false;

    UPROPERTY(BlueprintReadOnly)
    int32 VoteCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 EligiblePlayerCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 RequiredVoteCount = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<int32> VotedPlayerIds;

    UPROPERTY(BlueprintReadOnly)
    int32 Revision = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FChimeraStageLoadRequestChanged,
    const FCMStageLoadRequest&,
    Request
);

UCLASS()
// 플레이 흐름·스테이지 진행·서버 시간 동기화 담당
class CHIMERA_API ACMPlayGameState : public ACMGameState
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    // Phase 변경·시작 서버 시간 기록 처리
    void SetPlayPhase(ECMPlayPhase NewPhase, float NewDuration = 0.0f);

    // 현재 인덱스·전체 스테이지 수 설정 처리
    void SetStageProgress(int32 NewStageIndex, int32 NewStageCount, FGameplayTag NewStageBGMTag);
    void SetStagePresentationState(ECMStagePresentationState NewState);

    void SetRetryVoteState(
        bool bActive,
        const TArray<int32>& VotedPlayerIds,
        int32 EligiblePlayerCount,
        int32 RequiredVoteCount);

    // 서버가 모든 클라이언트에서 실행할 스테이지 로드 요청을 갱신
    void BeginStageLoadRequest(const FCMStageLoadRequest& NewRequest, int32 NewTargetPlayerCount, float NewTimeoutDuration);

    // 서버의 로드 배리어 상태와 UI용 진행도를 갱신
    void SetStageLoadStatus(
        ECMStageLoadState NewState,
        ECMStageLoadFailureReason NewFailureReason,
        int32 NewReadyPlayerCount,
        int32 NewTargetPlayerCount,
        float NewTimeoutDuration = 0.0f);

    UFUNCTION(BlueprintPure, Category = "Chimera|Game Flow")
    ECMPlayPhase GetPlayPhase() const { return PlayPhase; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Game Flow")
    int32 GetCurrentStageIndex() const { return CurrentStageIndex; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Game Flow")
    int32 GetTotalStageCount() const { return TotalStageCount; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Game Flow")
    float GetPhaseRemainingTime() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Game Flow")
    float GetCompletedStageTime() const { return CompletedStageTime; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Stage")
    ECMStagePresentationState GetStagePresentationState() const { return StagePresentationState; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Loading")
    const FCMStageLoadRequest& GetStageLoadRequest() const
    {
        return StageLoadSnapshot.Request;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Loading")
    ECMStageLoadState GetStageLoadState() const { return StageLoadSnapshot.State; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Loading")
    ECMStageLoadFailureReason GetStageLoadFailureReason() const { return StageLoadSnapshot.FailureReason; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Loading")
    int32 GetLoadReadyPlayerCount() const { return StageLoadSnapshot.ReadyPlayerCount; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Loading")
    int32 GetLoadTargetPlayerCount() const { return StageLoadSnapshot.TargetPlayerCount; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Loading")
    const FCMStageLoadSnapshot& GetStageLoadSnapshot() const { return StageLoadSnapshot; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Retry Vote")
    const FCMRetryVoteSnapshot& GetRetryVoteSnapshot() const
    {
        return RetryVoteSnapshot;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Loading")
    float GetStageLoadRemainingTime() const;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Game Flow")
    FChimeraPlayStateChanged OnPlayStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Loading")
    FChimeraStageLoadRequestChanged OnStageLoadRequestChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Loading")
    FChimeraPlayStateChanged OnStageLoadStatusChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Stage")
    FChimeraPlayStateChanged OnStagePresentationChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Retry Vote")
    FChimeraPlayStateChanged OnRetryVoteChanged;

private:
    void BroadcastPlayState() const;
    void HandlePlayStateRequest(FGameplayTag Channel, const struct FCMPlayStateRequest& Request);
    FGameplayMessageListenerHandle PlayStateRequestHandle;

    UFUNCTION()
    void OnRep_PlayState();

    UFUNCTION()
    void OnRep_StageLoadSnapshot();

    UFUNCTION()
    void OnRep_StagePresentationState();

    UFUNCTION()
    void OnRep_RetryVoteSnapshot();

    // 현재 클라이언트가 실행해야 하는 최신 스테이지 로드 요청
    UPROPERTY(ReplicatedUsing = OnRep_StageLoadSnapshot)
    FCMStageLoadSnapshot StageLoadSnapshot;

    UPROPERTY(ReplicatedUsing = OnRep_StagePresentationState)
    ECMStagePresentationState StagePresentationState = ECMStagePresentationState::None;

    UPROPERTY(ReplicatedUsing = OnRep_RetryVoteSnapshot)
    FCMRetryVoteSnapshot RetryVoteSnapshot;

    // 서버가 선택한 스테이지 음악. 클라이언트는 로컬 Route 없이 이 태그를 사용한다.
    UPROPERTY(ReplicatedUsing = OnRep_PlayState)
    FGameplayTag CurrentStageBGMTag;

    // 현재 전역 플레이 Phase
    UPROPERTY(ReplicatedUsing = OnRep_PlayState)
    ECMPlayPhase PlayPhase = ECMPlayPhase::Loading;

    // 0부터 시작하는 현재 스테이지 인덱스
    UPROPERTY(ReplicatedUsing = OnRep_PlayState)
    int32 CurrentStageIndex = 0;

    // 캠페인의 전체 스테이지 수
    UPROPERTY(ReplicatedUsing = OnRep_PlayState)
    int32 TotalStageCount = 1;

    // Phase 시작 서버 시간
    UPROPERTY(ReplicatedUsing = OnRep_PlayState)
    double PhaseStartServerTime = 0.0;

    // Phase 제한 시간, 0이면 제한 없음
    UPROPERTY(ReplicatedUsing = OnRep_PlayState)
    float PhaseDuration = 0.0f;

    // 직전 Playing Phase의 서버 기준 경과 시간
    UPROPERTY(ReplicatedUsing = OnRep_PlayState)
    float CompletedStageTime = 0.0f;
};
