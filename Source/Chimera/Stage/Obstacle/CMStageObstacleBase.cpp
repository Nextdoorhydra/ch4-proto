#include "Stage/Obstacle/CMStageObstacleBase.h"

#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "Stage/Obstacle/Component/CMChimeraEffectZoneComponent.h"
#include "Stage/Obstacle/Component/CMForceZoneComponent.h"
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
    const bool bHasStatusSelection = BalanceSelection.StatusBalanceTable
        && !BalanceSelection.StatusBalanceRow.IsNone();
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
    if (!BalanceSelection.StatusBalanceRow.IsNone()
        && !BalanceSelection.StatusBalanceTable)
    {
        UE_LOG(LogChimeraObstacleBalance, Error,
            TEXT("Status balance row is set without a table. Obstacle=%s Row=%s"),
            *GetPathName(), *BalanceSelection.StatusBalanceRow.ToString());
        return;
    }
    if (!bHasDamageSelection && !bHasStatusSelection && !bHasCustomDamage)
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

    const FCMObstacleStatusBalanceTableRow* StatusRow = nullptr;
    if (bHasStatusSelection)
    {
        if (BalanceSelection.StatusBalanceTable->GetRowStruct()
            != FCMObstacleStatusBalanceTableRow::StaticStruct())
        {
            UE_LOG(LogChimeraObstacleBalance, Error,
                TEXT("Status balance table has wrong row struct. Obstacle=%s Table=%s"),
                *GetPathName(), *BalanceSelection.StatusBalanceTable->GetPathName());
            return;
        }
        StatusRow = BalanceSelection.StatusBalanceTable
            ->FindRow<FCMObstacleStatusBalanceTableRow>(
                BalanceSelection.StatusBalanceRow,
                TEXT("CMStageObstacleBase.StatusBalance"));
        if (!StatusRow)
        {
            UE_LOG(LogChimeraObstacleBalance, Error,
                TEXT("Status balance row was not found. Obstacle=%s Row=%s"),
                *GetPathName(), *BalanceSelection.StatusBalanceRow.ToString());
            return;
        }
    }
    const ACMStageDirector* Director = StageElement
        ? StageElement->GetRegisteredDirector() : nullptr;
    const float StageDamageMultiplier = Director
        ? Director->GetObstacleDamageMultiplier() : 1.0f;
    ResolvedObstacleBalance = CMObstacleBalance::Resolve(
        DamageRow, StatusRow, BalanceSelection, StageDamageMultiplier);
}

void ACMStageObstacleBase::ApplyResolvedBalance()
{
    if (!ResolvedObstacleBalance.bValid)
    {
        return;
    }

    const bool bPartStatus =
        ResolvedObstacleBalance.StatusEffect == ECMObstacleStatusEffect::PartSlowed
        || ResolvedObstacleBalance.StatusEffect
            == ECMObstacleStatusEffect::PartElectrified;
    const bool bBodyStatus =
        ResolvedObstacleBalance.StatusEffect == ECMObstacleStatusEffect::BodyConfused
        || ResolvedObstacleBalance.StatusEffect
            == ECMObstacleStatusEffect::BodyDelirious
        || ResolvedObstacleBalance.StatusEffect
            == ECMObstacleStatusEffect::BodyBlinded
        || ResolvedObstacleBalance.StatusEffect
            == ECMObstacleStatusEffect::BodyVisionReduced;
    PartEffect.bEnabled = ResolvedObstacleBalance.Damage > 0.0f || bPartStatus;
    PartEffect.DamagePerApplication = ResolvedObstacleBalance.Damage;
    PartEffect.StatusDuration = ResolvedObstacleBalance.Duration;
    PartEffect.StatusTag = FGameplayTag();
    PartEffect.MovementMultiplier = 1.0f;
    PartEffect.bBlocksAbility = false;

    if (ResolvedObstacleBalance.StatusEffect == ECMObstacleStatusEffect::PartSlowed)
    {
        PartEffect.StatusTag = CMPartStatusTags::Slowed;
        PartEffect.MovementMultiplier = ResolvedObstacleBalance.PrimaryStatusValue;
    }
    else if (ResolvedObstacleBalance.StatusEffect
        == ECMObstacleStatusEffect::PartElectrified)
    {
        PartEffect.StatusTag = CMPartStatusTags::Electrified;
        PartEffect.bBlocksAbility = true;
    }

    ChimeraEffect.bEnabled = bBodyStatus && ChimeraEffect.GameplayEffectClass;
    ChimeraEffect.StatusDuration = ResolvedObstacleBalance.Duration;
    ChimeraEffect.StatusEffect = ResolvedObstacleBalance.StatusEffect;
    ChimeraEffect.PrimaryStatusValue = ResolvedObstacleBalance.PrimaryStatusValue;
    ChimeraEffect.SecondaryStatusValue = ResolvedObstacleBalance.SecondaryStatusValue;
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

// 해석된 효과 설정을 부착된 Hazard와 ChimeraEffectZone에 전달
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
