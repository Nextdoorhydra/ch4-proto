#include "Stage/Trigger/Component/CMPowerSocketComponent.h"

#include "Net/UnrealNetwork.h"
#include "Stage/Trigger/CMPowerCableActor.h"

UCMPowerSocketComponent::UCMPowerSocketComponent()
{
    SetIsReplicatedByDefault(true);
}

bool UCMPowerSocketComponent::TryConnectCable(
    ACMPowerCableActor* Cable,
    bool bIgnoreConnectionRadius
)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Cable
        || ConnectedCables.Num() >= FMath::Max(MaxConnectedCables, 1)
        || Cable->HasConnectedSocket())
    {
        return false;
    }

    if (PowerChannel.IsNone()
        || Cable->GetPowerChannel() != PowerChannel
        || (!bIgnoreConnectionRadius && FVector::DistSquared(
            GetComponentLocation(),
            Cable->GetClosestFreeEndpointLocation(GetComponentLocation()))
            > FMath::Square(ConnectionRadius)))
    {
        return false;
    }

    ConnectedCables.AddUnique(Cable);
    Cable->SetConnectedSocket(this);
    OnConnectionChanged.Broadcast(true);
    NotifyPowerStateChanged();
    GetOwner()->ForceNetUpdate();
    return true;
}

void UCMPowerSocketComponent::DisconnectCable(ACMPowerCableActor* Cable)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()
        || !bAllowCableDisconnect || !ConnectedCables.Contains(Cable))
    {
        return;
    }

    ConnectedCables.Remove(Cable);
    Cable->SetConnectedSocket(nullptr);
    OnConnectionChanged.Broadcast(false);
    NotifyPowerStateChanged();
    GetOwner()->ForceNetUpdate();
}

void UCMPowerSocketComponent::OnRep_ConnectedCables()
{
    OnConnectionChanged.Broadcast(!ConnectedCables.IsEmpty());
    NotifyPowerStateChanged();
}

bool UCMPowerSocketComponent::IsPowered() const
{
    for (const ACMPowerCableActor* Cable : ConnectedCables)
    {
        if (Cable && Cable->IsPowerActive())
        {
            return true;
        }
    }
    return false;
}

void UCMPowerSocketComponent::NotifyPowerStateChanged()
{
    OnPowerStateChanged.Broadcast(IsPowered());
    for (ACMPowerCableActor* Cable : PowerOutputCables)
    {
        if (IsValid(Cable))
        {
            Cable->NotifyPowerStateChanged();
        }
    }
}

void UCMPowerSocketComponent::AddPowerOutputCable(
    ACMPowerCableActor* Cable
)
{
    if (IsValid(Cable))
    {
        PowerOutputCables.AddUnique(Cable);
    }
}

void UCMPowerSocketComponent::RemovePowerOutputCable(
    ACMPowerCableActor* Cable
)
{
    PowerOutputCables.Remove(Cable);
}

void UCMPowerSocketComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCMPowerSocketComponent, ConnectedCables);
}
