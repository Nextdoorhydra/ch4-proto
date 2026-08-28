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

    if (HasAuthority())
    {
        MaxHealth = FMath::Max(MaxHealth, 1.0f);
        Health = MaxHealth;
        bDead = false;
        bDisabled = false;
        ForceNetUpdate();
    }
}

void ACMPartActorBase::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMPartActorBase, AttachedSlotAddress);
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
    if (!HasAuthority() || !PartSlot)
    {
        return;
    }

    AttachedPartSlot = PartSlot;
    AttachedSlotAddress = PartSlot->GetSlotAddress();
    ForceNetUpdate();
}

void ACMPartActorBase::OnDetachedFromPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    if (!HasAuthority())
    {
        return;
    }

    AttachedPartSlot.Reset();
    PendingContributingPlayerState.Reset();
    if (PartStatusComponent)
    {
        PartStatusComponent->ClearAllStatuses();
    }
    if (BattleComponent)
    {
        BattleComponent->EndParryWindow();
    }
    AttachedSlotAddress = FCMPartSlotAddress();
    ForceNetUpdate();
}

UCMPartSlotComponent* ACMPartActorBase::GetAttachedPartSlot() const
{
    if (UCMPartSlotComponent* PartSlot = AttachedPartSlot.Get())
    {
        return PartSlot;
    }

    return SceneRoot
        ? Cast<UCMPartSlotComponent>(SceneRoot->GetAttachParent())
        : nullptr;
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
    if (!HasAuthority() || !IsAlive() || Damage <= 0.0f)
    {
        return false;
    }

    const float PreviousHealth = Health;
    Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
    OnHealthChanged.Broadcast(PreviousHealth, Health, MaxHealth);

    const bool bDiedNow = Health <= 0.0f;
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
        OnPartDied.Broadcast();
    }
}

void ACMPartActorBase::OnRep_Disabled()
{
    OnDisabledChanged.Broadcast(bDisabled);
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
