#pragma once

#include "CoreMinimal.h"
#include "GameMode/CMGameFlowTypes.h"
#include "GameMode/CMGameState.h"

#include "CMLobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraLobbyStateChanged);

UCLASS()
// 로비 흐름·Ready 집계·게임 시작 가능 여부 복제 담당
class CHIMERA_API ACMLobbyGameState : public ACMGameState
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    // 서버 권한으로 로비 흐름 상태 변경 처리
    void SetLobbyPhase(ECMLobbyPhase NewPhase);

    // 서버 권한으로 Ready 인원·게임 시작 가능 여부 반영 처리
    void SetLobbySummary(int32 NewReadyCount, bool bNewCanStart);

    UFUNCTION(BlueprintPure, Category = "Chimera|Lobby")
    ECMLobbyPhase GetLobbyPhase() const { return LobbyPhase; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Lobby")
    int32 GetReadyPlayerCount() const { return ReadyPlayerCount; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Lobby")
    bool CanStartGame() const { return bCanStartGame; }

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Lobby")
    FChimeraLobbyStateChanged OnLobbyStateChanged;

private:
    UFUNCTION()
    void OnRep_LobbyState();

    // Waiting·Loading 로비 흐름 상태
    UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
    ECMLobbyPhase LobbyPhase = ECMLobbyPhase::Waiting;

    // Ready 완료 참가자 수
    UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
    int32 ReadyPlayerCount = 0;

    // 게임 시작 조건 충족 여부
    UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
    bool bCanStartGame = false;
};
