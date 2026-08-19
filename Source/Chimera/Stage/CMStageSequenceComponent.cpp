#include "Stage/CMStageSequenceComponent.h"

#include "Stage/CMStageDirector.h"
#include "Stage/CMStageEventMessage.h"
#include "Stage/CMStageEventTags.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "TimerManager.h"

UCMStageSequenceComponent::UCMStageSequenceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCMStageSequenceComponent::BeginPlay()
{
    Super::BeginPlay();
    ACMStageDirector* Director = Cast<ACMStageDirector>(GetOwner());
    if (!Director || !Director->HasAuthority() || !SequenceTable)
    {
        return;
    }

    if (SequenceTable->GetRowStruct() != FCMStageSequenceRow::StaticStruct())
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Stage Sequence DataTable has the wrong RowStruct. Table=%s"),
            *GetNameSafe(SequenceTable));
        return;
    }

    TSet<FString> UsedOrders;
    for (const FName RowName : SequenceTable->GetRowNames())
    {
        const FCMStageSequenceRow* Row = SequenceTable->FindRow<FCMStageSequenceRow>(
            RowName, TEXT("StageSequenceValidation"));
        if (!Row)
        {
            continue;
        }

        const bool bHasPlacementTarget = !Row->TargetPlacementId.IsNone();
        const bool bHasGroupTarget = Row->TargetGroup.IsValid();
        if (!Row->StartEvent.IsValid() || !Row->CommandTag.IsValid()
            || bHasPlacementTarget == bHasGroupTarget || Row->DelaySeconds < 0.0f)
        {
            UE_LOG(LogChimeraStageLoad, Error,
                TEXT("Invalid Stage Sequence row skipped. Table=%s Row=%s StartEvent=%s Placement=%s Group=%s Command=%s Delay=%.2f"),
                *GetNameSafe(SequenceTable), *RowName.ToString(), *Row->StartEvent.ToString(),
                *Row->TargetPlacementId.ToString(), *Row->TargetGroup.ToString(),
                *Row->CommandTag.ToString(), Row->DelaySeconds);
            continue;
        }

        const FString OrderKey = FString::Printf(TEXT("%d:%d"), Row->StepOrder, Row->ActionOrder);
        if (UsedOrders.Contains(OrderKey))
        {
            UE_LOG(LogChimeraStageLoad, Warning,
                TEXT("Stage Sequence rows share StepOrder and ActionOrder; stable ID order will be used. Table=%s Row=%s Order=%s"),
                *GetNameSafe(SequenceTable), *RowName.ToString(), *OrderKey);
        }
        UsedOrders.Add(OrderKey);
        OrderedRows.Add(*Row);
    }
    OrderedRows.Sort([](const FCMStageSequenceRow& A, const FCMStageSequenceRow& B)
    {
        if (A.StepOrder != B.StepOrder)
        {
            return A.StepOrder < B.StepOrder;
        }
        if (A.ActionOrder != B.ActionOrder)
        {
            return A.ActionOrder < B.ActionOrder;
        }
        if (A.StepId != B.StepId)
        {
            return A.StepId.LexicalLess(B.StepId);
        }
        return A.TargetPlacementId.LexicalLess(B.TargetPlacementId);
    });

    EventListenerHandle = UGameplayMessageSubsystem::Get(this).RegisterListener<FCMStageEventMessage>(
        CMStageEventTags::Message_Stage_Event,
        this,
        &ThisClass::HandleStageEvent);
}

void UCMStageSequenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (EventListenerHandle.IsValid())
    {
        EventListenerHandle.Unregister();
    }
    Super::EndPlay(EndPlayReason);
}

void UCMStageSequenceComponent::HandleStageEvent(
    FGameplayTag Channel,
    const FCMStageEventMessage& Message)
{
    const ACMStageDirector* Director = Cast<ACMStageDirector>(GetOwner());
    if (!Director || Message.StageInstanceId != Director->GetStageInstanceId())
    {
        return;
    }

    for (const FCMStageSequenceRow& Row : OrderedRows)
    {
        if (!Row.StartEvent.MatchesTagExact(Message.EventTag))
        {
            continue;
        }
        if (Row.DelaySeconds <= 0.0f)
        {
            ExecuteRow(Row, Message.Instigator);
            continue;
        }

        TWeakObjectPtr<UCMStageSequenceComponent> WeakThis(this);
        TWeakObjectPtr<UObject> WeakInstigator(Message.Instigator);
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle,
            [WeakThis, Row, WeakInstigator]()
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->ExecuteRow(Row, WeakInstigator.Get());
                }
            },
            Row.DelaySeconds,
            false);
    }
}

void UCMStageSequenceComponent::ExecuteRow(
    FCMStageSequenceRow Row,
    UObject* EventInstigator)
{
    if (ACMStageDirector* Director = Cast<ACMStageDirector>(GetOwner()))
    {
        if (!Row.PreloadGroupId.IsNone())
        {
            Director->ExecuteStageCommandAfterLoad(
                Row.PreloadGroupId,
                Row.TargetPlacementId,
                Row.TargetGroup,
                Row.CommandTag,
                EventInstigator);
            return;
        }
        Director->ExecuteStageCommand(
            Row.TargetPlacementId,
            Row.TargetGroup,
            Row.CommandTag,
            EventInstigator);
    }
}
