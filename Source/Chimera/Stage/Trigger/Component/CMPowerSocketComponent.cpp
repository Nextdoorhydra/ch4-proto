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
        || ConnectedCable || Cable->IsConnected())
    {
        return false;
    }

    if (PowerChannel.IsNone()
        || Cable->GetPowerChannel() != PowerChannel
        || FVector::DistSquared(
            GetComponentLocation(), Cable->GetCableEndLocation())
            > FMath::Square(ConnectionRadius))
    {
        return false;
    }

    ConnectedCable = Cable;
    Cable->SetConnectedSocket(this);
    OnConnectionChanged.Broadcast(true);
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
    GetOwner()->ForceNetUpdate();
}

void UCMPowerSocketComponent::OnRep_ConnectedCable()
{
    OnConnectionChanged.Broadcast(ConnectedCable != nullptr);
}

void UCMPowerSocketComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCMPowerSocketComponent, ConnectedCable);
}
