#include "Stage/Trigger/Component/CMPowerSocketComponent.h"

#include "Net/UnrealNetwork.h"
#include "Stage/Trigger/CMPowerCableActor.h"

UCMPowerSocketComponent::UCMPowerSocketComponent()
{
    SetIsReplicatedByDefault(true);
}

bool UCMPowerSocketComponent::TryConnectCable(ACMPowerCableActor* Cable)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Cable
        || ConnectedCable || Cable->HasConnectedSocket())
    {
        return false;
    }

    if (PowerChannel.IsNone()
        || Cable->GetPowerChannel() != PowerChannel
        || FVector::DistSquared(
            GetComponentLocation(),
            Cable->GetClosestFreeEndpointLocation(GetComponentLocation()))
            > FMath::Square(ConnectionRadius))
    {
        return false;
    }

    ConnectedCable = Cable;
    Cable->SetConnectedSocket(this);
    OnConnectionChanged.Broadcast(true);
    NotifyPowerStateChanged();
    GetOwner()->ForceNetUpdate();
    return true;
}

void UCMPowerSocketComponent::DisconnectCable(ACMPowerCableActor* Cable)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()
        || ConnectedCable != Cable)
    {
        return;
    }

    ConnectedCable = nullptr;
    Cable->SetConnectedSocket(nullptr);
    OnConnectionChanged.Broadcast(false);
    NotifyPowerStateChanged();
    GetOwner()->ForceNetUpdate();
}

void UCMPowerSocketComponent::OnRep_ConnectedCable()
{
    OnConnectionChanged.Broadcast(ConnectedCable != nullptr);
    NotifyPowerStateChanged();
}

bool UCMPowerSocketComponent::IsPowered() const
{
    return ConnectedCable && ConnectedCable->IsPowerActive();
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
    DOREPLIFETIME(UCMPowerSocketComponent, ConnectedCable);
}
