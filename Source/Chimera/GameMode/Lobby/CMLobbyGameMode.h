#pragma once

#include "CoreMinimal.h"
#include "GameMode/CMGameMode.h"

#include "CMLobbyGameMode.generated.h"

class UCMStageRouteDefinition;

UCLASS()
// 로비 참가자 입장·퇴장 처리 및 Ready 상태 집계 담당
class CHIMERA_API ACMLobbyGameMode : public ACMGameMode
{
    GENERATED_BODY()

public:
    ACMLobbyGameMode();

    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void GenericPlayerInitialization(AController* C) override;

    // 참가 인원·Ready 집계 결과 갱신 처리
    void RefreshLobbySummary();

    // 소유 PlayerController의 Ready 요청을 검증하고 로비 집계 갱신
    bool TrySetPlayerReady(APlayerController* RequestingPlayer, bool bReady);

    // 로비 조건과 캠페인 설정을 검증한 뒤 첫 스테이지 Travel 시작
    bool TryStartStageRoute(APlayerController* RequestingPlayer);

    // 로비 조건과 테스트 경로 설정을 검증한 뒤 테스트 맵 Travel 시작
    bool TryStartTestStageRoute(APlayerController* RequestingPlayer);

protected:
    // 이 로비에서 시작할 정식 캠페인 PDA
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|StageRoute")
    TObjectPtr<UCMStageRouteDefinition> StageRouteDefinition;

    // 로비의 테스트 시작 버튼이 사용할 반복 테스트 경로 PDA
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|StageRoute")
    TObjectPtr<UCMStageRouteDefinition> TestStageRouteDefinition;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Lobby", meta = (ClampMin = "2"))
    int32 MinimumPlayersToStart = 2;

private:
    // 선택한 Route를 서버 런타임에 등록하고 첫 맵으로 이동
    bool TryStartRouteDefinition(
        APlayerController* RequestingPlayer,
        UCMStageRouteDefinition* RouteDefinition,
        bool bTestRoute);

    // 현재 로비 월드에서 반복 사용하는 LobbyGameState 참조
    UPROPERTY(Transient)
    TObjectPtr<class ACMLobbyGameState> CachedLobbyGameState;

    bool bStageRouteStartInProgress = false;
};
