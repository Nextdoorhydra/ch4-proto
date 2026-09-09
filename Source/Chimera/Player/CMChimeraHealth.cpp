#include "Player/CMChimera.h"

#include "Components/BoxComponent.h"
#include "Gore/CMGoreResponseComponent.h"
#include "Player/CMControlBody.h"
#include "EngineUtils.h"

void ACMChimera::ApplyDamageToSegment(
    int32 SegmentIndex,
    float Damage
)
{
    const UBoxComponent* SegmentBody = BodySegments.IsValidIndex(SegmentIndex)
        ? BodySegments[SegmentIndex]
        : nullptr;
    const FVector HitLocation = SegmentBody
        ? SegmentBody->Bounds.Origin
        : GetActorLocation();
    ApplyDamageToSegmentAtHit(
        SegmentIndex,
        Damage,
        HitLocation,
        FVector::UpVector,
        FVector::UpVector);
}

void ACMChimera::ApplyDamageToSegmentAtHit(
    int32 SegmentIndex,
    float Damage,
    const FVector HitLocation,
    const FVector SurfaceNormal,
    const FVector BloodDirection
)
{
    if (!HasAuthority()
        || !CanBeDamaged()
        || !SegmentHealthStates.IsValidIndex(SegmentIndex)
        || Damage <= 0.0f)
    {
        UE_LOG(LogChimeraLineBody, Verbose,
            TEXT("[Segment Damage Rejected] Authority=%s Segment=%d Damage=%.1f"),
            HasAuthority() ? TEXT("true") : TEXT("false"),
            SegmentIndex,
            Damage);
        return;
    }

    FCMBodySegmentHealthState& SegmentState =
        SegmentHealthStates[SegmentIndex];
    if (SegmentState.bDead)
    {
        UE_LOG(LogChimeraLineBody, Verbose,
            TEXT("[Segment Damage Ignored] Segment=%d is already dead."),
            SegmentIndex);
        return;
    }

    const float OldHealth = SegmentState.Health;
    SegmentState.Health = FMath::Max(
        SegmentState.Health - Damage,
        0.0f
    );
    SegmentState.bDead = SegmentState.Health <= 0.0f;
    OnSegmentStatesChanged.Broadcast();

    if (GoreResponseComponent)
    {
        GoreResponseComponent->SpawnHitEffects(
            HitLocation,
            SurfaceNormal,
            BloodDirection);
    }

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[Segment Damage] Segment=%d Damage=%.1f Health=%.1f -> %.1f"),
        SegmentIndex, Damage, OldHealth, SegmentState.Health);

    if (SegmentState.bDead)
    {
        if (GoreResponseComponent)
        {
            GoreResponseComponent->SpawnDestructionEffects(
                HitLocation,
                BloodDirection);
        }

        for (int32 PartSlotIndex = 0;
            PartSlotIndex < CMControl::PartSlotsPerSegment;
            ++PartSlotIndex)
        {
            FCMPartSlotAddress PartSlotAddress;
            PartSlotAddress.SegmentIndex = SegmentIndex;
            PartSlotAddress.PartSlotIndex = PartSlotIndex;
            DetachPartFromSlot(PartSlotAddress);
        }

        // Every player sharing this Segment loses only the controls mapped to
        // its left or right PartSlot.
        for (TActorIterator<ACMControlBody> It(GetWorld()); It; ++It)
        {
            It->HandleSegmentDestroyed(SegmentIndex);
        }

        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Segment Death] Body=%d SegmentIndex=%d died; attached Parts were detached and assigned left/right PartSlot controls were locked for all joint owners."),
            SegmentIndex + 1,
            SegmentIndex);

        OnSegmentDestroyed.Broadcast(SegmentIndex);

        if (!bAllSegmentsDeathNotified && AreAllSegmentsDead())
        {
            bAllSegmentsDeathNotified = true;
            ClearPressedControlParts();

            UE_LOG(LogChimeraLineBody, Error,
                TEXT("[All Segments Dead] Every active segment is dead. Broadcasting OnAllSegmentsDead once on the server."));
            OnAllSegmentsDead.Broadcast();
        }
    }

    ForceNetUpdate();
}

bool ACMChimera::RestoreSegmentToFullHealth(int32 SegmentIndex)
{
    if (!HasAuthority()
        || !SegmentHealthStates.IsValidIndex(SegmentIndex))
    {
        return false;
    }

    FCMBodySegmentHealthState& SegmentState =
        SegmentHealthStates[SegmentIndex];
    if (SegmentState.bDead || SegmentState.MaxHealth <= 0.0f)
    {
        return false;
    }

    const float PreviousHealth = SegmentState.Health;
    SegmentState.Health = SegmentState.MaxHealth;
    OnSegmentStatesChanged.Broadcast();
    ForceNetUpdate();

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[Segment Full Heal] Segment=%d Health=%.1f->%.1f"),
        SegmentIndex,
        PreviousHealth,
        SegmentState.Health);
    return true;
}

bool ACMChimera::IsSegmentAlive(int32 SegmentIndex) const
{
    return SegmentHealthStates.IsValidIndex(SegmentIndex)
        && !SegmentHealthStates[SegmentIndex].bDead;
}

UBoxComponent* ACMChimera::GetBodySegmentComponent(
    int32 SegmentIndex
) const
{
    return BodySegments.IsValidIndex(SegmentIndex)
        ? BodySegments[SegmentIndex]
        : nullptr;
}

int32 ACMChimera::GetSegmentIndexFromHurtbox(
    const UPrimitiveComponent* HitComponent
) const
{
    for (int32 SegmentIndex = 0;
        SegmentIndex < SegmentHurtboxes.Num();
        ++SegmentIndex)
    {
        if (SegmentHurtboxes[SegmentIndex] == HitComponent)
        {
            return SegmentIndex;
        }
    }
    return INDEX_NONE;
}

int32 ACMChimera::GetSegmentIndexFromDamageComponent(
    const UPrimitiveComponent* HitComponent
) const
{
    const int32 HurtboxIndex = GetSegmentIndexFromHurtbox(HitComponent);
    if (HurtboxIndex != INDEX_NONE)
    {
        return HurtboxIndex;
    }

    for (int32 SegmentIndex = 0;
        SegmentIndex < BodySegments.Num();
        ++SegmentIndex)
    {
        if (BodySegments[SegmentIndex] == HitComponent)
        {
            return SegmentIndex;
        }
    }
    return INDEX_NONE;
}

UBoxComponent* ACMChimera::GetSegmentHurtbox(int32 SegmentIndex) const
{
    return SegmentHurtboxes.IsValidIndex(SegmentIndex)
        ? SegmentHurtboxes[SegmentIndex]
        : nullptr;
}

TArray<FCMBodySegmentHealthState> ACMChimera::GetSegmentHealthStates() const
{
    return SegmentHealthStates;
}

bool ACMChimera::AreAllSegmentsDead() const
{
    if (SegmentHealthStates.IsEmpty())
    {
        return false;
    }

    for (const FCMBodySegmentHealthState& SegmentState
        : SegmentHealthStates)
    {
        if (!SegmentState.bDead)
        {
            return false;
        }
    }

    return true;
}

void ACMChimera::RestoreForCheckpointRespawn()
{
    if (!HasAuthority())
    {
        return;
    }

    ClearPressedControlParts();
    for (FCMBodySegmentHealthState& SegmentState : SegmentHealthStates)
    {
        SegmentState.Health = SegmentState.MaxHealth;
        SegmentState.bDead = false;
    }
    bAllSegmentsDeathNotified = false;
    OnSegmentStatesChanged.Broadcast();

    for (TActorIterator<ACMControlBody> It(GetWorld()); It; ++It)
    {
        It->RestoreControlsAfterRespawn();
    }
    ForceNetUpdate();
}

void ACMChimera::InitializeSegmentHealth(float SegmentMaxHealth)
{
    if (!HasAuthority())
    {
        return;
    }

    bAllSegmentsDeathNotified = false;
    ConfiguredSegmentMaxHealth = SegmentMaxHealth;

    const int32 SegmentCount = FMath::Clamp(
        ActiveSegmentCount,
        1,
        BodySegments.Num()
    );
    SegmentHealthStates.SetNum(SegmentCount);

    for (int32 SegmentIndex = 0;
        SegmentIndex < SegmentCount;
        ++SegmentIndex)
    {
        FCMBodySegmentHealthState& SegmentState =
            SegmentHealthStates[SegmentIndex];
        SegmentState.SegmentIndex = SegmentIndex;
        SegmentState.Health = SegmentMaxHealth;
        SegmentState.MaxHealth = SegmentMaxHealth;
        SegmentState.bDead = false;
    }

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[CSV -> Segments] Initialized %d segments with %.1f HP each."),
        SegmentCount, SegmentMaxHealth);

    OnSegmentStatesChanged.Broadcast();
    ForceNetUpdate();
}

void ACMChimera::OnRep_SegmentHealthStates()
{
    OnSegmentStatesChanged.Broadcast();

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[Client Replication] Received %d segment health states."),
        SegmentHealthStates.Num());

    for (const FCMBodySegmentHealthState& SegmentState : SegmentHealthStates)
    {
        UE_LOG(LogChimeraLineBody, Verbose,
            TEXT("[Client Segment] Index=%d Health=%.1f/%.1f Dead=%s"),
            SegmentState.SegmentIndex,
            SegmentState.Health,
            SegmentState.MaxHealth,
            SegmentState.bDead ? TEXT("true") : TEXT("false"));
    }
}
