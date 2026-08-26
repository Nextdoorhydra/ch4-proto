#include "Stage/Trigger/CMBasicButtonBase.h"

#include "Components/BoxComponent.h"
#include "Parts/Arm/CMArmPart.h"
#include "Player/CMChimera.h"
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

// 서버에서 팔 타격 영역 진입 이벤트 구독
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
    if (HasAuthority())
    {
        HitVolume->OnComponentBeginOverlap.AddUniqueDynamic(
            this,
            &ThisClass::HandleHitVolumeBeginOverlap);
    }
}

// 일반 접촉은 무시하고 현재 휘두르는 팔만 버튼 입력으로 인정
void ACMBasicButtonBase::HandleHitVolumeBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
#if WITH_EDITOR
    if (bAllowChimeraBodyOverlapForTesting
        && Cast<ACMChimera>(OtherActor))
    {
        HandleValidButtonInput(OtherActor);
        return;
    }
#endif

    if (ACMArmPart* ArmPart = Cast<ACMArmPart>(OtherActor);
        ArmPart && ArmPart->IsSwinging())
    {
        HandleValidButtonInput(ArmPart);
    }
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
