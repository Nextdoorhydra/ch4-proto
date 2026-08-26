#include "Stage/Trigger/CMStageTriggerBase.h"

#include "Stage/CMStageCommandTags.h"
#include "Stage/CMStageElementComponent.h"
#include "Stage/Trigger/Component/CMActivationTriggerComponent.h"

ACMStageTriggerBase::ACMStageTriggerBase()
{
    ActivationTrigger = CreateDefaultSubobject<UCMActivationTriggerComponent>(TEXT("ActivationTrigger"));
    TargetCommandTag = CMStageCommandTags::Mechanism_Activate;
    ReleaseCommandTag = CMStageCommandTags::Mechanism_Deactivate;
}

// 런타임에 공통 트리거 상태를 StageDirector 대상 명령에 연결
void ACMStageTriggerBase::BeginPlay()
{
    Super::BeginPlay();
    ActivationTrigger->OnActivated.AddDynamic(this, &ThisClass::HandleTriggerActivated);
    ActivationTrigger->OnDeactivated.AddDynamic(this, &ThisClass::HandleTriggerDeactivated);
}

// 하위 구현에서 검증한 작동 조건을 공통 트리거에 전달
bool ACMStageTriggerBase::ActivateTrigger(AActor* TriggeringActor)
{
    return HasAuthority() && ActivationTrigger->TryActivate(TriggeringActor);
}

// 하위 구현에서 검증한 해제 조건을 공통 트리거에 전달
bool ACMStageTriggerBase::DeactivateTrigger(AActor* TriggeringActor)
{
    return HasAuthority()
        && ActivationTrigger->SetTriggeredState(false, TriggeringActor);
}

void ACMStageTriggerBase::SetDirectTargetCommandEnabled(bool bEnabled)
{
    if (HasAuthority())
    {
        bDirectTargetCommandEnabled = bEnabled;
    }
}

// 장치 활성 상태를 실제 트리거 입력 허용 상태에 연결
void ACMStageTriggerBase::HandleElementActiveChanged_Implementation(bool bIsActive)
{
    Super::HandleElementActiveChanged_Implementation(bIsActive);
    if (HasAuthority())
    {
        ActivationTrigger->SetTriggerEnabled(bIsActive);
    }
}

// 트리거 작동 이력을 레벨 시작 상태로 복원
void ACMStageTriggerBase::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    ActivationTrigger->ResetTrigger();
}

// 활성 조건이 충족되면 설정된 대상 명령과 표현 이벤트를 실행
void ACMStageTriggerBase::HandleTriggerActivated(AActor* TriggeringActor)
{
    if (bDirectTargetCommandEnabled)
    {
        StageElement->RequestStageCommand(
            ResolveTargetPlacementId(),
            TargetGroup,
            TargetCommandTag);
    }
    OnTriggerSignal.Broadcast(this, ResolveTriggerSignal(true));
    OnTriggerActivated(TriggeringActor);
}

// 활성 조건이 해제되면 설정된 해제 명령과 표현 이벤트를 실행
void ACMStageTriggerBase::HandleTriggerDeactivated(AActor* TriggeringActor)
{
    if (bDirectTargetCommandEnabled)
    {
        StageElement->RequestStageCommand(
            ResolveTargetPlacementId(),
            TargetGroup,
            ReleaseCommandTag);
    }
    OnTriggerSignal.Broadcast(this, ResolveTriggerSignal(false));
    OnTriggerDeactivated(TriggeringActor);
}

ECMStageTriggerSignal ACMStageTriggerBase::ResolveTriggerSignal(
    bool bActivated) const
{
    return bActivated
        ? ECMStageTriggerSignal::Activated
        : ECMStageTriggerSignal::Deactivated;
}

// 직접 선택한 액터의 StageElement ID를 우선 사용하고 수동 ID를 대체 경로로 사용
FName ACMStageTriggerBase::ResolveTargetPlacementId() const
{
    if (IsValid(TargetActor))
    {
        if (const UCMStageElementComponent* TargetStageElement =
                TargetActor->FindComponentByClass<UCMStageElementComponent>())
        {
            return TargetStageElement->PlacementId;
        }
    }
    return TargetPlacementId;
}
