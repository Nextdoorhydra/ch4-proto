#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "CMStageRouteSubsystem.generated.h"

class UCMStageRouteDefinition;
struct FCMStageRouteEntry;

UCLASS()
// 맵 전환에도 유지되어야 하는 서버 스테이지 경로 진행 인덱스 관리
class CHIMERA_API UCMStageRouteSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 새 스테이지 경로를 첫 스테이지부터 시작
    bool StartStageRoute(UCMStageRouteDefinition* Definition);

    // 새 스테이지 경로를 지정한 인덱스부터 시작
    bool StartStageRouteAtIndex(
        UCMStageRouteDefinition* Definition,
        int32 InitialStageIndex);

    // 현재 스테이지 경로 진행 상태 초기화
    void ResetStageRoute();

    // 다음 스테이지를 Pending으로 예약하되 현재 인덱스는 유지
    bool PrepareNextStage();

    // 지정한 스테이지를 예약하며 현재 진행도와 접속 인원 설정은 보존
    bool PrepareStageAtIndex(int32 StageIndex);

    // 새 맵 도착 후 Pending 인덱스를 현재 스테이지로 확정
    bool CommitPendingStage();

    // Travel 실패 시 Pending 전환 취소
    void CancelPendingStage();

    // 현재 스테이지 경로와 진행 인덱스가 유효한지 확인
    bool IsStageRouteActive() const;

    // 현재 경로가 클리어·실패 후 같은 스테이지를 반복하는지 확인
    bool ShouldLoopCurrentStage() const;

    // 현재 스테이지가 경로의 마지막인지 확인
    bool IsLastStage() const;

    // 현재 스테이지 인덱스 반환
    int32 GetCurrentStageIndex() const { return CurrentStageIndex; }

    // 맵 전환 대상으로 예약된 스테이지 인덱스 반환
    int32 GetPendingStageIndex() const { return PendingStageIndex; }

    // 전체 스테이지 수 반환
    int32 GetStageCount() const;

    // 현재 스테이지 설정 반환
    const FCMStageRouteEntry* GetCurrentStage() const;

    // 맵 전환 대상으로 예약된 스테이지 설정 반환
    const FCMStageRouteEntry* GetPendingStage() const;

    // 현재 사용 중인 스테이지 경로 정의 반환
    UCMStageRouteDefinition* GetStageRouteDefinition() const { return StageRouteDefinition; }

    void SetSoloTestMode(bool bEnabled) { bSoloTestMode = bEnabled; }
    bool IsSoloTestMode() const { return bSoloTestMode; }

    void SetExpectedPlayerCount(int32 PlayerCount)
    {
        ExpectedPlayerCount = FMath::Max(0, PlayerCount);
    }
    int32 GetExpectedPlayerCount() const { return ExpectedPlayerCount; }

private:
    UPROPERTY(Transient)
    TObjectPtr<UCMStageRouteDefinition> StageRouteDefinition;

    int32 CurrentStageIndex = INDEX_NONE;
    int32 PendingStageIndex = INDEX_NONE;
    int32 ExpectedPlayerCount = 0;
    bool bSoloTestMode = false;
};
