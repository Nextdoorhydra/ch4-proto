#include "Stage/Mechanism/CMStageButtonBase.h"

#include "Stage/CMStageCommandTags.h"
#include "Stage/CMStageElementComponent.h"
#include "Stage/Mechanism/Component/CMActivationTriggerComponent.h"

ACMStageButtonBase::ACMStageButtonBase()
{
    ActivationTrigger = CreateDefaultSubobject<UCMActivationTriggerComponent>(TEXT("ActivationTrigger"));
    TargetCommandTag = CMStageCommandTags::Mechanism_Activate;
    ReleaseCommandTag = CMStageCommandTags::Mechanism_Deactivate;
}

// 런타임에 버튼 작동 이벤트를 대상 명령 처리 함수에 연결
void ACMStageButtonBase::BeginPlay()
{
    Super::BeginPlay();
    ActivationTrigger->OnActivated.AddDynamic(this, &ThisClass::HandleTriggerActivated);
    ActivationTrigger->OnDeactivated.AddDynamic(this, &ThisClass::HandleTriggerDeactivated);
}

// 버튼의 공통 트리거에 작동 요청 전달
bool ACMStageButtonBase::PressButton(AActor* PressingActor)
{
    return HasAuthority() && ActivationTrigger->TryActivate(PressingActor);
}

// 유지형 버튼의 해제 요청을 공통 트리거에 전달
bool ACMStageButtonBase::ReleaseButton(AActor* ReleasingActor)
{
    return HasAuthority()
        && ActivationTrigger->SetTriggeredState(false, ReleasingActor);
}

// 버튼 활성 상태를 실제 입력 허용 상태에 연결
void ACMStageButtonBase::HandleMechanismActiveChanged_Implementation(bool bIsActive)
{
    Super::HandleMechanismActiveChanged_Implementation(bIsActive);
    if (HasAuthority())
    {
        ActivationTrigger->SetTriggerEnabled(bIsActive);
    }
}

// 버튼 작동 이력을 레벨 시작 상태로 복원
void ACMStageButtonBase::HandleMechanismReset_Implementation()
{
    Super::HandleMechanismReset_Implementation();
    ActivationTrigger->ResetTrigger();
}

// 작동한 버튼의 대상 명령을 StageDirector로 전달
void ACMStageButtonBase::HandleTriggerActivated(AActor* TriggeringActor)
{
    StageElement->RequestStageCommand(
        ResolveTargetPlacementId(),
        TargetGroup,
        TargetCommandTag);
    OnButtonPressed(TriggeringActor);
}

// 버튼 해제 명령을 같은 대상에 전달하고 표현 이벤트 호출
void ACMStageButtonBase::HandleTriggerDeactivated(AActor* TriggeringActor)
{
    StageElement->RequestStageCommand(
        ResolveTargetPlacementId(),
        TargetGroup,
        ReleaseCommandTag);
    OnButtonReleased(TriggeringActor);
}

// 직접 선택한 액터의 StageElement ID를 우선 사용하고 기존 수동 ID를 대체 경로로 유지
FName ACMStageButtonBase::ResolveTargetPlacementId() const
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
