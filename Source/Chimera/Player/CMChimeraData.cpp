#include "Player/CMChimera.h"

#include "Ability/CMChimeraAttributeSet.h"
#include "Ability/CMStaminaGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "Data/Body/CMBodyTableRow.h"
#include "Engine/DataTable.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Parts/Head/CMVisionComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Player/CMPartSlotComponent.h"

float ACMChimera::GetStamina() const
{
    return AttributeSet ? AttributeSet->GetStamina() : 0.0f;
}

float ACMChimera::GetMaxStamina() const
{
    return AttributeSet ? AttributeSet->GetMaxStamina() : 0.0f;
}

void ACMChimera::GetActiveHeadVisionSources(
    TArray<UCMVisionComponent*>& OutVisionSources) const
{
    OutVisionSources.Reset();
    for (const UCMPartSlotComponent* PartSlot : PartSlotPoints)
    {
        ACMHeadPartActor* Head = PartSlot
            ? Cast<ACMHeadPartActor>(PartSlot->GetAttachedPart())
            : nullptr;
        UCMVisionComponent* HeadVision = Head
            ? Head->GetVisionComponent() : nullptr;
        if (HeadVision && HeadVision->IsVisionActiveWithoutStatus())
        {
            OutVisionSources.Add(HeadVision);
        }
    }
}

bool ACMChimera::InitializeFromBodyData()
{
    UE_LOG(LogChimeraLineBody, Verbose,
        TEXT("[Data Load] Loading table=%s row=%s"),
        *BodyDataTable.ToSoftObjectPath().ToString(),
        *BodyRowName.ToString());

    UDataTable* LoadedBodyDataTable = BodyDataTable.LoadSynchronous();
    if (!LoadedBodyDataTable)
    {
        UE_LOG(LogChimeraLineBody, Error,
            TEXT("[Data Load Failed] Body DataTable could not be loaded: %s"),
            *BodyDataTable.ToSoftObjectPath().ToString());
        return false;
    }

    const FCMBodyTableRow* BodyRow =
        LoadedBodyDataTable->FindRow<FCMBodyTableRow>(
            BodyRowName,
            TEXT("ACMChimera::InitializeFromBodyData")
        );
    if (!BodyRow)
    {
        UE_LOG(LogChimeraLineBody, Error,
            TEXT("[Data Load Failed] Body row was not found: %s"),
            *BodyRowName.ToString());
        return false;
    }

    if (BodyRow->BodyType != TEXT("Line"))
    {
        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Layout Mismatch] ACMChimera uses the Line layout, but row %s has BodyType %s."),
            *BodyRowName.ToString(),
            *BodyRow->BodyType.ToString());
    }

    // Runtime tuning values are copied first, then ConfigureSegments applies
    // them to the actual Chaos components during BeginPlay.
    BodySegmentMass = BodyRow->SegmentMass;
    BodyGroundFriction = BodyRow->GroundFriction;
    BodyLinearDamping = BodyRow->LinearDamping;
    BodyAngularDamping = BodyRow->AngularDamping;
    MaxSpeed = CMChimeraPhysics::ResolveLinearSpeedLimit(
        BodyRow->MaxVelocity,
        MaximumSafeLinearSpeed);
    if (!FMath::IsNearlyEqual(MaxSpeed, BodyRow->MaxVelocity))
    {
        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Physics Safety] MaxVelocity %.1f was clamped to %.1f cm/s."),
            BodyRow->MaxVelocity,
            MaxSpeed);
    }
    PlayerCountForceMultiplier =
        FMath::Max(BodyRow->PlayerCountForceMultiplier, 0.0f);
    LegYawMultiplier = FMath::Max(BodyRow->LegYawMultiplier, 0.0f);
    LegForwardMultiplier =
        FMath::Max(BodyRow->LegForwardMultiplier, 0.0f);

    RuntimeBodyPhysicalMaterial = NewObject<UPhysicalMaterial>(this);
    RuntimeBodyPhysicalMaterial->Friction = BodyGroundFriction;

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[CSV -> Physics] Row=%s Type=%s Mass=%.1f Friction=%.2f LinearDamping=%.2f AngularDamping=%.2f MaxVelocity=%.1f PlayerCountForceMultiplier=%.2f LegYawMultiplier=%.2f LegForwardMultiplier=%.2f"),
        *BodyRowName.ToString(),
        *BodyRow->BodyType.ToString(),
        BodySegmentMass,
        BodyGroundFriction,
        BodyLinearDamping,
        BodyAngularDamping,
        MaxSpeed,
        PlayerCountForceMultiplier,
        LegYawMultiplier,
        LegForwardMultiplier);

    if (HasAuthority())
    {
        InitializeSharedAttributes(
            BodyRow->MaxStamina,
            BodyRow->StaminaRegen
        );
        InitializeSegmentHealth(BodyRow->SegmentMaxHP);

        UE_LOG(LogChimeraLineBody, Log,
            TEXT("[LineBody Ready] Row=%s ID=%s SegmentHP=%.1f SharedStamina=%.1f RegenPerSecond=%.1f Segments=%d"),
            *BodyRowName.ToString(),
            *BodyRow->ID.ToString(),
            BodyRow->SegmentMaxHP,
            BodyRow->MaxStamina,
            BodyRow->StaminaRegen,
            SegmentHealthStates.Num()
        );
    }

    return true;
}
void ACMChimera::InitializeSharedAttributes(
    float MaxStamina,
    float StaminaRegen
)
{
    if (!HasAuthority() || !AbilitySystemComponent || !AttributeSet)
    {
        return;
    }

    // Max must be assigned before Current so PreAttributeChange does not
    // clamp Current against the AttributeSet's initial zero Max value.
    AbilitySystemComponent->SetNumericAttributeBase(
        UCMChimeraAttributeSet::GetMaxStaminaAttribute(), MaxStamina);
    AbilitySystemComponent->SetNumericAttributeBase(
        UCMChimeraAttributeSet::GetStaminaAttribute(), MaxStamina);
    AbilitySystemComponent->SetNumericAttributeBase(
        UCMChimeraAttributeSet::GetStaminaRegenAttribute(), StaminaRegen);

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[CSV -> ASC] Stamina=%.1f/%.1f RegenPerSecond=%.1f"),
        AttributeSet->GetStamina(),
        AttributeSet->GetMaxStamina(),
        AttributeSet->GetStaminaRegen());

    StartStaminaRegeneration();
}

void ACMChimera::StartStaminaRegeneration()
{
    if (!HasAuthority()
        || !AbilitySystemComponent
        || !AttributeSet
        || StaminaRegenEffectHandle.IsValid())
    {
        return;
    }

    const FGameplayEffectSpecHandle RegenSpec =
        AbilitySystemComponent->MakeOutgoingSpec(
            UCMStaminaRegenGameplayEffect::StaticClass(),
            1.0f,
            AbilitySystemComponent->MakeEffectContext()
        );
    if (!RegenSpec.IsValid())
    {
        UE_LOG(LogChimeraLineBody, Error,
            TEXT("[GAS Stamina] Failed to create the periodic regeneration GameplayEffect spec."));
        return;
    }

    StaminaRegenEffectHandle =
        AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
        *RegenSpec.Data.Get()
    );

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[GAS Stamina] Periodic regeneration started. AmountPerSecond=%.1f"),
        AttributeSet->GetStaminaRegen());
}

void ACMChimera::PauseStaminaRegeneration()
{
    if (!HasAuthority()
        || !AbilitySystemComponent
        || !StaminaRegenEffectHandle.IsValid())
    {
        return;
    }

    AbilitySystemComponent->RemoveActiveGameplayEffect(
        StaminaRegenEffectHandle
    );
    StaminaRegenEffectHandle.Invalidate();
}
