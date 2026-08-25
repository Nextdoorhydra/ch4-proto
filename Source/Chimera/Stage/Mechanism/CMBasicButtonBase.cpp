#include "Stage/Mechanism/CMBasicButtonBase.h"

#include "Components/BoxComponent.h"
#include "Parts/Arm/CMArmPart.h"
#include "Stage/Mechanism/Component/CMActivationTriggerComponent.h"

ACMBasicButtonBase::ACMBasicButtonBase()
{
    HitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("HitVolume"));
    HitVolume->SetupAttachment(SceneRoot);
    HitVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    HitVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    HitVolume->SetGenerateOverlapEvents(true);
}

// 서버에서 팔 타격 영역 진입 이벤트 구독
void ACMBasicButtonBase::BeginPlay()
{
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
    if (ACMArmPart* ArmPart = Cast<ACMArmPart>(OtherActor);
        ArmPart && ArmPart->IsSwinging())
    {
        if (bToggleOnHit && ActivationTrigger->IsTriggered())
        {
            ReleaseButton(ArmPart);
        }
        else
        {
            PressButton(ArmPart);
        }
    }
}
