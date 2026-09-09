#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/CMControlTypes.h"
#include "Stage/Test/CMTestAreaTypes.h"

#include "CMPlayerController.generated.h"

class ACMChimera;
class ACMTestAreaManager;
class UCMVisionInputComponent;
class UInputAction;
class UInputMappingContext;
class UCMClientStageLoadComponent;
struct FInputActionValue;

UCLASS()
class CHIMERA_API ACMPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ACMPlayerController();

    UFUNCTION(BlueprintPure, Category = "Chimera|Game")
    bool CanRequestRetryGame() const;

    UFUNCTION(BlueprintCallable, Category = "Chimera|Game")
    void RequestRetryGame();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Game")
    void CancelRetryGameRequest();

    UFUNCTION(BlueprintPure, Category = "Chimera|Game")
    bool CanControlStageResult() const;

    UFUNCTION(BlueprintCallable, Category = "Chimera|Game")
    void RequestRestartCompletedStage();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Game")
    void RequestAdvanceCompletedStage();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Lobby")
    void RequestStartStageRoute();

    // 로비 UI에서 설정된 TestRoute 멀티플레이 시작 요청
    UFUNCTION(BlueprintCallable, Category = "Chimera|Lobby")
    void RequestStartTestStageRoute();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Lobby")
    void RequestSetReady(bool bReady);

    // 로컬 로드 컴포넌트의 결과를 서버 RPC로 전달
    void ReportLocalStageLoadComplete(FGuid RequestId, bool bSucceeded);

    /** Console-command entry point. The actual damage is always applied by the server. */
    void RequestCheatKillAllSegments();

    // 최신 룸 체크포인트로 공용 키메라 복구·이동을 서버에 요청
    void RequestCheatRespawnAtCheckpoint();

    void RequestCheatNextStage();

    void RequestCheatGoToStage(int32 OneBasedStageNumber);

    void RequestCheatGoToCheckpoint(int32 OneBasedCheckpointNumber);

    /** Console-command entry point using the zero-based body-segment index. */
    void RequestCheatKillSegment(int32 SegmentIndex);

    void RequestCheatDamageSegment(int32 SegmentIndex, float Damage);
    void RequestCheatDamagePart(int32 OneBasedSlotIndex, float Damage);
    void RequestCheatSetInvincible(bool bEnabled);

    void RequestCheatSpawnRandomParts();
    void RequestCheatAttachPart(int32 OneBasedSlotIndex, FName PartName);
    void RequestCheatFillAllSlotsWithPart(FName PartName);
    void RequestCheatClearRandomParts();
    void RequestCheatSpawnLegParts();
    void RequestCheatClearLegParts();
    // 콘솔 명령으로 현재 로컬 플레이어의 화살표 디버그 이동 활성화
    void SetCheatDebugMovementEnabled(bool bEnabled);

    // 호스트 또는 Standalone 개발 실행에서만 Test Area 이동 UI 허용
    UFUNCTION(BlueprintPure, Category = "Chimera|Testing")
    bool CanControlTestAreas() const;

    // Test Area 선택 UI가 표시할 현재 월드 시작점 목록 반환
    UFUNCTION(BlueprintCallable, Category = "Chimera|Testing")
    TArray<FCMTestAreaInfo> GetAvailableTestAreas() const;

    // 선택한 Test Area 시작점으로 공용 키메라 이동 요청
    UFUNCTION(BlueprintCallable, Category = "Chimera|Testing")
    void RequestTeleportToTestArea(FName AreaId);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Vision")
    TObjectPtr<UCMVisionInputComponent> VisionInputComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    int32 MappingPriority = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> FirstControlAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> SecondControlAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> ThirdControlAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> FourthControlAction;

    /** Hold this action while pressing Q/W/E/R to detach that control slot. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> DetachModifierAction;

    /** Hold this action while starting a Leg action to move backward. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> ReverseModifierAction;

    /** Local-only mouse-wheel input that changes the shared-body view distance. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> CameraDistanceAction;

private:
    void FirstControlKeyPressed();
    void SecondControlKeyPressed();
    void ThirdControlKeyPressed();
    void FourthControlKeyPressed();
    void FirstControlKeyReleased();
    void SecondControlKeyReleased();
    void ThirdControlKeyReleased();
    void FourthControlKeyReleased();
    void DetachModifierPressed();
    void DetachModifierReleased();
    void ReverseModifierPressed();
    void ReverseModifierReleased();
    void AdjustCameraDistance(const FInputActionValue& InputValue);
    void SetControlSlotPressed(int32 SlotIndex, bool bPressed);
    void SoloControlKeyPressed(FKey Key);
    void SoloControlKeyReleased(FKey Key);
    void SetSoloControlKeyPressed(FKey Key, bool bPressed);
    bool IsSoloTestMode() const;
    void DebugMoveForwardPressed();
    void DebugMoveForwardReleased();
    void DebugMoveBackwardPressed();
    void DebugMoveBackwardReleased();
    void DebugTurnLeftPressed();
    void DebugTurnLeftReleased();
    void DebugTurnRightPressed();
    void DebugTurnRightReleased();

    ACMChimera* GetSharedChimera() const;
    ACMTestAreaManager* FindTestAreaManager() const;

    /** SharedChimera가 복제된 순간에만 로컬 ViewTarget을 연결한다. */
    UFUNCTION()
    void HandleSharedChimeraChanged();

    UFUNCTION(Server, Reliable)
    void ServerRequestRetryGame();

    UFUNCTION(Server, Reliable)
    void ServerCancelRetryGameRequest();

    UFUNCTION(Server, Reliable)
    void ServerRequestRestartCompletedStage();

    UFUNCTION(Server, Reliable)
    void ServerRequestAdvanceCompletedStage();

    UFUNCTION(Server, Reliable)
    void ServerRequestStartStageRoute();

    UFUNCTION(Server, Reliable)
    void ServerRequestStartTestStageRoute();

    UFUNCTION(Server, Reliable)
    void ServerSetReady(bool bReady);

    UFUNCTION(Server, Reliable)
    void ServerReportStageLoadComplete(FGuid RequestId, bool bSucceeded);

    UFUNCTION(Server, Reliable)
    void ServerCheatKillAllSegments();

    UFUNCTION(Server, Reliable)
    void ServerCheatRespawnAtCheckpoint();

    UFUNCTION(Server, Reliable)
    void ServerCheatNextStage();

    UFUNCTION(Server, Reliable)
    void ServerCheatGoToStage(int32 OneBasedStageNumber);

    UFUNCTION(Server, Reliable)
    void ServerCheatGoToCheckpoint(int32 OneBasedCheckpointNumber);

    UFUNCTION(Server, Reliable)
    void ServerCheatKillSegment(int32 SegmentIndex);

    UFUNCTION(Server, Reliable)
    void ServerCheatDamageSegment(int32 SegmentIndex, float Damage);

    UFUNCTION(Server, Reliable)
    void ServerCheatDamagePart(int32 OneBasedSlotIndex, float Damage);

    UFUNCTION(Server, Reliable)
    void ServerCheatSetInvincible(bool bEnabled);

    UFUNCTION(Server, Reliable)
    void ServerCheatSpawnRandomParts();

    UFUNCTION(Server, Reliable)
    void ServerCheatAttachPart(int32 OneBasedSlotIndex, FName PartName);

    UFUNCTION(Server, Reliable)
    void ServerCheatFillAllSlotsWithPart(FName PartName);

    UFUNCTION(Server, Reliable)
    void ServerCheatClearRandomParts();

    UFUNCTION(Server, Reliable)
    void ServerCheatSpawnLegParts();

    UFUNCTION(Server, Reliable)
    void ServerCheatClearLegParts();

    UFUNCTION(Server, Unreliable)
    void ServerApplyCheatDebugMovement(float ForwardInput, float TurnInput);

    UFUNCTION(Server, Reliable)
    void ServerSetSoloControlKeyPressed(
        int32 KeyIndex,
        bool bPressed,
        bool bReverseMovement,
        bool bDetachPart);

    UFUNCTION(Server, Reliable)
    void ServerRequestTeleportToTestArea(FName AreaId);

    UPROPERTY(Transient)
    TObjectPtr<ACMChimera> CachedSharedChimera;

    UPROPERTY(Transient)
    TObjectPtr<UCMClientStageLoadComponent> ClientStageLoadComponent;

    bool bDetachModifierHeld = false;
    bool bReverseModifierHeld = false;
    bool bCheatDebugMovementEnabled = false;
    bool bDebugMoveForwardHeld = false;
    bool bDebugMoveBackwardHeld = false;
    bool bDebugTurnLeftHeld = false;
    bool bDebugTurnRightHeld = false;

    FCMPartSlotAddress SoloPressedPartSlots[CMControl::SoloTestKeyCount];
    double SoloControlKeyStartTimes[CMControl::SoloTestKeyCount] = {};
    bool bSoloControlKeyReverseMovement[CMControl::SoloTestKeyCount] = {};

};
