#include "Parts/Core/CMPartActorBase.h"

#include "Collision/CMCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/Part/CMPartLegArmTableRow.h"
#include "Data/Part/CMPartTierTableRow.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gore/CMGoreResponseComponent.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Combat/CMBattleComponent.h"
#include "Parts/Core/CMPartStatusComponent.h"
#include "Player/CMPartSlotComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPart, Log, All);

ACMPartActorBase::ACMPartActorBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);
    Tags.AddUnique(TEXT("TentacleInteractiveObject"));

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    PartMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PartMesh"));
    PartMesh->SetupAttachment(SceneRoot);
    PartMesh->SetCollisionResponseToChannel(
        CMCollision::WeaponTrace,
        ECR_Ignore
    );

    DamageHurtbox = CreateDefaultSubobject<UBoxComponent>(
        TEXT("DamageHurtbox")
    );
    DamageHurtbox->SetupAttachment(SceneRoot);
    DamageHurtbox->SetBoxExtent(FVector(50.0f));
    DamageHurtbox->SetCollisionProfileName(TEXT("CMHurtbox"));
    DamageHurtbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DamageHurtbox->SetCollisionObjectType(CMCollision::ChimeraHurtbox);
    DamageHurtbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    DamageHurtbox->SetCollisionResponseToChannel(
        ECC_WorldDynamic,
        ECR_Overlap
    );
    DamageHurtbox->SetCollisionResponseToChannel(
        CMCollision::WeaponTrace,
        ECR_Block
    );
    DamageHurtbox->SetGenerateOverlapEvents(true);
    DamageHurtbox->SetCanEverAffectNavigation(false);

    BattleComponent = CreateDefaultSubobject<UCMBattleComponent>(
        TEXT("BattleComponent")
    );
    PartStatusComponent = CreateDefaultSubobject<UCMPartStatusComponent>(
        TEXT("PartStatusComponent")
    );
    GoreResponseComponent = CreateDefaultSubobject<UCMGoreResponseComponent>(
        TEXT("GoreResponseComponent")
    );

    PartDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
        TEXT("/Game/Chimera/Data/Body/DT_LegArmDataTable.DT_LegArmDataTable")
    ));
    PartTierDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
        TEXT("/Game/Chimera/Data/Body/DT_PartTierDataTable.DT_PartTierDataTable")
    ));
}

ACMPartActorBase* ACMPartActorBase::SpawnPartFromDataRows(
    UObject* WorldContextObject,
    TSubclassOf<ACMPartActorBase> PartClass,
    FName InPartRowName,
    FName InTierRowName,
    const FTransform& SpawnTransform,
    AActor* InOwner
)
{
    UWorld* World = GEngine
        ? GEngine->GetWorldFromContextObject(
            WorldContextObject,
            EGetWorldErrorMode::ReturnNull)
        : nullptr;
    if (!World || !PartClass || World->GetNetMode() == NM_Client)
    {
        return nullptr;
    }

    ACMPartActorBase* PartActor =
        World->SpawnActorDeferred<ACMPartActorBase>(
            PartClass,
            SpawnTransform,
            InOwner,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );
    if (!PartActor)
    {
        return nullptr;
    }

    if (!InPartRowName.IsNone())
    {
        PartActor->PartRowName = InPartRowName;
    }
    if (!InTierRowName.IsNone())
    {
        PartActor->TierRowName = InTierRowName;
    }
    PartActor->FinishSpawning(SpawnTransform);
    return PartActor;
}

void ACMPartActorBase::BeginPlay()
{
    Super::BeginPlay();

    InitializeFromPartData();
    CaptureMountedPhysicsState();

    if (HasAuthority())
    {
        MaxHealth = FMath::Max(MaxHealth, 1.0f);
        Health = MaxHealth;
        bDead = false;
        bDisabled = false;
        ForceNetUpdate();
    }
    ApplyAttachmentPhysicsState();
}

void ACMPartActorBase::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMPartActorBase, AttachedSlotAddress);
    DOREPLIFETIME(ACMPartActorBase, bTentaclePullActive);
    DOREPLIFETIME(ACMPartActorBase, MaxHealth);
    DOREPLIFETIME(ACMPartActorBase, Health);
    DOREPLIFETIME(ACMPartActorBase, Strength);
    DOREPLIFETIME(ACMPartActorBase, BaseMovementImpulse);
    DOREPLIFETIME(ACMPartActorBase, MovementImpulseMultiplier);
    DOREPLIFETIME(ACMPartActorBase, PartRowName);
    DOREPLIFETIME(ACMPartActorBase, TierRowName);
    DOREPLIFETIME(ACMPartActorBase, Tier);
    DOREPLIFETIME(ACMPartActorBase, PartStateTags);
    DOREPLIFETIME(ACMPartActorBase, bDead);
    DOREPLIFETIME(ACMPartActorBase, bDisabled);
}

ECMPartSlotType ACMPartActorBase::GetPartType_Implementation() const
{
    return PartType;
}

TSubclassOf<UGameplayAbility>
ACMPartActorBase::GetGrantedAbilityClass_Implementation() const
{
    return GrantedAbilityClass;
}

void ACMPartActorBase::OnAttachedToPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    if (!PartSlot)
    {
        return;
    }

    SynchronizeAttachedPartSlot(PartSlot);
}

void ACMPartActorBase::OnDetachedFromPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    if (!HasAuthority())
    {
        return;
    }

    PendingContributingPlayerState.Reset();
    if (PartStatusComponent)
    {
        PartStatusComponent->ClearAllStatuses();
    }
    if (BattleComponent)
    {
        BattleComponent->EndParryWindow();
    }
    SynchronizeAttachedPartSlot(nullptr);
}

void ACMPartActorBase::SynchronizeAttachedPartSlot(
    UCMPartSlotComponent* PartSlot)
{
    if (!HasAuthority())
    {
        return;
    }

    AttachedPartSlot = PartSlot;
    AttachedSlotAddress = PartSlot
        ? PartSlot->GetSlotAddress()
        : FCMPartSlotAddress();
    bTentaclePullActive = false;
    TentacleReservationOwner.Reset();
    ApplyAttachmentPhysicsState();
    ForceNetUpdate();
}

void ACMPartActorBase::PrepareForPartSlotAttachment()
{
    CaptureMountedPhysicsState();
    if (!bMountedPhysicsStateCaptured || !PartMesh || !SceneRoot)
    {
        return;
    }

    PartMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
    PartMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    PartMesh->SetAllBodiesSimulatePhysics(false);
    PartMesh->SetSimulatePhysics(false);
    PartMesh->SetAllBodiesPhysicsBlendWeight(0.0f, false);
    PartMesh->SetPhysicsBlendWeight(0.0f);
    PartMesh->AttachToComponent(
        SceneRoot,
        FAttachmentTransformRules::KeepWorldTransform);
    PartMesh->SetRelativeTransform(
        MountedMeshRelativeTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);

    PartMesh->SetCollisionProfileName(MountedMeshCollisionProfile);
    ECollisionEnabled::Type MountedQueryCollision =
        MountedMeshCollisionEnabled.GetValue();
    if (MountedQueryCollision == ECollisionEnabled::QueryAndPhysics)
    {
        MountedQueryCollision = ECollisionEnabled::QueryOnly;
    }
    else if (MountedQueryCollision == ECollisionEnabled::PhysicsOnly)
    {
        MountedQueryCollision = ECollisionEnabled::NoCollision;
    }
    PartMesh->SetCollisionEnabled(MountedQueryCollision);
    PartMesh->SetGenerateOverlapEvents(bMountedMeshGenerateOverlapEvents);
    DamageHurtbox->SetCollisionEnabled(MountedHurtboxCollisionEnabled);
}

UCMPartSlotComponent* ACMPartActorBase::GetAttachedPartSlot() const
{
    if (UCMPartSlotComponent* PartSlot = AttachedPartSlot.Get())
    {
        return PartSlot;
    }

    for (USceneComponent* Parent = SceneRoot
            ? SceneRoot->GetAttachParent()
            : nullptr;
        Parent;
        Parent = Parent->GetAttachParent())
    {
        if (UCMPartSlotComponent* PartSlot =
            Cast<UCMPartSlotComponent>(Parent))
        {
            return PartSlot;
        }
    }
    return nullptr;
}

FCMPartSlotAddress ACMPartActorBase::GetAttachedSlotAddress() const
{
    return AttachedSlotAddress;
}

UCMBattleComponent* ACMPartActorBase::GetBattleComponent() const
{
    return BattleComponent;
}

UCMPartStatusComponent* ACMPartActorBase::GetPartStatusComponent() const
{
    return PartStatusComponent;
}

float ACMPartActorBase::GetHealth() const
{
    return Health;
}

float ACMPartActorBase::GetMaxHealth() const
{
    return MaxHealth;
}

float ACMPartActorBase::GetStrength() const
{
    return Strength;
}

float ACMPartActorBase::GetMovementImpulseMultiplier() const
{
    return MovementImpulseMultiplier
        * (PartStatusComponent
            ? PartStatusComponent->GetMovementMultiplier()
            : 1.0f);
}

float ACMPartActorBase::GetMovementImpulse() const
{
    return BaseMovementImpulse * GetMovementImpulseMultiplier();
}

void ACMPartActorBase::ApplyPartData(
    const FCMPartLegArmTableRow& PartRow
)
{
}

bool ACMPartActorBase::InitializeFromPartData()
{
    Tier = CMPartTier::FromRowName(TierRowName);

    if (PartDataTable.IsNull() || PartTierDataTable.IsNull()
        || PartRowName.IsNone() || TierRowName.IsNone())
    {
        return false;
    }

    UDataTable* LoadedTable = PartDataTable.LoadSynchronous();
    if (!LoadedTable)
    {
        UE_LOG(LogChimeraPart, Error,
            TEXT("[Part Data Failed] Part=%s Table=%s could not be loaded."),
            *GetName(),
            *PartDataTable.ToSoftObjectPath().ToString());
        return false;
    }

    UDataTable* LoadedTierTable = PartTierDataTable.LoadSynchronous();
    if (!LoadedTierTable)
    {
        UE_LOG(LogChimeraPart, Error,
            TEXT("[Part Data Failed] Part=%s TierTable=%s could not be loaded."),
            *GetName(),
            *PartTierDataTable.ToSoftObjectPath().ToString());
        return false;
    }

    const FCMPartLegArmTableRow* PartRow =
        LoadedTable->FindRow<FCMPartLegArmTableRow>(
            PartRowName,
            TEXT("ACMPartActorBase::InitializeFromPartData")
        );
    if (!PartRow)
    {
        UE_LOG(LogChimeraPart, Error,
            TEXT("[Part Data Failed] Part=%s Row=%s was not found."),
            *GetName(),
            *PartRowName.ToString());
        return false;
    }

    const FCMPartTierTableRow* TierRow =
        LoadedTierTable->FindRow<FCMPartTierTableRow>(
            TierRowName,
            TEXT("ACMPartActorBase::InitializeFromPartData")
        );
    if (!TierRow)
    {
        UE_LOG(LogChimeraPart, Error,
            TEXT("[Part Data Failed] Part=%s TierRow=%s was not found."),
            *GetName(),
            *TierRowName.ToString());
        return false;
    }

    const FName ExpectedType = PartType == ECMPartSlotType::Arm
        ? FName(TEXT("Arm"))
        : PartType == ECMPartSlotType::Leg
            ? FName(TEXT("Leg"))
            : NAME_None;
    if (!ExpectedType.IsNone() && PartRow->PartType != ExpectedType)
    {
        UE_LOG(LogChimeraPart, Error,
            TEXT("[Part Data Failed] Part=%s Row=%s expected Type=%s but received %s."),
            *GetName(),
            *PartRowName.ToString(),
            *ExpectedType.ToString(),
            *PartRow->PartType.ToString());
        return false;
    }

    PartDataID = PartRow->ID;
    Species = PartRow->Species;
    MaxHealth = FMath::Max(
        PartRow->MaxHealth * TierRow->HealthMultiplier,
        1.0f
    );
    Strength = FMath::Max(
        PartRow->Strength * TierRow->StrengthMultiplier,
        0.0f
    );
    BaseMovementImpulse = FMath::Max(
        PartRow->BaseMovementImpulse,
        0.0f
    );
    MovementImpulseMultiplier = FMath::Max(
        TierRow->MovementImpulseMultiplier,
        0.0f
    );
    ApplyPartData(*PartRow);

    UE_LOG(LogChimeraPart, Log,
        TEXT("[Part Data Ready] Part=%s Row=%s Tier=%s ID=%s TierID=%s Type=%s Species=%s Health=%.1f Strength=%.1f BaseImpulse=%.1f MoveScale=%.2f FinalImpulse=%.1f"),
        *GetName(),
        *PartRowName.ToString(),
        *TierRowName.ToString(),
        *PartDataID.ToString(),
        *TierRow->ID.ToString(),
        *PartRow->PartType.ToString(),
        *Species.ToString(),
        MaxHealth,
        Strength,
        BaseMovementImpulse,
        MovementImpulseMultiplier,
        GetMovementImpulse());
    return true;
}

bool ACMPartActorBase::IsAlive() const
{
    return !bDead && Health > 0.0f;
}

bool ACMPartActorBase::IsDisabled() const
{
    return bDisabled;
}

bool ACMPartActorBase::IsAttached() const
{
    return GetAttachedPartSlot() != nullptr;
}

bool ACMPartActorBase::IsOperational() const
{
    return IsAlive()
        && !bDisabled
        && IsAttached()
        && (!PartStatusComponent
            || !PartStatusComponent->BlocksAbility());
}

bool ACMPartActorBase::ApplyPartDamage(float Damage)
{
    const FVector HitLocation = PartMesh
        ? PartMesh->Bounds.Origin
        : GetActorLocation();
    return ApplyPartDamageAtHit(
        Damage,
        HitLocation,
        FVector::UpVector,
        FVector::UpVector);
}

bool ACMPartActorBase::ApplyPartDamageAtHit(
    float Damage,
    const FVector HitLocation,
    const FVector SurfaceNormal,
    const FVector BloodDirection)
{
    if (!HasAuthority() || !IsAlive() || Damage <= 0.0f)
    {
        return false;
    }

    const float PreviousHealth = Health;
    Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
    // The last durability point is not playable health; crossing into it
    // destroys the Part and publishes a single final value of zero.
    const bool bDiedNow = Health <= 1.0f;
    if (bDiedNow)
    {
        Health = 0.0f;
    }
    OnHealthChanged.Broadcast(PreviousHealth, Health, MaxHealth);

    if (GoreResponseComponent)
    {
        GoreResponseComponent->SpawnHitEffects(
            HitLocation,
            SurfaceNormal,
            BloodDirection);
    }

    if (bDiedNow)
    {
        bDead = true;
        bDisabled = true;
        if (PartStatusComponent)
        {
            PartStatusComponent->ClearAllStatuses();
        }
        BattleComponent->EndParryWindow();
        OnPartDied.Broadcast();
        OnDisabledChanged.Broadcast(true);

        if (GoreResponseComponent)
        {
            GoreResponseComponent->SpawnDestructionEffects(
                HitLocation,
                BloodDirection);
        }

        if (UCMPartSlotComponent* PartSlot = GetAttachedPartSlot())
        {
            PartSlot->DetachPart();
        }
        ApplyDestroyedState();
    }

    ForceNetUpdate();

    UE_LOG(LogChimeraPart, Log,
        TEXT("[Part Damage] Part=%s Damage=%.1f Health=%.1f->%.1f Dead=%s"),
        *GetName(),
        Damage,
        PreviousHealth,
        Health,
        bDiedNow ? TEXT("true") : TEXT("false"));
    return bDiedNow;
}

void ACMPartActorBase::ApplyDestroyedState()
{
    if (PartMesh)
    {
        PartMesh->SetVisibility(false, true);
        PartMesh->SetHiddenInGame(true, true);
        PartMesh->SetSimulatePhysics(false);
        PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (DamageHurtbox)
    {
        DamageHurtbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void ACMPartActorBase::CaptureMountedPhysicsState()
{
    if (bMountedPhysicsStateCaptured || !PartMesh || !DamageHurtbox)
    {
        return;
    }

    MountedMeshCollisionProfile = PartMesh->GetCollisionProfileName();
    MountedMeshCollisionEnabled = PartMesh->GetCollisionEnabled();
    MountedHurtboxCollisionEnabled = DamageHurtbox->GetCollisionEnabled();
    bMountedMeshGenerateOverlapEvents =
        PartMesh->GetGenerateOverlapEvents();
    MountedMeshRelativeTransform = PartMesh->GetRelativeTransform();
    bMountedPhysicsStateCaptured = true;
}

void ACMPartActorBase::ApplyAttachmentPhysicsState()
{
    if (!bMountedPhysicsStateCaptured || !PartMesh || !DamageHurtbox)
    {
        return;
    }
    if (bDead)
    {
        ApplyDestroyedState();
        return;
    }

    const bool bMounted =
        CMControl::IsValidPartSlot(AttachedSlotAddress);

    if (bMounted)
    {
        PrepareForPartSlotAttachment();
        return;
    }

    PartMesh->SetAllBodiesSimulatePhysics(false);
    PartMesh->SetSimulatePhysics(false);
    PartMesh->SetPhysicsBlendWeight(0.0f);

    // A loose usable Part is represented only by its authored Physics Asset.
    DamageHurtbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PartMesh->SetCollisionProfileName(TEXT("Ragdoll"));
    PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PartMesh->SetGenerateOverlapEvents(true);
    if (!bTentaclePullActive)
    {
        PartMesh->SetAllBodiesSimulatePhysics(true);
        PartMesh->SetSimulatePhysics(true);
        PartMesh->SetAllBodiesPhysicsBlendWeight(1.0f, false);
        PartMesh->WakeAllRigidBodies();
    }
}

void ACMPartActorBase::SetPartDisabled(bool bNewDisabled)
{
    if (!HasAuthority() || (bDead && !bNewDisabled)
        || bDisabled == bNewDisabled)
    {
        return;
    }

    bDisabled = bNewDisabled;
    if (bDisabled && BattleComponent)
    {
        BattleComponent->EndParryWindow();
    }
    OnDisabledChanged.Broadcast(bDisabled);
    ForceNetUpdate();
}

void ACMPartActorBase::OnRep_MaxHealth()
{
    OnHealthChanged.Broadcast(Health, Health, MaxHealth);
}

void ACMPartActorBase::OnRep_Health(float PreviousHealth)
{
    if (!FMath::IsNearlyEqual(PreviousHealth, Health))
    {
        OnHealthChanged.Broadcast(PreviousHealth, Health, MaxHealth);
    }
}

void ACMPartActorBase::OnRep_Dead()
{
    if (bDead)
    {
        ApplyDestroyedState();
        OnPartDied.Broadcast();
    }
}

void ACMPartActorBase::OnRep_Disabled()
{
    OnDisabledChanged.Broadcast(bDisabled);
}

void ACMPartActorBase::OnRep_AttachmentPhysicsState()
{
    CaptureMountedPhysicsState();
    ApplyAttachmentPhysicsState();
}

void ACMPartActorBase::SetContributingPlayerState(
    ACMPlayerState* PlayerState
)
{
    if (HasAuthority())
    {
        PendingContributingPlayerState = PlayerState;
    }
}

ACMPlayerState* ACMPartActorBase::ConsumeContributingPlayerState()
{
    ACMPlayerState* PlayerState = PendingContributingPlayerState.Get();
    PendingContributingPlayerState.Reset();
    return PlayerState;
}

bool ACMPartActorBase::TryReserveForTentacle(AActor* Requester)
{
    if (!HasAuthority() || !IsValid(Requester) || GetAttachedPartSlot())
    {
        return false;
    }
    if (TentacleReservationOwner.IsValid()
        && TentacleReservationOwner.Get() != Requester)
    {
        return false;
    }
    TentacleReservationOwner = Requester;
    bTentaclePullActive = true;
    ApplyAttachmentPhysicsState();
    ForceNetUpdate();
    return true;
}

void ACMPartActorBase::ReleaseTentacleReservation(AActor* Requester)
{
    if (HasAuthority() && TentacleReservationOwner.Get() == Requester)
    {
        TentacleReservationOwner.Reset();
        bTentaclePullActive = false;
        ApplyAttachmentPhysicsState();
        ForceNetUpdate();
    }
}

bool ACMPartActorBase::IsReservedForTentacle(
    const AActor* Requester) const
{
    return TentacleReservationOwner.IsValid()
        && TentacleReservationOwner.Get() != Requester;
}

bool ACMPartActorBase::IsReservedByTentacle(
    const AActor* Requester) const
{
    return IsValid(Requester)
        && TentacleReservationOwner.Get() == Requester;
}
