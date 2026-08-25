#include "Stage/Device/CMStageDoorBase.h"

#include "Stage/Trigger/CMStageTriggerBase.h"

ACMStageDoorBase::ACMStageDoorBase()
{
    bStartActive = false;
}

void ACMStageDoorBase::BeginPlay()
{
    Super::BeginPlay();

    if (!bUseActivationConditions)
    {
        return;
    }

    for (ACMStageTriggerBase* Condition : ActivationConditions)
    {
        if (Condition)
        {
            Condition->OnTriggerConditionChanged.AddUniqueDynamic(
                this, &ThisClass::HandleActivationConditionChanged);
        }
    }

    if (HasAuthority())
    {
        RefreshActivationConditionState();
    }
}

// Mechanism 활성 상태를 문의 열림 상태로 전달
void ACMStageDoorBase::HandleElementActiveChanged_Implementation(bool bIsActive)
{
    Super::HandleElementActiveChanged_Implementation(bIsActive);
    OnDoorOpenStateChanged(bIsActive);
}

// 문 전용 초기화 표현을 하위 구현에 전달
void ACMStageDoorBase::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    OnDoorReset();
}

void ACMStageDoorBase::HandleActivationConditionChanged(bool bIsTriggered)
{
    if (HasAuthority())
    {
        RefreshActivationConditionState();
    }
}

void ACMStageDoorBase::RefreshActivationConditionState()
{
    if (!bUseActivationConditions || !HasAuthority())
    {
        return;
    }

    const bool bShouldBeActive = AreActivationConditionsSatisfied();
    if (bShouldBeActive != IsElementActive())
    {
        if (bShouldBeActive)
        {
            ActivateElement();
        }
        else
        {
            DeactivateElement();
        }
    }
}

bool ACMStageDoorBase::AreActivationConditionsSatisfied() const
{
    if (ActivationConditions.IsEmpty())
    {
        return false;
    }

    int32 ActiveCount = 0;
    for (const ACMStageTriggerBase* Condition : ActivationConditions)
    {
        ActiveCount += Condition && Condition->IsTriggerConditionActive()
            ? 1
            : 0;
    }

    switch (ConditionOperation)
    {
    case ECMStageConditionOperation::Any:
        return ActiveCount > 0;
    case ECMStageConditionOperation::AtLeast:
        return ActiveCount >= FMath::Max(RequiredActiveConditionCount, 1);
    case ECMStageConditionOperation::Exactly:
        return ActiveCount == FMath::Max(RequiredActiveConditionCount, 1);
    case ECMStageConditionOperation::All:
    default:
        return ActiveCount == ActivationConditions.Num();
    }
}
