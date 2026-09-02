#include "Stage/Obstacle/CMStageObstacleBase.h"

#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "Stage/Obstacle/Component/CMForceZoneComponent.h"
#include "Stage/Obstacle/Component/CMFlashComponent.h"
#include "Stage/Obstacle/Component/CMHazardComponent.h"
#include "Stage/Obstacle/Component/CMObstacleMotionComponent.h"
#include "Stage/Obstacle/Component/CMStatusZoneComponent.h"
#include "Parts/Core/CMPartStatusTags.h"
#include "Stage/CMStageDirector.h"
#include "Stage/CMStageElementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraObstacleBalance, Log, All);

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

// 배치 인스턴스의 밸런스를 해석하고 런타임 효과 컴포넌트 준비
void ACMStageObstacleBase::BeginPlay()
{
    Super::BeginPlay();
    ResolveBalance();
    ApplyResolvedBalance();
    ConfigureDirectEffects();
}

void ACMStageObstacleBase::ResolveBalance()
{
    ResolvedObstacleBalance = {};
    const bool bHasDamageSelection = BalanceSelection.DamageBalanceTable
        && !BalanceSelection.DamageBalanceRow.IsNone();
    const bool bHasCustomDamage = BalanceSelection.DamageMode
        == ECMObstacleDamageMode::Custom;

    if (!BalanceSelection.DamageBalanceRow.IsNone()
        && !BalanceSelection.DamageBalanceTable)
    {
        UE_LOG(LogChimeraObstacleBalance, Error,
            TEXT("Damage balance row is set without a table. Obstacle=%s Row=%s"),
            *GetPathName(), *BalanceSelection.DamageBalanceRow.ToString());
        return;
    }
    if (!bHasDamageSelection && !bHasCustomDamage)
    {
        return;
    }

    const FCMObstacleDamageBalanceTableRow* DamageRow = nullptr;
    if (bHasDamageSelection)
    {
        if (BalanceSelection.DamageBalanceTable->GetRowStruct()
            != FCMObstacleDamageBalanceTableRow::StaticStruct())
        {
            UE_LOG(LogChimeraObstacleBalance, Error,
                TEXT("Damage balance table has wrong row struct. Obstacle=%s Table=%s"),
                *GetPathName(), *BalanceSelection.DamageBalanceTable->GetPathName());
            return;
        }
        DamageRow = BalanceSelection.DamageBalanceTable
            ->FindRow<FCMObstacleDamageBalanceTableRow>(
                BalanceSelection.DamageBalanceRow,
                TEXT("CMStageObstacleBase.DamageBalance"));
        if (!DamageRow)
        {
            UE_LOG(LogChimeraObstacleBalance, Error,
                TEXT("Damage balance row was not found. Obstacle=%s Row=%s"),
                *GetPathName(), *BalanceSelection.DamageBalanceRow.ToString());
            return;
        }
    }

    const ACMStageDirector* Director = StageElement
        ? StageElement->GetRegisteredDirector() : nullptr;
    const float StageDamageMultiplier = Director
        ? Director->GetObstacleDamageMultiplier() : 1.0f;
    ResolvedObstacleBalance = CMObstacleBalance::Resolve(
        DamageRow, BalanceSelection, StageDamageMultiplier);
}

void ACMStageObstacleBase::ApplyResolvedBalance()
{
    PartEffect.bEnabled = ResolvedObstacleBalance.Damage > 0.0f
        || PartEffect.StatusEffect != ECMPartObstacleStatusEffect::None;
    PartEffect.DamagePerApplication = ResolvedObstacleBalance.bValid
        ? ResolvedObstacleBalance.Damage : 0.0f;
    PartEffect.StatusTag = FGameplayTag();
    PartEffect.bBlocksAbility = false;

    if (PartEffect.StatusEffect == ECMPartObstacleStatusEffect::Slowed)
    {
        PartEffect.StatusTag = CMPartStatusTags::Slowed;
        PartEffect.MovementMultiplier = FMath::Clamp(
            PartEffect.MovementMultiplier, 0.0f, 1.0f);
    }
    else if (PartEffect.StatusEffect
        == ECMPartObstacleStatusEffect::Electrified)
    {
        PartEffect.StatusTag = CMPartStatusTags::Electrified;
        PartEffect.MovementMultiplier = 1.0f;
        PartEffect.bBlocksAbility = true;
    }
    else
    {
        PartEffect.MovementMultiplier = 1.0f;
    }

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

// 활성화 요청마다 부착된 단발 섬광을 실행한다.
// 시작 상태 복원과 BeginPlay 초기화에서는 호출되지 않는다.
void ACMStageObstacleBase::HandleElementActivationRequested()
{
    if (!IsObstacleActive())
    {
        return;
    }

    TInlineComponentArray<UCMFlashComponent*> FlashComponents(this);
    for (UCMFlashComponent* FlashComponent : FlashComponents)
    {
        FlashComponent->TriggerFlash();
    }
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

    TInlineComponentArray<UCMForceZoneComponent*> ForceZoneComponents(this);
    for (UCMForceZoneComponent* ForceZoneComponent : ForceZoneComponents)
    {
        ForceZoneComponent->SetZoneEnabled(bIsActive);
    }

    TInlineComponentArray<UCMFlashComponent*> FlashComponents(this);
    for (UCMFlashComponent* FlashComponent : FlashComponents)
    {
        FlashComponent->SetFlashEnabled(bIsActive);
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

// 해석된 피해와 파츠 상태 설정을 부착된 Hazard에 전달
void ACMStageObstacleBase::ConfigureDirectEffects()
{
    TInlineComponentArray<UCMHazardComponent*> HazardComponents(this);
    for (UCMHazardComponent* HazardComponent : HazardComponents)
    {
        HazardComponent->ConfigurePartEffect(PartEffect);
        HazardComponent->ConfigureHeadVisionEffect(HeadVisionEffect);
        HazardComponent->ConfigureControlEffect(ControlEffect);
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
