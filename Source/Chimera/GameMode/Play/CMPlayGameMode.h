#pragma once

#include "CoreMinimal.h"
#include "GameMode/CMGameFlowTypes.h"
#include "GameMode/CMGameMode.h"
#include "Player/CMControlTypes.h"

#include "CMPlayGameMode.generated.h"

class ACMStageDirector;
class ACMPlayerController;
class UCMStageRouteDefinition;
class UCMStageLoadBarrierComponent;
enum class ECMStageLoadState : uint8;
enum class ECMStageLoadFailureReason : uint8;

struct FCMQueuedStageLoadRequest
{
    FPrimaryAssetId ScheduleId;
};

// 한 스테이지 안에서 재접속을 위해 보존하는 서버 전용 조작 배정
struct FCMDisconnectedPlayerRecord
{
    int32 PlayerSlotId = INDEX_NONE;
    int32 PlayerColorIndex = INDEX_NONE;
    int32 OwnedSegmentIndex = INDEX_NONE;
    TArray<FCMPartSlotAddress> ControlSlots;
    FTimerHandle ExpirationTimer;
};

UCLASS()
// 상위 플레이 흐름·스테이지 진행 결정 담당
class CHIMERA_API ACMPlayGameMode : public ACMGameMode
{
    GENERATED_BODY()

public:
    ACMPlayGameMode();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void RestartPlayer(AController* NewPlayer) override;

    // 전역 플레이 Phase·제한 시간 변경 처리
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Game Flow")
    void SetPlayPhase(ECMPlayPhase NewPhase, float Duration = 0.0f);

    // 현재 인덱스·전체 스테이지 수 초기화 처리
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Game Flow")
    void InitializeStageProgress(int32 StageIndex, int32 StageCount);

    // 현재 맵의 StageDirector를 서버 게임 흐름에 등록
    bool RegisterStageDirector(ACMStageDirector* NewStageDirector);

    // 준비 완료 후 시작 연출 Phase 진입
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Game Flow")
    bool StartStage();

    // StageDirector의 시작 연출 완료 보고 처리
    bool HandleStartingPresentationFinished(ACMStageDirector* ReportingDirector);

    // StageDirector의 결과 연출 완료 보고를 받아 다음 스테이지 전환 시작
    bool HandleResultPresentationFinished(ACMStageDirector* ReportingDirector);

    // StageDirector의 현재 스테이지 클리어 보고 처리
    bool HandleStageCompleted(ACMStageDirector* ReportingDirector);

    // StageDirector의 현재 스테이지 실패 보고 처리
    bool HandleStageFailed(ACMStageDirector* ReportingDirector);

    // 전체 패배 확정 후 Defeat Phase 진입
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Game Flow")
    void ConfirmGameDefeat();

    // 승리·패배 결과 표시 후 Ending Phase 진입
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Game Flow")
    void StartEnding();

    // PlayerController가 보고한 로컬 로드 결과를 현재 서버 배리어에 반영
    void HandleStageLoadComplete(
        ACMPlayerController* ReportingController,
        FGuid RequestId,
        bool bSucceeded);

    // 실제 레벨 스트리밍 또는 Travel 구현이 연결될 스테이지 전환 경계
    UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void BeginStageTransition(int32 NextStageIndex);
    virtual void BeginStageTransition_Implementation(int32 NextStageIndex);

    // 레벨 전환 준비가 끝난 뒤 다음 Stage.Entry 로드를 시작
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void FinishStageTransition();

protected:
    // 현재 스테이지 이탈자의 파츠·슬롯 배정을 유예 시간 동안 보존
    virtual bool ShouldPreservePlayerOnLogout(
        AController* Exiting,
        const ACMPlayerState* ExitingPlayerState) const override;

    // 유예 시간 안에 돌아온 플레이어에게 기존 배정을 그대로 복원
    virtual bool RestorePreservedControlAssignment(
        AController* NewPlayer,
        ACMChimera* SharedChimera) override;

    // 로비를 거치지 않고 직접 실행한 개발 맵에서 사용할 기본 스테이지 경로 PDA
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|StageRoute")
    TObjectPtr<UCMStageRouteDefinition> DefaultStageRouteDefinition;

    // 첫 스테이지의 Stage.Entry 로드를 시작할 Schedule PDA
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Loading")
    FPrimaryAssetId InitialStageLoadScheduleId;

    // 스테이지 진행 순서대로 사용할 로드 Schedule PDA 목록
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Loading")
    TArray<FPrimaryAssetId> StageLoadScheduleIds;

    // Blocking 로드 완료를 기다리는 최대 시간, 0이면 제한 없음
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Loading", meta = (ClampMin = "0.0"))
    float StageLoadTimeout = 60.0f;

    // 시작 연출 완료 보고를 기다리는 최대 시간, 초과 시 자동 진행 없이 오류로 알림
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Stage", meta = (ClampMin = "0.0"))
    float StartingPresentationTimeout = 30.0f;

    // 반복 Route에서 결과 확인 후 현재 맵을 다시 여는 대기 시간
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Testing", meta = (ClampMin = "0.0"))
    float LoopingStageRestartDelay = 1.0f;

    // 접속 종료 후 기존 파츠와 조작 배정을 유지하는 시간
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Reconnect", meta = (ClampMin = "0.0"))
    float ReconnectGracePeriod = 60.0f;

private:
    // 에디터 직접 실행 시 현재 맵에 해당하는 스테이지 경로를 시작
    bool TryBootstrapDirectStageRoute(
        class UCMStageRouteSubsystem* StageRoute);

    bool IsCurrentStageDirector(const ACMStageDirector* Director) const;
    void TryStartStageWhenReady();
    void BindSharedChimeraEvents();

    UFUNCTION()
    void HandleAllSegmentsDead();

    bool PublishStageLoadRequest(FPrimaryAssetId ScheduleId);
    bool StartStageLoadRequest(const FCMQueuedStageLoadRequest& Request);
    void StartNextQueuedStageLoadRequest();
    void HandleLoadBarrierCompleted(FGuid RequestId);
    void HandleLoadBarrierFailed(
        FGuid RequestId,
        ECMStageLoadState State,
        ECMStageLoadFailureReason Reason);
    FPrimaryAssetId GetStageLoadScheduleId(int32 StageIndex) const;
    void CompleteActiveStageLoad();
    void FailActiveStageLoad(ECMStageLoadState State, ECMStageLoadFailureReason Reason);
    void AdvanceToNextStage();

    // 반복 Route라면 현재 스테이지 재시작 예약
    bool TryScheduleStageLoopRestart();

    // 현재 맵을 다시 열어 키메라와 모든 레벨 상태 초기화
    void RestartLoopingStage();

    // 시작 연출이 완료되지 않아 Starting에 멈춘 상태를 오류로 보고
    void HandleStartingPresentationTimeout();

    // 온라인 ID를 우선 사용하고 PIE에서는 플레이어 이름으로 재접속 키 생성
    FString BuildReconnectKey(const ACMPlayerState* PlayerState) const;

    // 유예가 끝난 플레이어의 담당 파츠를 탈착하고 활성 인원에서 제거
    void ExpireDisconnectedPlayer(FString ReconnectKey);

    // 현재 스테이지 진행 중 새로 들어온 사용자를 관전 상태로 전환
    bool ShouldSpectateCurrentStage(const ACMPlayerState* PlayerState) const;

    UPROPERTY(Transient)
    TObjectPtr<ACMStageDirector> StageDirector;

    // 현재 플레이 월드에서 반복 사용하는 PlayGameState 참조
    UPROPERTY(Transient)
    TObjectPtr<class ACMPlayGameState> CachedPlayGameState;

    UPROPERTY(VisibleAnywhere, Category = "Chimera|Loading")
    TObjectPtr<UCMStageLoadBarrierComponent> StageLoadBarrier;

    FGuid ActiveStageLoadRequestId;
    int32 PendingStageTransitionIndex = INDEX_NONE;
    TArray<FCMQueuedStageLoadRequest> QueuedStageLoadRequests;
    FTimerHandle StartingPresentationTimeoutHandle;
    FTimerHandle StageLoopRestartTimerHandle;
    bool bStageLoadReady = false;
    bool bStageLoopRestartScheduled = false;
    TMap<FString, FCMDisconnectedPlayerRecord> DisconnectedPlayers;
    TSet<FString> ExpiredReconnectKeys;
};
