#include "Player/CMChimera.h"

#include "Player/CMControlBody.h"
#include "EngineUtils.h"

void ACMChimera::ApplyDamageToSegment(
    int32 SegmentIndex,
    float Damage
)
{
    if (!HasAuthority()
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

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[Segment Damage] Segment=%d Damage=%.1f Health=%.1f -> %.1f"),
        SegmentIndex, Damage, OldHealth, SegmentState.Health);

    if (SegmentState.bDead)
    {
        // Segment ownership is independent from the randomly assigned physical
        // PartSlots. The first owned Segment disables Q/W and the second E/R.
        for (TActorIterator<ACMControlBody> It(GetWorld()); It; ++It)
        {
            It->HandleSegmentDestroyed(SegmentIndex);
        }

        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Segment Death] Segment=%d died; its owner's corresponding Q/W or E/R pair was disabled. Physical PartSlot assignment remains random."),
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

bool ACMChimera::IsSegmentAlive(int32 SegmentIndex) const
{
    return SegmentHealthStates.IsValidIndex(SegmentIndex)
        && !SegmentHealthStates[SegmentIndex].bDead;
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

    ForceNetUpdate();
}

void ACMChimera::OnRep_SegmentHealthStates()
{
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
