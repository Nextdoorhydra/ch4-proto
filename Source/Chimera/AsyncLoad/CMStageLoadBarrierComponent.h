#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameMode/Play/CMPlayGameState.h"

#include "CMStageLoadBarrierComponent.generated.h"

class ACMPlayerController;

DECLARE_MULTICAST_DELEGATE_OneParam(FCMStageLoadBarrierCompleted, FGuid);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FCMStageLoadBarrierFailed,
    FGuid,
    ECMStageLoadState,
    ECMStageLoadFailureReason);

UCLASS(ClassGroup = (Chimera))
// 서버의 플레이어별 로드 완료 집계, 타임아웃, 접속 인원 재계산 담당
class CHIMERA_API UCMStageLoadBarrierComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    void BeginBarrier(
        FGuid RequestId,
        bool bBlocking,
        float TimeoutSeconds,
        int32 ExpectedPlayerCount);
    void ReportPlayerResult(ACMPlayerController* Controller, FGuid RequestId, bool bSucceeded);
    void HandlePlayerJoined();
    void HandlePlayerLeft(ACMPlayerController* Controller);
    void CancelBarrier();

    int32 GetReadyPlayerCount() const { return ReadyControllers.Num(); }
    int32 GetTargetPlayerCount() const;

    FCMStageLoadBarrierCompleted OnBarrierCompleted;
    FCMStageLoadBarrierFailed OnBarrierFailed;

private:
    void RefreshProgress();
    bool AreAllPlayersReady() const;
    void HandleTimeout();
    void FinishFailure(ECMStageLoadState State, ECMStageLoadFailureReason Reason);

    FGuid ActiveRequestId;
    bool bBlockingRequest = false;
    float ActiveTimeout = 0.0f;
    int32 ExpectedTargetPlayerCount = 0;
    TSet<TWeakObjectPtr<ACMPlayerController>> ReadyControllers;
    FTimerHandle TimeoutHandle;
};
