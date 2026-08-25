#include "Stage/Obstacle/CMStageObstacleBase.h"

#include "AsyncLoad/CMStageLoadLog.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayEffect.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Stage/Obstacle/Component/CMForceZoneComponent.h"
#include "Stage/Obstacle/Component/CMHazardComponent.h"
#include "Stage/Obstacle/Component/CMChimeraEffectZoneComponent.h"
#include "Stage/Obstacle/Component/CMObstacleDefinitionComponent.h"
#include "Stage/Obstacle/Component/CMObstacleMotionComponent.h"
#include "Stage/Obstacle/Component/CMStatusZoneComponent.h"
#include "Stage/Obstacle/Data/CMObstacleDefinition.h"

ACMStageObstacleBase::ACMStageObstacleBase()
{
    SetReplicateMovement(true);
    DefinitionComponent = CreateDefaultSubobject<UCMObstacleDefinitionComponent>(TEXT("ObstacleDefinition"));

    PrimaryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrimaryMesh"));
    PrimaryMesh->SetupAttachment(SceneRoot);
    PrimaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PrimaryEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PrimaryEffect"));
    PrimaryEffect->SetupAttachment(SceneRoot);
    PrimaryEffect->SetAutoActivate(false);

    LoopAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("LoopAudio"));
    LoopAudio->SetupAttachment(SceneRoot);
    LoopAudio->bAutoActivate = false;
}

// 스테이지 명령과 Definition 완료 이벤트를 연결하고 초기 준비 상태 계산
void ACMStageObstacleBase::BeginPlay()
{
    Super::BeginPlay();

    DefinitionComponent->OnDefinitionReady.AddDynamic(this, &ThisClass::HandleDefinitionReady);
    DefinitionComponent->OnDefinitionFailed.AddDynamic(this, &ThisClass::HandleDefinitionFailed);

    bDefinitionReady = !DefinitionComponent->HasDefinition();
    HandleElementActiveChanged(false);

    if (DefinitionComponent->HasDefinition())
    {
        PrimaryMesh->SetVisibility(false, true);
        DefinitionComponent->RefreshDefinitionState();
        if (UCMObstacleDefinition* LoadedDefinition = DefinitionComponent->GetLoadedDefinition())
        {
            HandleDefinitionReady(LoadedDefinition);
        }
        else if (DefinitionComponent->HasDefinitionFailed())
        {
            HandleDefinitionFailed();
        }
    }
    else
    {
        RefreshElementActiveState();
        HandleElementActiveChanged(IsElementActive());
    }
}

// 장애물을 작동 가능한 상태로 전환
void ACMStageObstacleBase::ActivateObstacle()
{
    ActivateElement();
}

// 장애물의 작동을 중지
void ACMStageObstacleBase::DeactivateObstacle()
{
    DeactivateElement();
}

// 장애물을 초기 활성 상태로 되돌리고 하위 구현을 초기화
void ACMStageObstacleBase::ResetObstacle()
{
    ResetElement();
}

// Definition 준비 여부를 공통 활성화 요청의 추가 조건으로 사용
bool ACMStageObstacleBase::CanActivateElement() const
{
    return bDefinitionReady;
}

// 공통 활성 상태를 장애물 컴포넌트와 블루프린트 표현에 전달
void ACMStageObstacleBase::HandleElementActiveChanged_Implementation(bool bIsActive)
{
    Super::HandleElementActiveChanged_Implementation(bIsActive);
    const bool bLocallyReadyAndActive = bIsActive && bDefinitionReady;
    ApplyComponentActiveState(bLocallyReadyAndActive);
    OnObstacleActiveChanged(bLocallyReadyAndActive);
}

// 장애물 이동과 블루프린트 전용 상태를 레벨 시작 상태로 복원
void ACMStageObstacleBase::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    ResetMotionComponents();
    OnObstacleReset();
}

// 준비된 Definition 에셋을 적용하고 대기 중인 활성화 요청 재평가
void ACMStageObstacleBase::HandleDefinitionReady(UCMObstacleDefinition* LoadedDefinition)
{
    if (bDefinitionReady)
    {
        return;
    }
    if (!ApplyDefinitionAssets(LoadedDefinition))
    {
        HandleDefinitionFailed();
        return;
    }

    bDefinitionReady = true;
    PrimaryMesh->SetVisibility(true, true);
    if (HasAuthority())
    {
        RefreshElementActiveState();
    }
    HandleElementActiveChanged(IsElementActive());
}

// 실패한 Definition 장애물을 모든 기능과 기본 연출이 꺼진 상태로 유지
void ACMStageObstacleBase::HandleDefinitionFailed()
{
    bDefinitionReady = false;
    PrimaryMesh->SetVisibility(false, true);
    if (HasAuthority())
    {
        RefreshElementActiveState();
    }
    HandleElementActiveChanged(false);
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

// Gameplay Bundle에 포함된 Soft Asset을 검증한 뒤 공통 표현과 Hazard에 적용
bool ACMStageObstacleBase::ApplyDefinitionAssets(UCMObstacleDefinition* LoadedDefinition)
{
    if (!IsValid(LoadedDefinition))
    {
        return false;
    }

    UStaticMesh* LoadedMesh = LoadedDefinition->PrimaryMesh.Get();
    if (!LoadedDefinition->PrimaryMesh.IsNull() && !LoadedMesh)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Obstacle PrimaryMesh was not prepared by the Gameplay bundle. Actor=%s Definition=%s"),
            *GetNameSafe(this), *GetNameSafe(LoadedDefinition));
        return false;
    }

    TArray<UMaterialInterface*> LoadedMaterials;
    for (const TSoftObjectPtr<UMaterialInterface>& Material : LoadedDefinition->Materials)
    {
        UMaterialInterface* LoadedMaterial = Material.Get();
        if (!Material.IsNull() && !LoadedMaterial)
        {
            UE_LOG(LogChimeraStageLoad, Error,
                TEXT("Obstacle Material was not prepared by the Gameplay bundle. Actor=%s Definition=%s Asset=%s"),
                *GetNameSafe(this), *GetNameSafe(LoadedDefinition), *Material.ToSoftObjectPath().ToString());
            return false;
        }
        LoadedMaterials.Add(LoadedMaterial);
    }

    UNiagaraSystem* LoadedNiagara = LoadedDefinition->NiagaraSystem.Get();
    USoundBase* LoadedSound = LoadedDefinition->LoopSound.Get();
    UClass* LoadedGameplayEffect =
        LoadedDefinition->ChimeraEffect.GameplayEffectClass.Get();
    if (LoadedDefinition->ChimeraEffect.bEnabled
        && LoadedDefinition->ChimeraEffect.GameplayEffectClass.IsNull())
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Obstacle ChimeraEffect is enabled without a GameplayEffectClass. Actor=%s Definition=%s"),
            *GetNameSafe(this), *GetNameSafe(LoadedDefinition));
        return false;
    }
    if ((!LoadedDefinition->NiagaraSystem.IsNull() && !LoadedNiagara)
        || (!LoadedDefinition->LoopSound.IsNull() && !LoadedSound)
        || (!LoadedDefinition->ChimeraEffect.GameplayEffectClass.IsNull()
            && !LoadedGameplayEffect))
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Obstacle Definition bundle is incomplete. Actor=%s Definition=%s"),
            *GetNameSafe(this), *GetNameSafe(LoadedDefinition));
        return false;
    }

    PrimaryMesh->SetStaticMesh(LoadedMesh);
    for (int32 Index = 0; Index < LoadedMaterials.Num(); ++Index)
    {
        PrimaryMesh->SetMaterial(Index, LoadedMaterials[Index]);
    }
    PrimaryEffect->SetAsset(LoadedNiagara);
    LoopAudio->SetSound(LoadedSound);

    TInlineComponentArray<UCMHazardComponent*> HazardComponents(this);
    for (UCMHazardComponent* HazardComponent : HazardComponents)
    {
        HazardComponent->ConfigurePartEffect(LoadedDefinition->PartEffect);
    }

    TInlineComponentArray<UCMChimeraEffectZoneComponent*>
        ChimeraEffectZoneComponents(this);
    for (UCMChimeraEffectZoneComponent* ChimeraEffectZone
        : ChimeraEffectZoneComponents)
    {
        ChimeraEffectZone->ConfigureChimeraEffect(
            LoadedDefinition->ChimeraEffect,
            LoadedGameplayEffect);
    }
    return true;
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
