#include "Parts/Core/CMPartActorBase.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/Part/CMPartLegArmTableRow.h"
#include "Engine/DataTable.h"
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

    PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PartMesh"));
    PartMesh->SetupAttachment(SceneRoot);

    BattleComponent = CreateDefaultSubobject<UCMBattleComponent>(
        TEXT("BattleComponent")
    );
    PartStatusComponent = CreateDefaultSubobject<UCMPartStatusComponent>(
        TEXT("PartStatusComponent")
    );
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

void ACMPartActorBase::ApplyPartData(
    const FCMPartLegArmTableRow& PartRow
)
{
}

bool ACMPartActorBase::InitializeFromPartData()
{
    if (PartDataTable.IsNull() || PartRowName.IsNone())
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
    MaxHealth = FMath::Max(PartRow->MaxHealth, 1.0f);
    Strength = FMath::Max(PartRow->Strength, 0.0f);
    MovementImpulseMultiplier = FMath::Max(
        PartRow->MovementImpulseMultiplier,
        0.0f
    );
    ApplyPartData(*PartRow);

    UE_LOG(LogChimeraPart, Log,
        TEXT("[Part Data Ready] Part=%s Row=%s ID=%s Type=%s Species=%s Health=%.1f Strength=%.1f MoveScale=%.2f"),
        *GetName(),
        *PartRowName.ToString(),
        *PartDataID.ToString(),
        *PartRow->PartType.ToString(),
        *Species.ToString(),
        MaxHealth,
        Strength,
        MovementImpulseMultiplier);
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
