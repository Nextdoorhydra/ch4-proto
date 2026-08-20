#include "Stage/Mechanism/Component/CMActivationTriggerComponent.h"

#include "Net/UnrealNetwork.h"

UCMActivationTriggerComponent::UCMActivationTriggerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

// 트리거 활성 상태와 일회성 작동 이력을 네트워크로 복제
void UCMActivationTriggerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, bTriggerEnabled);
    DOREPLIFETIME(ThisClass, bHasTriggered);
}

// 현재 조건을 만족하면 한 번 작동시키고 서버 이벤트 전달
bool UCMActivationTriggerComponent::TryActivate(AActor* TriggeringActor)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !CanActivate())
    {
        return false;
    }

    bHasTriggered = true;
    OnActivated.Broadcast(TriggeringActor);
    OnTriggerStateChanged(bTriggerEnabled, bHasTriggered);
    GetOwner()->ForceNetUpdate();
    return true;
}

// 일회성 작동 이력을 지워 트리거 재사용 허용
void UCMActivationTriggerComponent::ResetTrigger()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bHasTriggered = false;
        OnTriggerStateChanged(bTriggerEnabled, bHasTriggered);
        GetOwner()->ForceNetUpdate();
    }
}

// 장치 상태에 맞춰 새로운 작동 요청 허용 여부 변경
void UCMActivationTriggerComponent::SetTriggerEnabled(bool bEnabled)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bTriggerEnabled = bEnabled;
        OnTriggerStateChanged(bTriggerEnabled, bHasTriggered);
        GetOwner()->ForceNetUpdate();
    }
}

// 복제된 트리거 상태를 클라이언트 표현에 반영
void UCMActivationTriggerComponent::OnRep_TriggerState()
{
    OnTriggerStateChanged(bTriggerEnabled, bHasTriggered);
}
