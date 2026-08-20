#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "CMPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraPlayerColorChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraPlayerReadyChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraPlayerSlotChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraParticipationStateChanged);

UENUM(BlueprintType)
// 로비 참가부터 플레이 이탈까지 플레이어의 현재 참여 상태 구분
enum class ECMPlayerParticipationState : uint8
{
    Lobby,            // 로비에서 게임 시작 대기
    Active,           // 담당 몸통 마디와 Q/W/E/R 입력 활성화
    Spectating,       // 현재 스테이지 관전 중
    Defeated,         // 담당 몸통 마디 파괴로 입력 불가
    Disconnected,     // 플레이 도중 접속 종료
    WaitingNextStage  // 중도 합류 후 다음 스테이지 활성화 대기
};

UCLASS()
class CHIMERA_API ACMPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    ACMPlayerState();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;
    virtual void SetPlayerName(const FString& NewPlayerName) override;
    virtual void OnRep_PlayerName() override;
    virtual void CopyProperties(APlayerState* PlayerState) override;
    virtual void ClientInitialize(AController* Controller) override;

    void SetPlayerColorIndex(int32 NewPlayerColorIndex);
    void SetReady(bool bNewReady);
    void SetPlayerSlotId(int32 NewPlayerSlotId);
    void SetParticipationState(ECMPlayerParticipationState NewState);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Vision")
    void SetVisionSystemEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Chimera|Vision")
    bool IsVisionSystemEnabled() const { return bVisionSystemEnabled; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Lobby")
    bool IsReady() const { return bReady; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Player")
    FLinearColor GetPlayerColor() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Player")
    int32 GetPlayerColorIndex() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Player")
    int32 GetPlayerSlotId() const { return PlayerSlotId; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Player")
    ECMPlayerParticipationState GetParticipationState() const
    {
        return ParticipationState;
    }

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Player")
    FChimeraPlayerColorChanged OnPlayerColorChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Lobby")
    FChimeraPlayerReadyChanged OnReadyChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Player")
    FChimeraPlayerSlotChanged OnPlayerSlotChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Player")
    FChimeraParticipationStateChanged OnParticipationStateChanged;

private:
    UFUNCTION()
    void OnRep_PlayerColorIndex();

    UFUNCTION()
    void OnRep_Ready();

    UFUNCTION()
    void OnRep_PlayerSlotId();

    UFUNCTION()
    void OnRep_ParticipationState();

    UFUNCTION()
    void OnRep_VisionSystemEnabled();

    UPROPERTY(ReplicatedUsing = OnRep_PlayerColorIndex)
    int32 PlayerColorIndex = INDEX_NONE;

    UPROPERTY(ReplicatedUsing = OnRep_Ready)
    bool bReady = false;

    // 입장 순서에 따라 한 번 배정되고 Seamless Travel에서도 유지되는 자리
    UPROPERTY(ReplicatedUsing = OnRep_PlayerSlotId)
    int32 PlayerSlotId = INDEX_NONE;

    UPROPERTY(ReplicatedUsing = OnRep_ParticipationState)
    ECMPlayerParticipationState ParticipationState =
        ECMPlayerParticipationState::Lobby;

    UPROPERTY(ReplicatedUsing = OnRep_VisionSystemEnabled)
    bool bVisionSystemEnabled = true;
};
