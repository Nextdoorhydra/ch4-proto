#include "Player/CMChimera.h"

#include "Ability/CMChimeraAttributeSet.h"
#include "Ability/CMStaminaGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "Data/Body/CMBodyTableRow.h"
#include "Engine/DataTable.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

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
    MaxSpeed = BodyRow->MaxVelocity;
    // A legacy DT that has not been reimported yet contains zero for the new
    // column. Keep the C++ default so opening PIE before reimport does not
    // silently disable every movement Part.
    if (BodyRow->BaseMovementImpulse > 0.0f)
    {
        BaseMovementImpulse = BodyRow->BaseMovementImpulse;
    }
    else
    {
        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[CSV -> Physics] Row=%s has no valid BaseMovementImpulse; keeping fallback %.1f. Reimport the Body table."),
            *BodyRowName.ToString(),
            BaseMovementImpulse);
    }

    RuntimeBodyPhysicalMaterial = NewObject<UPhysicalMaterial>(this);
    RuntimeBodyPhysicalMaterial->Friction = BodyGroundFriction;

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[CSV -> Physics] Row=%s Type=%s Mass=%.1f Friction=%.2f LinearDamping=%.2f AngularDamping=%.2f MaxVelocity=%.1f BaseImpulse=%.1f"),
        *BodyRowName.ToString(),
        *BodyRow->BodyType.ToString(),
        BodySegmentMass,
        BodyGroundFriction,
        BodyLinearDamping,
        BodyAngularDamping,
        MaxSpeed,
        BaseMovementImpulse);

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
        || !AttributeSet)
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

    AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
        *RegenSpec.Data.Get()
    );

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[GAS Stamina] Periodic regeneration started. AmountPerSecond=%.1f"),
        AttributeSet->GetStaminaRegen());
}
