#include "Stage/Trigger/CMBasicButtonBase.h"

#include "Components/BoxComponent.h"
#include "Parts/Arm/CMArmPart.h"
#include "Stage/CMStageCommandTags.h"
#include "Stage/Trigger/Component/CMActivationTriggerComponent.h"

ACMBasicButtonBase::ACMBasicButtonBase()
{
    HitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("HitVolume"));
    HitVolume->SetupAttachment(SceneRoot);
    HitVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    HitVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    HitVolume->SetGenerateOverlapEvents(true);

    // 일반 버튼은 대상의 초기 상태와 관계없이 현재 활성 상태를 반전
    TargetCommandTag = CMStageCommandTags::Mechanism_Toggle;
    ReleaseCommandTag = CMStageCommandTags::Mechanism_Toggle;
}

// 일반 버튼의 토글 및 일회성 정책 초기화
void ACMBasicButtonBase::BeginPlay()
{
    // 기존 BP나 레벨 인스턴스에 저장된 명시적 태그와 관계없이 일반 버튼은 토글로 통일
    TargetCommandTag = CMStageCommandTags::Mechanism_Toggle;
    ReleaseCommandTag = CMStageCommandTags::Mechanism_Toggle;
    if (bToggleOnHit)
    {
        ActivationTrigger->bOneShot = false;
    }
    Super::BeginPlay();
}

void ACMBasicButtonBase::NotifySwingHit(
    ACMArmPart* ArmPart, UPrimitiveComponent* HitComponent)
{
    if (!HasAuthority() || !IsValid(ArmPart) || !ArmPart->HasAuthority()
        || !ArmPart->IsSwinging() || !ArmPart->IsOperational()
        || HitComponent != HitVolume || !IsElementActive())
    {
        return;
    }

    const FGuid AttackId = ArmPart->GetCurrentSwingAttackId();
    if (!AttackId.IsValid())
    {
        return;
    }
    for (auto It = LastSwingAttackIds.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
    }
    FGuid& LastAttackId = LastSwingAttackIds.FindOrAdd(ArmPart);
    if (LastAttackId == AttackId)
    {
        return;
    }
    LastAttackId = AttackId;
    HandleValidButtonInput(ArmPart);
}

ECMStageTriggerSignal ACMBasicButtonBase::ResolveTriggerSignal(
    bool bActivated) const
{
    // 반복 토글 버튼은 현재 눌림 상태를 퍼즐 조건에 전달하고 일회성 버튼은 순간 입력만 전달
    if (bToggleOnHit)
    {
        return bActivated
            ? ECMStageTriggerSignal::Activated
            : ECMStageTriggerSignal::Deactivated;
    }
    return ECMStageTriggerSignal::Pulse;
}

// 반복 버튼은 눌림과 해제를 교대하고 일회성 버튼은 최초 눌림만 전달
void ACMBasicButtonBase::HandleValidButtonInput(AActor* TriggeringActor)
{
    if (bToggleOnHit && ActivationTrigger->IsTriggered())
    {
        ReleaseButton(TriggeringActor);
    }
    else
    {
        PressButton(TriggeringActor);
    }
}
