#include "Stage/Obstacle/CMStageObstacleBase.h"

#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "Stage/Obstacle/Component/CMChimeraEffectZoneComponent.h"
#include "Stage/Obstacle/Component/CMForceZoneComponent.h"
#include "Stage/Obstacle/Component/CMHazardComponent.h"
#include "Stage/Obstacle/Component/CMObstacleMotionComponent.h"
#include "Stage/Obstacle/Component/CMStatusZoneComponent.h"

ACMStageObstacleBase::ACMStageObstacleBase()
{
    SetReplicateMovement(true);

    PrimaryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrimaryMesh"));
    PrimaryMesh->SetupAttachment(SceneRoot);
    PrimaryMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    PrimaryMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PrimaryMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

    PrimaryEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PrimaryEffect"));
    PrimaryEffect->SetupAttachment(SceneRoot);
    PrimaryEffect->SetAutoActivate(false);

    LoopAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("LoopAudio"));
    LoopAudio->SetupAttachment(SceneRoot);
    LoopAudio->bAutoActivate = false;
}

// BP 또는 배치 인스턴스에 직접 설정된 장애물 효과 준비
void ACMStageObstacleBase::BeginPlay()
{
    Super::BeginPlay();
    ConfigureDirectEffects();
}

void ACMStageObstacleBase::ActivateObstacle()
{
    ActivateElement();
}

void ACMStageObstacleBase::DeactivateObstacle()
{
    DeactivateElement();
}

void ACMStageObstacleBase::ResetObstacle()
{
    ResetElement();
}

// 공통 활성 상태를 장애물 컴포넌트와 블루프린트 표현에 전달
void ACMStageObstacleBase::HandleElementActiveChanged_Implementation(bool bIsActive)
{
    Super::HandleElementActiveChanged_Implementation(bIsActive);
    ApplyComponentActiveState(bIsActive);
    OnObstacleActiveChanged(bIsActive);
}

// 장애물 이동과 블루프린트 전용 상태를 레벨 시작 상태로 복원
void ACMStageObstacleBase::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    ResetMotionComponents();
    OnObstacleReset();
}

// 부착된 이동·위험·장판·힘 컴포넌트와 기본 연출을 같은 활성 상태로 통일
void ACMStageObstacleBase::ApplyComponentActiveState(bool bIsActive)
{
    TInlineComponentArray<UCMObstacleMotionComponent*> MotionComponents(this);
    for (UCMObstacleMotionComponent* MotionComponent : MotionComponents)
    {
        if (bIsActive)
        {
            MotionComponent->StartMotion();
        }
        else
        {
            MotionComponent->StopMotion();
        }
    }

    TInlineComponentArray<UCMHazardComponent*> HazardComponents(this);
    for (UCMHazardComponent* HazardComponent : HazardComponents)
    {
        HazardComponent->SetHazardEnabled(bIsActive);
    }

    TInlineComponentArray<UCMStatusZoneComponent*> StatusZoneComponents(this);
    for (UCMStatusZoneComponent* StatusZoneComponent : StatusZoneComponents)
    {
        StatusZoneComponent->SetZoneEnabled(bIsActive);
    }

    TInlineComponentArray<UCMChimeraEffectZoneComponent*>
        ChimeraEffectZoneComponents(this);
    for (UCMChimeraEffectZoneComponent* ChimeraEffectZone
        : ChimeraEffectZoneComponents)
    {
        ChimeraEffectZone->SetZoneEnabled(bIsActive);
    }

    TInlineComponentArray<UCMForceZoneComponent*> ForceZoneComponents(this);
    for (UCMForceZoneComponent* ForceZoneComponent : ForceZoneComponents)
    {
        ForceZoneComponent->SetZoneEnabled(bIsActive);
    }

    if (PrimaryEffect && PrimaryEffect->GetAsset())
    {
        if (bIsActive)
        {
            PrimaryEffect->Activate(true);
        }
        else
        {
            PrimaryEffect->Deactivate();
        }
    }
    if (LoopAudio && LoopAudio->GetSound())
    {
        if (bIsActive)
        {
            LoopAudio->Play();
        }
        else
        {
            LoopAudio->Stop();
        }
    }

    HandleObstacleActiveStateChanged(bIsActive);
}

// 직접 지정된 효과 설정을 부착된 Hazard와 ChimeraEffectZone에 전달
void ACMStageObstacleBase::ConfigureDirectEffects()
{
    TInlineComponentArray<UCMHazardComponent*> HazardComponents(this);
    for (UCMHazardComponent* HazardComponent : HazardComponents)
    {
        HazardComponent->ConfigurePartEffect(PartEffect);
    }

    TInlineComponentArray<UCMChimeraEffectZoneComponent*>
        ChimeraEffectZoneComponents(this);
    for (UCMChimeraEffectZoneComponent* ChimeraEffectZone
        : ChimeraEffectZoneComponents)
    {
        ChimeraEffectZone->ConfigureChimeraEffect(
            ChimeraEffect,
            ChimeraEffect.GameplayEffectClass);
    }
}

// 부착된 모든 이동 컴포넌트와 액터 Transform을 최초 배치 상태로 복원
void ACMStageObstacleBase::ResetMotionComponents()
{
    TInlineComponentArray<UCMObstacleMotionComponent*> MotionComponents(this);
    for (UCMObstacleMotionComponent* MotionComponent : MotionComponents)
    {
        MotionComponent->ResetMotion();
    }
}
