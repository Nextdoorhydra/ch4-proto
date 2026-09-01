#include "Stage/Trigger/Component/CMPowerSourceComponent.h"

#include "Net/UnrealNetwork.h"
#include "Stage/Trigger/CMPowerCableActor.h"

UCMPowerSourceComponent::UCMPowerSourceComponent()
{
    SetIsReplicatedByDefault(true);
}

bool UCMPowerSourceComponent::TryConnectCable(ACMPowerCableActor* Cable)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Cable
        || ConnectedCable || Cable->HasConnectedSource())
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
    Cable->SetConnectedSource(this);
    OnConnectionChanged.Broadcast(true);
    GetOwner()->ForceNetUpdate();
    return true;
}

void UCMPowerSourceComponent::DisconnectCable(ACMPowerCableActor* Cable)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()
        || ConnectedCable != Cable)
    {
        return;
    }

    ConnectedCable = nullptr;
    Cable->SetConnectedSource(nullptr);
    OnConnectionChanged.Broadcast(false);
    GetOwner()->ForceNetUpdate();
}

void UCMPowerSourceComponent::OnRep_ConnectedCable()
{
    OnConnectionChanged.Broadcast(ConnectedCable != nullptr);
}

void UCMPowerSourceComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCMPowerSourceComponent, ConnectedCable);
}
