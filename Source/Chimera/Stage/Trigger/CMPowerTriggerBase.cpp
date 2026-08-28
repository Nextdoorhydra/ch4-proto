#include "Stage/Trigger/CMPowerTriggerBase.h"

#include "Stage/Trigger/Component/CMPowerSocketComponent.h"

ACMPowerTriggerBase::ACMPowerTriggerBase()
{
}

void ACMPowerTriggerBase::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        return;
    }

    RequiredSockets.RemoveAll(
        [](const TObjectPtr<UCMPowerSocketComponent>& Socket)
        {
            return !IsValid(Socket);
        });

    if (RequiredSockets.IsEmpty())
    {
        TArray<UCMPowerSocketComponent*> OwnedSockets;
        GetComponents(OwnedSockets);
        for (UCMPowerSocketComponent* Socket : OwnedSockets)
        {
            if (IsValid(Socket))
            {
                RequiredSockets.Add(Socket);
            }
        }
    }

    for (UCMPowerSocketComponent* Socket : RequiredSockets)
    {
        if (Socket)
        {
            Socket->OnConnectionChanged.AddUniqueDynamic(
                this,
                &ThisClass::HandleSocketConnectionChanged
            );
        }
    }

    EvaluatePowerState();
}

void ACMPowerTriggerBase::HandleSocketConnectionChanged(bool bConnected)
{
    EvaluatePowerState();
}

void ACMPowerTriggerBase::EvaluatePowerState()
{
    if (!HasAuthority())
    {
        return;
    }

    int32 ConnectedCount = 0;
    for (const UCMPowerSocketComponent* Socket : RequiredSockets)
    {
        if (Socket && Socket->IsConnected())
        {
            ++ConnectedCount;
        }
    }

    const int32 RequiredCount = RequiredSockets.Num();
    const bool bConditionMet = bRequireAllSockets
        ? RequiredCount > 0 && ConnectedCount == RequiredCount
        : ConnectedCount > 0;
    const bool bIsTriggered = ActivationTrigger
        && ActivationTrigger->IsTriggered();

    if (bConditionMet && !bIsTriggered)
    {
        ActivateTrigger(nullptr);
    }
    else if (!bConditionMet && bIsTriggered)
    {
        DeactivateTrigger(nullptr);
    }
}
