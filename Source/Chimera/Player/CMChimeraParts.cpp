#include "Player/CMChimera.h"

#include "Ability/CMChimeraAttributeSet.h"
#include "Movement/CMLineBodyMovementCoordinator.h"
#include "Player/CMControlBody.h"
#include "Player/CMDebugPartActor.h"
#include "Player/CMPartSlotComponent.h"
#include "Player/CMPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void ACMChimera::ActivatePartSlot(
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState
)
{
    if (!HasAuthority()
        || !CMControl::IsValidPartSlot(
            PartSlotAddress,
            ActiveSegmentCount))
    {
        return;
    }

    UCMPartSlotComponent* PartSlot =
        GetPartSlotComponent(PartSlotAddress);
    if (PartSlot && PartSlot->HasAttachedPart())
    {
#if !UE_BUILD_SHIPPING
        ACMDebugPartActor* DebugPart =
            Cast<ACMDebugPartActor>(PartSlot->GetAttachedPart());
        if (DebugPart)
        {
            DebugPart->SetContributingPlayerState(
                ContributingPlayerState
            );
        }
#endif

        const bool bActivated = PartSlot->TryActivateGrantedAbility();
#if !UE_BUILD_SHIPPING
        if (DebugPart && !bActivated)
        {
            DebugPart->ConsumeContributingPlayerState();
        }
#endif
        UE_LOG(LogChimeraLineBody, Log,
            TEXT("[Attached Part Input] Slot=(%d,%d) Part=%s Activated=%s"),
            PartSlotAddress.SegmentIndex,
            PartSlotAddress.PartSlotIndex,
            *GetNameSafe(PartSlot->GetAttachedPart()),
            bActivated ? TEXT("true") : TEXT("false"));
        return;
    }

    UE_LOG(LogChimeraLineBody, Verbose,
        TEXT("[Empty PartSlot] PartSlot=(%d,%d) has no attached Part action."),
        PartSlotAddress.SegmentIndex,
        PartSlotAddress.PartSlotIndex);
}

#if !UE_BUILD_SHIPPING
void ACMChimera::ActivateDebugLegPart(
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState
)
{
    if (!HasAuthority()
        || !CMControl::IsValidPartSlot(
            PartSlotAddress,
            ActiveSegmentCount))
    {
        return;
    }

    const int32 SegmentIndex = PartSlotAddress.SegmentIndex;
    UCMPartSlotComponent* PartSlot =
        GetPartSlotComponent(PartSlotAddress);
    if (!PartSlot)
    {
        return;
    }

    const float SafeStaminaCost = FMath::Max(LegStaminaCost, 0.0f);
    if (AttributeSet->GetStamina() < SafeStaminaCost)
    {
        UE_LOG(LogChimeraLineBody, Log,
            TEXT("[Input Rejected] Not enough shared Stamina. Required=%.1f Current=%.1f PartSlot=(%d,%d)"),
            SafeStaminaCost,
            AttributeSet->GetStamina(),
            PartSlotAddress.SegmentIndex,
            PartSlotAddress.PartSlotIndex);
        return;
    }

    const bool bActionSucceeded = MovementCoordinator
        && MovementCoordinator->TryActivateLeg(
            *this,
            SegmentIndex,
            PartSlot,
            ContributingPlayerState
        );
    if (bActionSucceeded)
    {
        ApplyStaminaCost(SafeStaminaCost);
    }
}
#endif

UCMPartSlotComponent* ACMChimera::GetPartSlotComponent(
    const FCMPartSlotAddress& PartSlotAddress
) const
{
    if (!CMControl::IsValidPartSlot(
        PartSlotAddress,
        ActiveSegmentCount))
    {
        return nullptr;
    }

    const int32 FlatIndex =
        CMControl::ToFlatPartSlotIndex(PartSlotAddress);
    return PartSlotPoints.IsValidIndex(FlatIndex)
        ? PartSlotPoints[FlatIndex]
        : nullptr;
}

bool ACMChimera::AttachPartToSlot(
    const FCMPartSlotAddress& PartSlotAddress,
    AActor* PartActor
)
{
    if (!HasAuthority())
    {
        return false;
    }

    UCMPartSlotComponent* PartSlot =
        GetPartSlotComponent(PartSlotAddress);
    return PartSlot && PartSlot->AttachPart(PartActor);
}

AActor* ACMChimera::DetachPartFromSlot(
    const FCMPartSlotAddress& PartSlotAddress
)
{
    if (!HasAuthority())
    {
        return nullptr;
    }

    UCMPartSlotComponent* PartSlot =
        GetPartSlotComponent(PartSlotAddress);
    return PartSlot ? PartSlot->DetachPart() : nullptr;
}

#if !UE_BUILD_SHIPPING
void ACMChimera::SpawnRandomDebugParts()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    ClearRandomDebugParts();

    static const ECMPartSlotType DebugPartTypes[] =
    {
        ECMPartSlotType::Head,
        ECMPartSlotType::Arm,
        ECMPartSlotType::Leg
    };

    int32 AttachedCount = 0;
    const int32 ActiveSlotCount =
        ActiveSegmentCount * CMControl::PartSlotsPerSegment;
    for (int32 FlatIndex = 0; FlatIndex < ActiveSlotCount; ++FlatIndex)
    {
        if (!PartSlotPoints.IsValidIndex(FlatIndex)
            || !PartSlotPoints[FlatIndex]
            || PartSlotPoints[FlatIndex]->HasAttachedPart())
        {
            continue;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ACMDebugPartActor* DebugPart = GetWorld()->SpawnActor<
            ACMDebugPartActor>(
                GetActorLocation(),
                FRotator::ZeroRotator,
                SpawnParameters
            );
        if (!DebugPart)
        {
            continue;
        }

        DebugPart->InitializeDebugPart(
            DebugPartTypes[FMath::RandHelper(UE_ARRAY_COUNT(DebugPartTypes))]
        );
        if (PartSlotPoints[FlatIndex]->AttachPart(DebugPart))
        {
            ++AttachedCount;
        }
        else
        {
            DebugPart->Destroy();
        }
    }

    UE_LOG(LogChimeraLineBody, Warning,
        TEXT("[Debug Parts Ready] Attached %d random Head/Arm/Leg Parts. Press Q/W/E/R to verify slot-to-GA routing."),
        AttachedCount);
}

void ACMChimera::ClearRandomDebugParts()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    TArray<ACMDebugPartActor*> DebugParts;
    for (TActorIterator<ACMDebugPartActor> It(GetWorld()); It; ++It)
    {
        DebugParts.Add(*It);
    }

    int32 RemovedCount = 0;
    for (ACMDebugPartActor* DebugPart : DebugParts)
    {
        if (!IsValid(DebugPart))
        {
            continue;
        }

        for (UCMPartSlotComponent* PartSlot : PartSlotPoints)
        {
            if (PartSlot && PartSlot->GetAttachedPart() == DebugPart)
            {
                PartSlot->DetachPart();
                break;
            }
        }

        DebugPart->Destroy();
        ++RemovedCount;
    }

    UE_LOG(LogChimeraLineBody, Warning,
        TEXT("[Debug Parts Cleared] Removed %d diagnostic Parts."),
        RemovedCount);
}
#endif

void ACMChimera::SetPartSlotPressed(
    const FCMPartSlotAddress& PartSlotAddress,
    bool bPressed
)
{
    if (!HasAuthority()
        || !CMControl::IsValidPartSlot(
            PartSlotAddress,
            ActiveSegmentCount))
    {
        return;
    }

    const uint32 PartSlotBit = 1u
        << CMControl::ToFlatPartSlotIndex(PartSlotAddress);
    if (bPressed)
    {
        PressedPartSlotMask |= PartSlotBit;
    }
    else
    {
        PressedPartSlotMask &= ~PartSlotBit;
    }

    ForceNetUpdate();
}

void ACMChimera::ClearPressedControlParts()
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bHadPressedPart = PressedPartSlotMask != 0;
    PressedPartSlotMask = 0;

    for (TActorIterator<ACMControlBody> It(GetWorld()); It; ++It)
    {
        It->ClearPressedControlSlots();
    }

    if (bHadPressedPart)
    {
        ForceNetUpdate();
    }
}
