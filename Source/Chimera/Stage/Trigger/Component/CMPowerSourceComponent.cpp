#include "Stage/Trigger/Component/CMPowerSourceComponent.h"

#include "Net/UnrealNetwork.h"
#include "Stage/Trigger/CMPowerCableActor.h"

UCMPowerSourceComponent::UCMPowerSourceComponent()
{
    SetIsReplicatedByDefault(true);
}

bool UCMPowerSourceComponent::TryConnectCable(
    ACMPowerCableActor* Cable,
    bool bIgnoreConnectionRadius
)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Cable
        || ConnectedCables.Num() >= FMath::Max(MaxConnectedCables, 1)
        || Cable->HasConnectedSource())
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
    Cable->SetConnectedSource(this);
    OnConnectionChanged.Broadcast(true);
    GetOwner()->ForceNetUpdate();
    return true;
}

void UCMPowerSourceComponent::DisconnectCable(ACMPowerCableActor* Cable)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()
        || !bAllowCableDisconnect || !ConnectedCables.Contains(Cable))
    {
        return;
    }

    ConnectedCables.Remove(Cable);
    Cable->SetConnectedSource(nullptr);
    OnConnectionChanged.Broadcast(false);
    GetOwner()->ForceNetUpdate();
}

void UCMPowerSourceComponent::OnRep_ConnectedCables()
{
    OnConnectionChanged.Broadcast(!ConnectedCables.IsEmpty());
}

void UCMPowerSourceComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCMPowerSourceComponent, ConnectedCables);
}
