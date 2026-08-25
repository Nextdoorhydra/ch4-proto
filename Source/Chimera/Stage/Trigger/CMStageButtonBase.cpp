#include "Stage/Trigger/CMStageButtonBase.h"

#include "Stage/Trigger/Component/CMActivationTriggerComponent.h"

ACMStageButtonBase::ACMStageButtonBase()
{
}

// 런타임에 버튼 작동 이벤트를 대상 명령 처리 함수에 연결
void ACMStageButtonBase::BeginPlay()
{
    Super::BeginPlay();
    ActivationTrigger->OnActivated.AddDynamic(this, &ThisClass::HandleButtonActivated);
    ActivationTrigger->OnDeactivated.AddDynamic(this, &ThisClass::HandleButtonDeactivated);
}

// 버튼의 공통 트리거에 작동 요청 전달
bool ACMStageButtonBase::PressButton(AActor* PressingActor)
{
    return ActivateTrigger(PressingActor);
}

// 유지형 버튼의 해제 요청을 공통 트리거에 전달
bool ACMStageButtonBase::ReleaseButton(AActor* ReleasingActor)
{
    return DeactivateTrigger(ReleasingActor);
}

// 공통 트리거 활성 상태를 버튼 전용 표현 이벤트로 전달
void ACMStageButtonBase::HandleButtonActivated(AActor* TriggeringActor)
{
    OnButtonPressed(TriggeringActor);
}

// 공통 트리거 해제 상태를 버튼 전용 표현 이벤트로 전달
void ACMStageButtonBase::HandleButtonDeactivated(AActor* TriggeringActor)
{
    OnButtonReleased(TriggeringActor);
}
