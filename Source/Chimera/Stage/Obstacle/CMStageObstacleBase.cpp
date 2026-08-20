#include "Stage/Obstacle/CMStageObstacleBase.h"

#include "AsyncLoad/CMStageLoadLog.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayEffect.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Stage/CMStageCommandTags.h"
#include "Stage/CMStageElementComponent.h"
#include "Stage/Obstacle/Component/CMForceZoneComponent.h"
#include "Stage/Obstacle/Component/CMHazardComponent.h"
#include "Stage/Obstacle/Component/CMObstacleDefinitionComponent.h"
#include "Stage/Obstacle/Component/CMObstacleMotionComponent.h"
#include "Stage/Obstacle/Component/CMStatusZoneComponent.h"
#include "Stage/Obstacle/Data/CMObstacleDefinition.h"

ACMStageObstacleBase::ACMStageObstacleBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    StageElement = CreateDefaultSubobject<UCMStageElementComponent>(TEXT("StageElement"));
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

    StageElement->OnStageCommandReceived.AddDynamic(this, &ThisClass::HandleStageCommand);
    DefinitionComponent->OnDefinitionReady.AddDynamic(this, &ThisClass::HandleDefinitionReady);
    DefinitionComponent->OnDefinitionFailed.AddDynamic(this, &ThisClass::HandleDefinitionFailed);

    bActivationRequested = bStartActive;
    bDefinitionReady = !DefinitionComponent->HasDefinition();
    bObstacleActive = false;
    ApplyComponentActiveState(false);

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
    else if (HasAuthority())
    {
        UpdateEffectiveActiveState();
    }
    OnObstacleActiveChanged(bObstacleActive);
}

// 활성 상태를 네트워크로 복제
void ACMStageObstacleBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, bObstacleActive);
}

// 장애물을 작동 가능한 상태로 전환
void ACMStageObstacleBase::ActivateObstacle()
{
    if (HasAuthority())
    {
        bActivationRequested = true;
        UpdateEffectiveActiveState();
    }
}

// 장애물의 작동을 중지
void ACMStageObstacleBase::DeactivateObstacle()
{
    if (HasAuthority())
    {
        bActivationRequested = false;
        UpdateEffectiveActiveState();
    }
}

// 장애물을 초기 활성 상태로 되돌리고 하위 구현을 초기화
void ACMStageObstacleBase::ResetObstacle()
{
    if (!HasAuthority())
    {
        return;
    }

    bActivationRequested = bStartActive;

    // 실행 중인 기능을 먼저 끄고 초기화 이후 활성 상태가 반드시 다시 적용되도록 처리
    SetObstacleActive(false);
    ResetMotionComponents();
    UpdateEffectiveActiveState();
    OnObstacleReset();
}

// StageDirector의 공통 명령 태그를 장애물 동작에 연결
void ACMStageObstacleBase::HandleStageCommand(FGameplayTag CommandTag, UObject* CommandInstigator)
{
    if (!HasAuthority())
    {
        return;
    }

    if (CommandTag.MatchesTagExact(CMStageCommandTags::Effect_Activate))
    {
        ActivateObstacle();
    }
    else if (CommandTag.MatchesTagExact(CMStageCommandTags::Effect_Deactivate))
    {
        DeactivateObstacle();
    }
    else if (CommandTag.MatchesTagExact(CMStageCommandTags::Effect_Restart))
    {
        ResetObstacle();
    }
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
    ApplyComponentActiveState(bObstacleActive);
    OnObstacleActiveChanged(bObstacleActive);
    if (HasAuthority())
    {
        UpdateEffectiveActiveState();
    }
}

// 실패한 Definition 장애물을 모든 기능과 기본 연출이 꺼진 상태로 유지
void ACMStageObstacleBase::HandleDefinitionFailed()
{
    bDefinitionReady = false;
    PrimaryMesh->SetVisibility(false, true);
    ApplyComponentActiveState(false);
    OnObstacleActiveChanged(false);
    if (HasAuthority())
    {
        SetObstacleActive(false);
    }
}

// 복제된 활성 상태를 클라이언트의 표현에 전달
void ACMStageObstacleBase::OnRep_ObstacleActive()
{
    const bool bLocallyReadyAndActive = bObstacleActive && bDefinitionReady;
    ApplyComponentActiveState(bLocallyReadyAndActive);
    OnObstacleActiveChanged(bLocallyReadyAndActive);
}

// 활성화 요청과 로컬 Definition 준비 상태가 모두 만족될 때만 실제 작동 허용
void ACMStageObstacleBase::UpdateEffectiveActiveState()
{
    if (HasAuthority())
    {
        SetObstacleActive(bActivationRequested && bDefinitionReady);
    }
}

// 상태가 실제로 바뀐 경우에만 표현 갱신 이벤트를 호출
void ACMStageObstacleBase::SetObstacleActive(bool bNewActive)
{
    if (bObstacleActive == bNewActive)
    {
        return;
    }

    bObstacleActive = bNewActive;
    ApplyComponentActiveState(bObstacleActive);
    OnObstacleActiveChanged(bObstacleActive);
    ForceNetUpdate();
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
    UClass* LoadedGameplayEffect = LoadedDefinition->GameplayEffectClass.Get();
    if ((!LoadedDefinition->NiagaraSystem.IsNull() && !LoadedNiagara)
        || (!LoadedDefinition->LoopSound.IsNull() && !LoadedSound)
        || (!LoadedDefinition->GameplayEffectClass.IsNull() && !LoadedGameplayEffect))
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
        HazardComponent->ConfigureHazard(
            LoadedGameplayEffect,
            LoadedDefinition->HazardEffectTag,
            LoadedDefinition->ApplicationMode,
            LoadedDefinition->PeriodSeconds);
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
