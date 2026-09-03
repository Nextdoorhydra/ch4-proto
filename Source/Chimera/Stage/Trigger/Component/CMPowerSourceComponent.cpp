#include "Stage/Trigger/Component/CMPowerSourceComponent.h"

#include "Net/UnrealNetwork.h"
#include "Stage/Trigger/CMPowerCableActor.h"
#include "Stage/Trigger/Subsystem/CMPowerSubsystem.h"

UCMPowerSourceComponent::UCMPowerSourceComponent()
{
    SetIsReplicatedByDefault(true);
}

void UCMPowerSourceComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UWorld* World = GetWorld())
    {
        World->GetSubsystem<UCMPowerSubsystem>()->RegisterSource(this);
    }
}

void UCMPowerSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetSubsystem<UCMPowerSubsystem>()->UnregisterSource(this);
    }
    Super::EndPlay(EndPlayReason);
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

    if (!Cable->CanConnectEndpointWithinLength(GetComponentLocation()))
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

void UCMPowerSourceComponent::SetPowerEnabled(bool bEnabled)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()
        || bPowerEnabled == bEnabled)
    {
        return;
    }

    bPowerEnabled = bEnabled;
    NotifyPowerStateChanged();
    if (UWorld* World = GetWorld())
    {
        World->GetSubsystem<UCMPowerSubsystem>()->RefreshPowerState();
    }
    GetOwner()->ForceNetUpdate();
}

void UCMPowerSourceComponent::NotifyPowerStateChanged()
{
    OnPowerStateChanged.Broadcast(bPowerEnabled);
    for (ACMPowerCableActor* Cable : ConnectedCables)
    {
        if (IsValid(Cable))
        {
            Cable->NotifyPowerStateChanged();
        }
    }
}

void UCMPowerSourceComponent::OnRep_ConnectedCables()
{
    OnConnectionChanged.Broadcast(!ConnectedCables.IsEmpty());
}

void UCMPowerSourceComponent::OnRep_PowerEnabled()
{
    NotifyPowerStateChanged();
    if (UWorld* World = GetWorld())
    {
        World->GetSubsystem<UCMPowerSubsystem>()->RefreshPowerState();
    }
}

void UCMPowerSourceComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCMPowerSourceComponent, ConnectedCables);
    DOREPLIFETIME(UCMPowerSourceComponent, bPowerEnabled);
}
