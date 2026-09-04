#pragma once

#include "CoreMinimal.h"
#include "Stage/CMStageElementBase.h"
#include "Stage/Trigger/CMStageTriggerBase.h"

#include "CMStagePuzzleController.generated.h"

UENUM(BlueprintType)
enum class ECMPuzzleElementCommand : uint8
{
    Activate,
    Deactivate,
    Toggle,
    Reset
};

UENUM(BlueprintType)
enum class ECMPuzzleTriggerCondition : uint8
{
    Any, // 등록된 Trigger 중 하나의 유효 신호로 실행
    All, // 등록된 Trigger가 모두 조건을 충족하면 실행
    Sequence // Expected Trigger Sequence의 순서대로 입력되면 실행
};

UENUM(BlueprintType)
enum class ECMPuzzleSequenceWrongInputBehavior : uint8
{
    Ignore,
    ResetSequence,
    ResetAndExecuteFailureCommands
};

UENUM(BlueprintType)
enum class ECMPuzzleAllConditionMode : uint8
{
    Simultaneous, // 모든 Trigger의 현재 활성 상태가 목표 조건과 일치하면 충족
    Latched       // 각 Trigger가 한 번씩 신호를 보내면 충족
};

UENUM(BlueprintType)
enum class ECMPuzzleSimultaneousMatchState : uint8
{
    AllActive UMETA(DisplayName = "All Active"),
    AllInactive UMETA(DisplayName = "All Inactive"),
    AllEqual UMETA(DisplayName = "All Equal")
};

UENUM(BlueprintType)
enum class ECMPuzzleAcceptedSignal : uint8
{
    // 기존 에셋의 enum 이름/값을 보존하는 로드 호환용 항목. 새 설정에서는 숨긴다.
    PulseOrActivated = 0 UMETA(Hidden),
    ActivatedOnly = 1 UMETA(DisplayName = "ON Only"),
    DeactivatedOnly = 2 UMETA(DisplayName = "OFF Only"),
    Any = 3 UMETA(Hidden),
    StateChanged = 4 UMETA(DisplayName = "ON / OFF", ToolTip = "ON과 OFF 상태 전환을 모두 받습니다.")
};

UENUM(BlueprintType)
enum class ECMPuzzleStepEndBehavior : uint8
{
    Stop UMETA(ToolTip = "마지막 Step 이후 ON/OFF 모두 무시합니다. Reset Puzzle로 재사용합니다."),
    Loop UMETA(ToolTip = "마지막 Step 이후 이 채널의 첫 Step으로 복귀합니다. 다음 허용 신호를 기다립니다."),
    RepeatCurrent UMETA(ToolTip = "마지막 Step에 머물러 다음 허용 신호마다 마지막 Step을 실행합니다.")
};

USTRUCT(BlueprintType)
struct FCMPuzzleTargetCommand
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (ToolTip = "Toggle은 허용된 신호마다 대상 상태를 반전합니다. ON/OFF 양쪽 반전은 State Changed + Loop를 사용합니다."))
    ECMPuzzleElementCommand Command = ECMPuzzleElementCommand::Toggle;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly)
    TArray<TObjectPtr<ACMStageElementBase>> Targets;
};

USTRUCT(BlueprintType)
struct FCMPuzzleStep
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FCMPuzzleTargetCommand> Commands;
};

USTRUCT(BlueprintType)
struct FCMPuzzleChannel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ChannelId;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        meta = (EditCondition = "TriggerCondition != ECMPuzzleTriggerCondition::Sequence", EditConditionHides))
    TArray<TObjectPtr<ACMStageTriggerBase>> Triggers;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECMPuzzleTriggerCondition TriggerCondition = ECMPuzzleTriggerCondition::Any;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "TriggerCondition == ECMPuzzleTriggerCondition::All"))
    ECMPuzzleAllConditionMode AllConditionMode = ECMPuzzleAllConditionMode::Latched;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "TriggerCondition == ECMPuzzleTriggerCondition::All && AllConditionMode == ECMPuzzleAllConditionMode::Simultaneous"))
    ECMPuzzleSimultaneousMatchState SimultaneousMatchState =
        ECMPuzzleSimultaneousMatchState::AllActive;

    // Sequence 조건에서 사용할 정확한 입력 순서. 같은 Trigger를 여러 번 등록할 수 있다.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        meta = (EditCondition = "TriggerCondition == ECMPuzzleTriggerCondition::Sequence", EditConditionHides))
    TArray<TObjectPtr<ACMStageTriggerBase>> ExpectedTriggerSequence;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "TriggerCondition == ECMPuzzleTriggerCondition::Sequence", EditConditionHides))
    ECMPuzzleSequenceWrongInputBehavior WrongInputBehavior =
        ECMPuzzleSequenceWrongInputBehavior::ResetSequence;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "TriggerCondition == ECMPuzzleTriggerCondition::Sequence && WrongInputBehavior == ECMPuzzleSequenceWrongInputBehavior::ResetAndExecuteFailureCommands", EditConditionHides))
    TArray<FCMPuzzleTargetCommand> FailureCommands;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECMPuzzleAcceptedSignal AcceptedSignal = ECMPuzzleAcceptedSignal::StateChanged;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FCMPuzzleStep> Steps;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECMPuzzleStepEndBehavior EndBehavior = ECMPuzzleStepEndBehavior::Stop;
};

UCLASS(Blueprintable)
// 여러 Trigger 입력을 단계별 StageElement 명령으로 변환하는 퍼즐 단위 관리자
class CHIMERA_API ACMStagePuzzleController : public ACMStageElementBase
{
    GENERATED_BODY()

public:
    ACMStagePuzzleController();
    virtual void PostLoad() override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Stage|Puzzle")
    void ResetPuzzle();

    UFUNCTION(BlueprintPure, Category = "Chimera|Stage|Puzzle")
    int32 GetCurrentStepIndex(FName ChannelId) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Stage|Puzzle")
    int32 GetCurrentSequenceIndex(FName ChannelId) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Stage|Puzzle")
    int32 GetSequenceLength(FName ChannelId) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void HandleElementReset_Implementation() override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Stage|Puzzle")
    TArray<FCMPuzzleChannel> PuzzleChannels;

    // 퍼즐 리셋 시 모든 Step에 직접 등록된 대상도 시작 상태로 복원
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Stage|Puzzle")
    bool bResetTargetsWithPuzzle = true;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Stage|Puzzle")
    TArray<int32> CurrentStepIndices;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Stage|Puzzle")
    TArray<bool> CompletedChannels;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Stage|Puzzle")
    TArray<int32> CurrentSequenceIndices;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCMTriggerPresentationTest;
#endif
    UFUNCTION()
    void HandleTriggerSignal(
        ACMStageTriggerBase* Trigger,
        ECMStageTriggerSignal Signal);

    bool IsSignalAccepted(
        const FCMPuzzleChannel& Channel,
        ECMStageTriggerSignal Signal) const;
    void HandleChannelSignal(
        int32 ChannelIndex,
        ACMStageTriggerBase* Trigger,
        ECMStageTriggerSignal Signal);
    void HandleSequenceSignal(
        int32 ChannelIndex,
        ACMStageTriggerBase* Trigger,
        ECMStageTriggerSignal Signal);
    void ExecuteCurrentStep(int32 ChannelIndex);
    void ExecuteTargetCommand(const FCMPuzzleTargetCommand& TargetCommand);
    bool IsTriggerRegistered(
        const FCMPuzzleChannel& Channel,
        const ACMStageTriggerBase* Trigger) const;
    void ResetRuntimeState(bool bResetTargets);
    void NormalizeAcceptedSignals();
    void SetTriggerDirectCommandsEnabled(bool bEnabled);

    TArray<TArray<TWeakObjectPtr<ACMStageTriggerBase>>> SatisfiedTriggersByChannel;
    TArray<bool> AllConditionSatisfied;

    // 같은 상태 신호가 중복 전달되어도 Toggle/Sequence는 한 번만 실행한다.
    TMap<TWeakObjectPtr<ACMStageTriggerBase>, bool> LastTriggerStates;
};
