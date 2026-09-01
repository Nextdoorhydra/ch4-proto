#include "Stage/Puzzle/CMStagePuzzleController.h"

#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPuzzle, Log, All);

ACMStagePuzzleController::ACMStagePuzzleController()
{
}

void ACMStagePuzzleController::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, CurrentStepIndices);
    DOREPLIFETIME(ThisClass, CompletedChannels);
    DOREPLIFETIME(ThisClass, CurrentSequenceIndices);
}

// 서버에서 직접 참조한 Trigger를 구독하고 채널 런타임 상태 준비
void ACMStagePuzzleController::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority())
    {
        return;
    }

    ResetRuntimeState(false);
    SetTriggerDirectCommandsEnabled(false);
    for (const FCMPuzzleChannel& Channel : PuzzleChannels)
    {
        const TArray<TObjectPtr<ACMStageTriggerBase>>& ChannelTriggers =
            Channel.TriggerCondition == ECMPuzzleTriggerCondition::Sequence
                ? Channel.ExpectedTriggerSequence
                : Channel.Triggers;
        for (ACMStageTriggerBase* Trigger : ChannelTriggers)
        {
            if (IsValid(Trigger))
            {
                Trigger->OnTriggerSignal.AddUniqueDynamic(
                    this, &ThisClass::HandleTriggerSignal);
            }
        }
    }
}

// Controller 제거 시 Trigger의 기존 직접 대상 명령 경로 복원
void ACMStagePuzzleController::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        for (const FCMPuzzleChannel& Channel : PuzzleChannels)
        {
            const TArray<TObjectPtr<ACMStageTriggerBase>>& ChannelTriggers =
                Channel.TriggerCondition == ECMPuzzleTriggerCondition::Sequence
                    ? Channel.ExpectedTriggerSequence
                    : Channel.Triggers;
            for (ACMStageTriggerBase* Trigger : ChannelTriggers)
            {
                if (IsValid(Trigger))
                {
                    Trigger->OnTriggerSignal.RemoveDynamic(
                        this, &ThisClass::HandleTriggerSignal);
                }
            }
        }
        SetTriggerDirectCommandsEnabled(true);
    }
    Super::EndPlay(EndPlayReason);
}

void ACMStagePuzzleController::ResetPuzzle()
{
    if (HasAuthority())
    {
        ResetRuntimeState(bResetTargetsWithPuzzle);
    }
}

int32 ACMStagePuzzleController::GetCurrentStepIndex(FName ChannelId) const
{
    for (int32 ChannelIndex = 0;
        ChannelIndex < PuzzleChannels.Num();
        ++ChannelIndex)
    {
        if (PuzzleChannels[ChannelIndex].ChannelId == ChannelId)
        {
            return CurrentStepIndices.IsValidIndex(ChannelIndex)
                ? CurrentStepIndices[ChannelIndex]
                : INDEX_NONE;
        }
    }
    return INDEX_NONE;
}

int32 ACMStagePuzzleController::GetCurrentSequenceIndex(FName ChannelId) const
{
    for (int32 ChannelIndex = 0;
        ChannelIndex < PuzzleChannels.Num();
        ++ChannelIndex)
    {
        if (PuzzleChannels[ChannelIndex].ChannelId == ChannelId)
        {
            return CurrentSequenceIndices.IsValidIndex(ChannelIndex)
                ? CurrentSequenceIndices[ChannelIndex]
                : INDEX_NONE;
        }
    }
    return INDEX_NONE;
}

int32 ACMStagePuzzleController::GetSequenceLength(FName ChannelId) const
{
    for (const FCMPuzzleChannel& Channel : PuzzleChannels)
    {
        if (Channel.ChannelId == ChannelId)
        {
            return Channel.TriggerCondition == ECMPuzzleTriggerCondition::Sequence
                ? Channel.ExpectedTriggerSequence.Num()
                : 0;
        }
    }
    return INDEX_NONE;
}

void ACMStagePuzzleController::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    if (HasAuthority())
    {
        ResetRuntimeState(bResetTargetsWithPuzzle);
    }
}

// 하나의 Trigger가 여러 채널에 등록된 경우 각 채널 규칙을 독립 평가
void ACMStagePuzzleController::HandleTriggerSignal(
    ACMStageTriggerBase* Trigger,
    ECMStageTriggerSignal Signal)
{
    if (!HasAuthority() || !IsElementActive() || !IsValid(Trigger))
    {
        return;
    }

    for (int32 ChannelIndex = 0;
        ChannelIndex < PuzzleChannels.Num();
        ++ChannelIndex)
    {
        if (IsTriggerRegistered(PuzzleChannels[ChannelIndex], Trigger))
        {
            HandleChannelSignal(ChannelIndex, Trigger, Signal);
        }
    }
}

bool ACMStagePuzzleController::IsSignalAccepted(
    const FCMPuzzleChannel& Channel,
    ECMStageTriggerSignal Signal) const
{
    switch (Channel.AcceptedSignal)
    {
    case ECMPuzzleAcceptedSignal::PulseOrActivated:
        return Signal == ECMStageTriggerSignal::Pulse
            || Signal == ECMStageTriggerSignal::Activated;
    case ECMPuzzleAcceptedSignal::ActivatedOnly:
        return Signal == ECMStageTriggerSignal::Activated;
    case ECMPuzzleAcceptedSignal::DeactivatedOnly:
        return Signal == ECMStageTriggerSignal::Deactivated;
    case ECMPuzzleAcceptedSignal::Any:
        return true;
    default:
        return false;
    }
}

// Any는 즉시 실행하고 All은 동시 또는 누적 충족 상태를 계산
void ACMStagePuzzleController::HandleChannelSignal(
    int32 ChannelIndex,
    ACMStageTriggerBase* Trigger,
    ECMStageTriggerSignal Signal)
{
    if (!PuzzleChannels.IsValidIndex(ChannelIndex)
        || !CompletedChannels.IsValidIndex(ChannelIndex)
        || CompletedChannels[ChannelIndex])
    {
        return;
    }

    const FCMPuzzleChannel& Channel = PuzzleChannels[ChannelIndex];
    if (Channel.TriggerCondition == ECMPuzzleTriggerCondition::Sequence)
    {
        HandleSequenceSignal(ChannelIndex, Trigger, Signal);
        return;
    }

    if (Channel.TriggerCondition == ECMPuzzleTriggerCondition::Any)
    {
        if (IsSignalAccepted(Channel, Signal))
        {
            ExecuteCurrentStep(ChannelIndex);
        }
        return;
    }

    if (!SatisfiedTriggersByChannel.IsValidIndex(ChannelIndex)
        || !AllConditionSatisfied.IsValidIndex(ChannelIndex))
    {
        return;
    }

    TArray<TWeakObjectPtr<ACMStageTriggerBase>>& SatisfiedTriggers =
        SatisfiedTriggersByChannel[ChannelIndex];
    if (Channel.AllConditionMode == ECMPuzzleAllConditionMode::Simultaneous)
    {
        bool bTriggerStateChanged = false;
        if (Signal == ECMStageTriggerSignal::Activated)
        {
            if (!SatisfiedTriggers.Contains(Trigger))
            {
                SatisfiedTriggers.Add(Trigger);
                bTriggerStateChanged = true;
            }
        }
        else if (Signal == ECMStageTriggerSignal::Deactivated)
        {
            bTriggerStateChanged = SatisfiedTriggers.Remove(Trigger) > 0;
        }
        else
        {
            // Simultaneous는 상태가 없는 Pulse로 현재 ON/OFF 조건을 판정하지 않음
            return;
        }

        if (bTriggerStateChanged)
        {
            AllConditionSatisfied[ChannelIndex] = false;
        }
    }
    else if (IsSignalAccepted(Channel, Signal))
    {
        SatisfiedTriggers.AddUnique(Trigger);
    }

    int32 ValidTriggerCount = 0;
    for (ACMStageTriggerBase* RequiredTrigger : Channel.Triggers)
    {
        if (IsValid(RequiredTrigger)
            && SatisfiedTriggers.Contains(RequiredTrigger))
        {
            ++ValidTriggerCount;
        }
    }

    int32 RequiredCount = 0;
    for (ACMStageTriggerBase* RequiredTrigger : Channel.Triggers)
    {
        if (IsValid(RequiredTrigger))
        {
            ++RequiredCount;
        }
    }
    bool bAllSatisfied = RequiredCount > 0
        && ValidTriggerCount == RequiredCount;
    if (Channel.AllConditionMode == ECMPuzzleAllConditionMode::Simultaneous)
    {
        const bool bAllActive = RequiredCount > 0
            && ValidTriggerCount == RequiredCount;
        const bool bAllInactive = RequiredCount > 0
            && ValidTriggerCount == 0;
        switch (Channel.SimultaneousMatchState)
        {
        case ECMPuzzleSimultaneousMatchState::AllActive:
            bAllSatisfied = bAllActive;
            break;
        case ECMPuzzleSimultaneousMatchState::AllInactive:
            bAllSatisfied = bAllInactive;
            break;
        case ECMPuzzleSimultaneousMatchState::AllEqual:
            bAllSatisfied = bAllActive || bAllInactive;
            break;
        }
    }

    if (!bAllSatisfied
        || AllConditionSatisfied[ChannelIndex]
        || !IsSignalAccepted(Channel, Signal))
    {
        return;
    }

    AllConditionSatisfied[ChannelIndex] = true;
    ExecuteCurrentStep(ChannelIndex);
    if (Channel.AllConditionMode == ECMPuzzleAllConditionMode::Latched)
    {
        SatisfiedTriggers.Reset();
        AllConditionSatisfied[ChannelIndex] = false;
    }
}

// 유효 신호가 정확한 Trigger 순서를 따르는지 검사하고 완성 시 현재 Step 실행
void ACMStagePuzzleController::HandleSequenceSignal(
    int32 ChannelIndex,
    ACMStageTriggerBase* Trigger,
    ECMStageTriggerSignal Signal)
{
    if (!PuzzleChannels.IsValidIndex(ChannelIndex)
        || !CurrentSequenceIndices.IsValidIndex(ChannelIndex))
    {
        return;
    }

    const FCMPuzzleChannel& Channel = PuzzleChannels[ChannelIndex];
    if (!IsSignalAccepted(Channel, Signal)
        || Channel.ExpectedTriggerSequence.IsEmpty())
    {
        return;
    }

    int32& SequenceIndex = CurrentSequenceIndices[ChannelIndex];
    if (!Channel.ExpectedTriggerSequence.IsValidIndex(SequenceIndex))
    {
        SequenceIndex = 0;
    }

    ACMStageTriggerBase* ExpectedTrigger =
        Channel.ExpectedTriggerSequence[SequenceIndex];
    if (!IsValid(ExpectedTrigger))
    {
        UE_LOG(LogChimeraPuzzle, Error,
            TEXT("[Puzzle Sequence Invalid] Controller=%s Channel=%s Index=%d has no Trigger"),
            *GetName(), *Channel.ChannelId.ToString(), SequenceIndex);
        return;
    }

    if (Trigger == ExpectedTrigger)
    {
        ++SequenceIndex;
        if (SequenceIndex == Channel.ExpectedTriggerSequence.Num())
        {
            SequenceIndex = 0;
            ExecuteCurrentStep(ChannelIndex);
        }
        else
        {
            ForceNetUpdate();
        }
        return;
    }

    switch (Channel.WrongInputBehavior)
    {
    case ECMPuzzleSequenceWrongInputBehavior::Ignore:
        return;
    case ECMPuzzleSequenceWrongInputBehavior::ResetSequence:
        SequenceIndex = 0;
        break;
    case ECMPuzzleSequenceWrongInputBehavior::ResetAndExecuteFailureCommands:
        SequenceIndex = 0;
        for (const FCMPuzzleTargetCommand& Command : Channel.FailureCommands)
        {
            ExecuteTargetCommand(Command);
        }
        break;
    }

    ForceNetUpdate();
}

bool ACMStagePuzzleController::IsTriggerRegistered(
    const FCMPuzzleChannel& Channel,
    const ACMStageTriggerBase* Trigger) const
{
    return Channel.TriggerCondition == ECMPuzzleTriggerCondition::Sequence
        ? Channel.ExpectedTriggerSequence.Contains(Trigger)
        : Channel.Triggers.Contains(Trigger);
}

// 현재 Step의 모든 명령을 실행하고 채널 종료 정책에 따라 다음 위치 결정
void ACMStagePuzzleController::ExecuteCurrentStep(int32 ChannelIndex)
{
    if (!PuzzleChannels.IsValidIndex(ChannelIndex)
        || !CurrentStepIndices.IsValidIndex(ChannelIndex)
        || !CompletedChannels.IsValidIndex(ChannelIndex))
    {
        return;
    }

    const FCMPuzzleChannel& Channel = PuzzleChannels[ChannelIndex];
    const int32 StepIndex = CurrentStepIndices[ChannelIndex];
    if (!Channel.Steps.IsValidIndex(StepIndex))
    {
        return;
    }

    for (const FCMPuzzleTargetCommand& TargetCommand
        : Channel.Steps[StepIndex].Commands)
    {
        ExecuteTargetCommand(TargetCommand);
    }

    const bool bLastStep = StepIndex == Channel.Steps.Num() - 1;
    if (!bLastStep)
    {
        ++CurrentStepIndices[ChannelIndex];
    }
    else
    {
        switch (Channel.EndBehavior)
        {
        case ECMPuzzleStepEndBehavior::Stop:
            CompletedChannels[ChannelIndex] = true;
            break;
        case ECMPuzzleStepEndBehavior::Loop:
            CurrentStepIndices[ChannelIndex] = 0;
            break;
        case ECMPuzzleStepEndBehavior::RepeatCurrent:
            break;
        }
    }

    UE_LOG(LogChimeraPuzzle, Log,
        TEXT("[Puzzle Step Executed] Controller=%s Channel=%s Step=%d Next=%d Completed=%s"),
        *GetName(),
        *Channel.ChannelId.ToString(),
        StepIndex,
        CurrentStepIndices[ChannelIndex],
        CompletedChannels[ChannelIndex] ? TEXT("true") : TEXT("false"));
    ForceNetUpdate();
}

// 직접 참조한 StageElement에 공통 활성 상태 명령 실행
void ACMStagePuzzleController::ExecuteTargetCommand(
    const FCMPuzzleTargetCommand& TargetCommand)
{
    for (ACMStageElementBase* Target : TargetCommand.Targets)
    {
        if (!IsValid(Target) || Target == this)
        {
            continue;
        }

        switch (TargetCommand.Command)
        {
        case ECMPuzzleElementCommand::Activate:
            Target->ActivateElement();
            break;
        case ECMPuzzleElementCommand::Deactivate:
            Target->DeactivateElement();
            break;
        case ECMPuzzleElementCommand::Toggle:
            Target->ToggleElement();
            break;
        case ECMPuzzleElementCommand::Reset:
            Target->ResetElement();
            break;
        }
    }
}

// 채널 진행도와 All 조건 기록을 초기화하고 선택적으로 모든 대상도 리셋
void ACMStagePuzzleController::ResetRuntimeState(bool bResetTargets)
{
    CurrentStepIndices.Init(0, PuzzleChannels.Num());
    CompletedChannels.Init(false, PuzzleChannels.Num());
    CurrentSequenceIndices.Init(0, PuzzleChannels.Num());
    SatisfiedTriggersByChannel.SetNum(PuzzleChannels.Num());
    AllConditionSatisfied.Init(false, PuzzleChannels.Num());
    for (TArray<TWeakObjectPtr<ACMStageTriggerBase>>& SatisfiedTriggers
        : SatisfiedTriggersByChannel)
    {
        SatisfiedTriggers.Reset();
    }

    if (bResetTargets)
    {
        TSet<ACMStageElementBase*> ResetTargets;
        for (const FCMPuzzleChannel& Channel : PuzzleChannels)
        {
            for (const FCMPuzzleTargetCommand& Command : Channel.FailureCommands)
            {
                for (ACMStageElementBase* Target : Command.Targets)
                {
                    if (IsValid(Target) && Target != this)
                    {
                        ResetTargets.Add(Target);
                    }
                }
            }
            for (const FCMPuzzleStep& Step : Channel.Steps)
            {
                for (const FCMPuzzleTargetCommand& Command : Step.Commands)
                {
                    for (ACMStageElementBase* Target : Command.Targets)
                    {
                        if (IsValid(Target) && Target != this)
                        {
                            ResetTargets.Add(Target);
                        }
                    }
                }
            }
        }
        for (ACMStageElementBase* Target : ResetTargets)
        {
            Target->ResetElement();
        }
    }
    ForceNetUpdate();
}

// Controller 소유 Trigger가 기존 TargetActor 명령까지 중복 실행하지 않도록 설정
void ACMStagePuzzleController::SetTriggerDirectCommandsEnabled(bool bEnabled)
{
    TSet<ACMStageTriggerBase*> UpdatedTriggers;
    for (const FCMPuzzleChannel& Channel : PuzzleChannels)
    {
        const TArray<TObjectPtr<ACMStageTriggerBase>>& ChannelTriggers =
            Channel.TriggerCondition == ECMPuzzleTriggerCondition::Sequence
                ? Channel.ExpectedTriggerSequence
                : Channel.Triggers;
        for (ACMStageTriggerBase* Trigger : ChannelTriggers)
        {
            if (IsValid(Trigger) && !UpdatedTriggers.Contains(Trigger))
            {
                Trigger->SetDirectTargetCommandEnabled(bEnabled);
                UpdatedTriggers.Add(Trigger);
            }
        }
    }
}
