#pragma once

#include "CoreMinimal.h"
#include "GameMode/CMGameFlowTypes.h"
#include "GameMode/CMGameMode.h"

#include "CMPlayGameMode.generated.h"

class ACMStageDirector;
class ACMPlayerController;
class UCMCampaignDefinition;
class UCMStageLoadBarrierComponent;
enum class ECMStageLoadState : uint8;
enum class ECMStageLoadFailureReason : uint8;

struct FCMQueuedStageLoadRequest
{
    FPrimaryAssetId ScheduleId;
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
    void HandleStartingPresentationFinished(ACMStageDirector* ReportingDirector);

    // StageDirector의 결과 연출 완료 보고를 받아 다음 스테이지 전환 시작
    void HandleResultPresentationFinished(ACMStageDirector* ReportingDirector);

    // StageDirector의 현재 스테이지 클리어 보고 처리
    void HandleStageCompleted(ACMStageDirector* ReportingDirector);

    // StageDirector의 현재 스테이지 실패 보고 처리
    void HandleStageFailed(ACMStageDirector* ReportingDirector);

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
    // 로비에서 캠페인이 시작되지 않은 개발 맵에서 사용할 기본 캠페인 PDA
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Campaign")
    TObjectPtr<UCMCampaignDefinition> DefaultCampaignDefinition;

    // 첫 스테이지의 Stage.Entry 로드를 시작할 Schedule PDA
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Loading")
    FPrimaryAssetId InitialStageLoadScheduleId;

    // 캠페인 진행 순서대로 사용할 스테이지 Schedule PDA 목록
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Loading")
    TArray<FPrimaryAssetId> StageLoadScheduleIds;

    // Blocking 로드 완료를 기다리는 최대 시간, 0이면 제한 없음
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Loading", meta = (ClampMin = "0.0"))
    float StageLoadTimeout = 60.0f;

private:
    bool IsCurrentStageDirector(const ACMStageDirector* Director) const;
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

    UPROPERTY(Transient)
    TObjectPtr<ACMStageDirector> StageDirector;

    UPROPERTY(VisibleAnywhere, Category = "Chimera|Loading")
    TObjectPtr<UCMStageLoadBarrierComponent> StageLoadBarrier;

    FGuid ActiveStageLoadRequestId;
    int32 PendingStageTransitionIndex = INDEX_NONE;
    TArray<FCMQueuedStageLoadRequest> QueuedStageLoadRequests;
};
