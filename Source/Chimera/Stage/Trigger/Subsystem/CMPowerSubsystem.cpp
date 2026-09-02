#include "Stage/Trigger/Subsystem/CMPowerSubsystem.h"

#include "Stage/Trigger/Component/CMPowerSocketComponent.h"
#include "Stage/Trigger/Component/CMPowerSourceComponent.h"

void UCMPowerSubsystem::RegisterSocket(UCMPowerSocketComponent* Socket)
{
    CleanupInvalidEntries();
    if (IsValid(Socket))
    {
        Sockets.AddUnique(Socket);
    }
}

void UCMPowerSubsystem::UnregisterSocket(UCMPowerSocketComponent* Socket)
{
    Sockets.Remove(Socket);
}

void UCMPowerSubsystem::RegisterSource(UCMPowerSourceComponent* Source)
{
    CleanupInvalidEntries();
    if (IsValid(Source))
    {
        Sources.AddUnique(Source);
    }
}

void UCMPowerSubsystem::UnregisterSource(UCMPowerSourceComponent* Source)
{
    Sources.Remove(Source);
}

void UCMPowerSubsystem::RefreshPowerState()
{
    CleanupInvalidEntries();
    for (const TWeakObjectPtr<UCMPowerSocketComponent>& SocketPtr : Sockets)
    {
        if (UCMPowerSocketComponent* Socket = SocketPtr.Get())
        {
            Socket->NotifyPowerStateChanged();
        }
    }
}

void UCMPowerSubsystem::CleanupInvalidEntries()
{
    Sockets.RemoveAll(
        [](const TWeakObjectPtr<UCMPowerSocketComponent>& Socket)
        {
            return !Socket.IsValid();
        });
    Sources.RemoveAll(
        [](const TWeakObjectPtr<UCMPowerSourceComponent>& Source)
        {
            return !Source.IsValid();
        });
}
