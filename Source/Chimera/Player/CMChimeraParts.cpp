#include "Player/CMChimera.h"

#include "Movement/CMLineBodyMovementCoordinator.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Arm/CMSpringArmPart.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Parts/Leg/CMLegPart.h"
#include "Player/CMControlBody.h"
#include "Player/CMPartSlotComponent.h"
#include "Parts/Tentacle/CMTentacleSegmentActor.h"
#include "Player/CMPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void ACMChimera::ActivatePartSlot(
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState,
    bool bReverseMovement
)
{
    ActivatePartSlotWithLegStrength(
        PartSlotAddress,
        ContributingPlayerState,
        bReverseMovement,
        1.0f
    );
}

void ACMChimera::ActivatePartSlotWithLegStrength(
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState,
    bool bReverseMovement,
    float LegStrengthMultiplier
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
        ACMPartActorBase* PartActor =
            Cast<ACMPartActorBase>(PartSlot->GetAttachedPart());
        if (PartActor)
        {
            PartActor->SetContributingPlayerState(
                ContributingPlayerState
            );
        }

        ACMLegPart* LegPart = Cast<ACMLegPart>(PartActor);
        if (LegPart)
        {
            LegPart->SetPendingReverseMovement(bReverseMovement);
            LegPart->SetPendingInputStrengthMultiplier(
                LegStrengthMultiplier);
        }

        const bool bActivated = PartSlot->TryActivateGrantedAbility();
        if (PartActor && !bActivated)
        {
            PartActor->ConsumeContributingPlayerState();
        }
        if (LegPart && !bActivated)
        {
            LegPart->ConsumePendingReverseMovement();
            LegPart->ConsumePendingInputStrengthMultiplier();
        }
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

float ACMChimera::GetLegInputStrengthMultiplier(
    const float HoldSeconds
) const
{
    const float MinimumStrength = FMath::Clamp(
        LegInputMinimumStrength,
        0.0f,
        1.0f
    );
    const float TapHoldSeconds = FMath::Max(
        LegInputTapHoldSeconds,
        0.0f
    );
    const float NormalHoldSeconds = FMath::Max(
        LegInputFullStrengthHoldSeconds,
        TapHoldSeconds + UE_SMALL_NUMBER
    );
    const float FullChargeHoldSeconds = FMath::Max(
        LegInputFullChargeHoldSeconds,
        NormalHoldSeconds + UE_SMALL_NUMBER
    );
    const float MaximumChargedStrength = FMath::Clamp(
        LegInputMaximumChargedStrength,
        1.0f,
        2.0f
    );
    const float FullOverchargeHoldSeconds = FullChargeHoldSeconds
        + FMath::Max(
            LegInputOverchargeDurationSeconds,
            UE_SMALL_NUMBER
        );
    const float MaximumOverchargedStrength = FMath::Clamp(
        LegInputMaximumOverchargedStrength,
        MaximumChargedStrength,
        10.0f
    );
    const float SafeHoldSeconds = FMath::Max(HoldSeconds, 0.0f);
    const float StrengthExponent = FMath::Max(
        LegInputStrengthExponent,
        UE_SMALL_NUMBER
    );

    if (SafeHoldSeconds <= TapHoldSeconds)
    {
        return MinimumStrength;
    }

    const auto CalculateEaseOutAlpha = [StrengthExponent](
        const float Alpha)
    {
        const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
        return 1.0f - FMath::Pow(1.0f - ClampedAlpha, StrengthExponent);
    };

    if (SafeHoldSeconds <= NormalHoldSeconds)
    {
        const float NormalAlpha = (SafeHoldSeconds - TapHoldSeconds)
            / (NormalHoldSeconds - TapHoldSeconds);
        return FMath::Lerp(
            MinimumStrength,
            1.0f,
            CalculateEaseOutAlpha(NormalAlpha)
        );
    }

    if (SafeHoldSeconds <= FullChargeHoldSeconds)
    {
        const float ChargeAlpha = (SafeHoldSeconds - NormalHoldSeconds)
            / (FullChargeHoldSeconds - NormalHoldSeconds);
        return FMath::Lerp(
            1.0f,
            MaximumChargedStrength,
            CalculateEaseOutAlpha(ChargeAlpha)
        );
    }

    if (SafeHoldSeconds <= FullOverchargeHoldSeconds)
    {
        const float OverchargeAlpha = FMath::Clamp(
            (SafeHoldSeconds - FullChargeHoldSeconds)
                / (FullOverchargeHoldSeconds - FullChargeHoldSeconds),
            0.0f,
            1.0f
        );
        const float OverchargeEaseInAlpha = FMath::Pow(
            OverchargeAlpha,
            FMath::Max(LegInputOverchargeExponent, 1.0f)
        );
        return FMath::Lerp(
            MaximumChargedStrength,
            MaximumOverchargedStrength,
            OverchargeEaseInAlpha
        );
    }

    return MaximumOverchargedStrength;
}

bool ACMChimera::TryActivateLegPart(
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState,
    bool bReverseMovement
)
{
    return TryActivateLegPartWithStrength(
        PartSlotAddress,
        ContributingPlayerState,
        bReverseMovement,
        1.0f
    );
}

bool ACMChimera::TryActivateLegPartWithStrength(
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState,
    bool bReverseMovement,
    float StrengthMultiplier
)
{
    if (!HasAuthority()
        || !CMControl::IsValidPartSlot(
            PartSlotAddress,
            ActiveSegmentCount))
    {
        return false;
    }

    UCMPartSlotComponent* PartSlot =
        GetPartSlotComponent(PartSlotAddress);
    ACMLegPart* LegPart = PartSlot
        ? Cast<ACMLegPart>(PartSlot->GetAttachedPart())
        : nullptr;
    if (!PartSlot || !LegPart || !LegPart->IsOperational())
    {
        return false;
    }

    return MovementCoordinator
        && MovementCoordinator->TryActivateLeg(
            *this,
            *LegPart,
            ContributingPlayerState,
            LegPart->GetMovementImpulse()
                * FMath::Clamp(StrengthMultiplier, 0.0f, 10.0f),
            bReverseMovement
        );
}

void ACMChimera::CancelLegStep(ACMLegPart* LegPart)
{
    if (HasAuthority() && MovementCoordinator)
    {
        MovementCoordinator->CancelLegStep(LegPart);
    }
}

bool ACMChimera::TryActivateArmPart(
    ACMArmPart* ArmPart,
    ACMPlayerState* ContributingPlayerState
)
{
    if (!HasAuthority() || !ArmPart
        || !ArmPart->IsOperational() || ArmPart->IsSwinging())
    {
        return false;
    }

    UCMPartSlotComponent* PartSlot = ArmPart->GetAttachedPartSlot();
    if (!PartSlot || PartSlot->GetOwner() != this)
    {
        return false;
    }

    if (!ArmPart->BeginSwing())
    {
        return false;
    }

    const FCMPartSlotAddress SlotAddress = PartSlot->GetSlotAddress();
    const bool bMovementApplied = MovementCoordinator
        && MovementCoordinator->TryActivateArm(
            *this,
            SlotAddress.SegmentIndex,
            PartSlot,
            ContributingPlayerState,
            ArmPart->GetMovementImpulse(),
            ArmPart->GetMovementImpulseMultiplier()
        );

    UE_LOG(LogChimeraLineBody, Log,
        TEXT("[Arm Input Accepted] PlayerState=%s Part=%s Duration=%.3f MoveScale=%.2f ImpulseApplied=%s"),
        *GetNameSafe(ContributingPlayerState),
        *GetNameSafe(ArmPart),
        ArmPart->GetSwingDuration(),
        ArmPart->GetMovementImpulseMultiplier(),
        bMovementApplied ? TEXT("true") : TEXT("false"));
    return true;
}

bool ACMChimera::ApplySpringArmPull(
    const FCMPartSlotAddress& PartSlotAddress,
    const FVector& AnchorLocation,
    float PullImpulse,
    float StopDistance
)
{
    if (!HasAuthority()
        || !CMControl::IsValidPartSlot(
            PartSlotAddress,
            ActiveSegmentCount))
    {
        return false;
    }

    return MovementCoordinator
        && MovementCoordinator->ApplyAnchorPull(
            *this,
            PartSlotAddress.SegmentIndex,
            AnchorLocation,
            PullImpulse,
            StopDistance
        );
}

bool ACMChimera::RequestSpringArmPull(ACMSpringArmPart* SpringArm)
{
    if (!HasAuthority() || !IsValid(SpringArm))
    {
        return false;
    }

    ACMSpringArmPart* CurrentPull = ActiveSpringArmPull.Get();

    if (CurrentPull && CurrentPull != SpringArm)
    {
        CurrentPull->CancelBodyPullFromOverride();
    }

    ActiveSpringArmPull = SpringArm;

    UE_LOG(
        LogChimeraLineBody,
        Log,
        TEXT("[SpringArm Pull Owner] New=%s"),
        *GetNameSafe(SpringArm)
    );

    return true;
}

void ACMChimera::ReleaseSpringArmPull(ACMSpringArmPart* SpringArm)
{
    if (!HasAuthority())
    {
        return;
    }

    if (ActiveSpringArmPull.Get() != SpringArm)
    {
        return;
    }

    UE_LOG(
        LogChimeraLineBody,
        Log,
        TEXT("[SpringArm Pull Owner] Released=%s"),
        *GetNameSafe(SpringArm)
    );

    ActiveSpringArmPull.Reset();
}

bool ACMChimera::IsSpringArmPulling() const
{
    return ActiveSpringArmPull.IsValid();
}

void ACMChimera::ActivateDebugLegPart(
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState
)
{
    TryActivateLegPart(
        PartSlotAddress,
        ContributingPlayerState,
        false
    );
}

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

bool ACMChimera::IsPartSlotPressed(
    const FCMPartSlotAddress& PartSlotAddress
) const
{
    if (!CMControl::IsValidPartSlot(
        PartSlotAddress,
        ActiveSegmentCount))
    {
        return false;
    }

    const int32 FlatIndex =
        CMControl::ToFlatPartSlotIndex(PartSlotAddress);
    return (PressedPartSlotMask & (1u << FlatIndex)) != 0;
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

bool ACMChimera::TryBeginTentaclePartAttachment(
    const FCMPartSlotAddress& PartSlotAddress)
{
    if (!HasAuthority()
        || !CMControl::IsValidPartSlot(
            PartSlotAddress, ActiveSegmentCount))
    {
        return false;
    }

    UCMPartSlotComponent* PartSlot =
        GetPartSlotComponent(PartSlotAddress);
    if (!PartSlot || PartSlot->HasAttachedPart())
    {
        return false;
    }

    ACMTentacleSegmentActor* Tentacle =
        TentacleSegments.IsValidIndex(PartSlotAddress.SegmentIndex)
            ? TentacleSegments[PartSlotAddress.SegmentIndex]
            : nullptr;
    return Tentacle
        && Tentacle->TryBeginPartAttachment(PartSlotAddress);
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

namespace
{
const FName CheatSpawnedRandomPartTag(TEXT("CM.CheatSpawnedRandomPart"));
const FName CheatSpawnedLegPartTag(TEXT("CM.CheatSpawnedLegPart"));

UClass* LoadTestLegPartClass()
{
    static TSoftClassPtr<ACMLegPart> TestLegPartClass(
        FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/Leg/BP_CMLegPart.BP_CMLegPart_C")
        )
    );
    return TestLegPartClass.LoadSynchronous();
}

FName GetTestPartTierRowName(int32 Tier)
{
    return Tier >= 1 && Tier <= 5
        ? FName(*FString::Printf(TEXT("Tier%d"), Tier))
        : NAME_None;
}

struct FDebugPartSpawnOption
{
    UClass* PartClass = nullptr;
    FName PartRowName = NAME_None;
    FName TierRowName = NAME_None;
};

void AddTestLegTierPartOptions(
    TArray<FDebugPartSpawnOption>& OutPartOptions
)
{
    UClass* LegPartClass = LoadTestLegPartClass();
    if (!LegPartClass)
    {
        return;
    }

    for (int32 Tier = 1; Tier <= 5; ++Tier)
    {
        OutPartOptions.Add({
            LegPartClass,
            TEXT("DefaultLeg"),
            GetTestPartTierRowName(Tier)
        });
    }
}

UClass* LoadTestArmPartClass()
{
    static TSoftClassPtr<ACMArmPart> TestArmPartClass(
        FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/Arm/BP_CMArmPart.BP_CMArmPart_C")
        )
    );
    return TestArmPartClass.LoadSynchronous();
}

UClass* LoadTestSpringArmPartClass()
{
    static TSoftClassPtr<ACMSpringArmPart> TestSpringArmPartClass(
        FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/Arm/BP_CMSpringArmPart.BP_CMSpringArmPart_C")
        )
    );
    return TestSpringArmPartClass.LoadSynchronous();
}

UClass* LoadTestHeadPartClass()
{
    static TSoftClassPtr<ACMHeadPartActor> TestHeadPartClass(
        FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/Head/BluePrint/BP_CMHead01HeadPart.BP_CMHead01HeadPart_C")
        )
    );
    return TestHeadPartClass.LoadSynchronous();
}

bool ResolveNamedDebugPart(
    FName PartName,
    FDebugPartSpawnOption& OutPartOption
)
{
    if (PartName == TEXT("DefaultArm"))
    {
        OutPartOption.PartClass = LoadTestArmPartClass();
        return OutPartOption.PartClass != nullptr;
    }
    if (PartName == TEXT("SpringArm"))
    {
        OutPartOption.PartClass = LoadTestSpringArmPartClass();
        return OutPartOption.PartClass != nullptr;
    }
    if (PartName == TEXT("DefaultHead"))
    {
        OutPartOption.PartClass = LoadTestHeadPartClass();
        return OutPartOption.PartClass != nullptr;
    }

    const FString PartString = PartName.ToString();
    constexpr TCHAR LegTierPrefix[] = TEXT("LegTier");
    if (PartString.StartsWith(LegTierPrefix, ESearchCase::IgnoreCase))
    {
        OutPartOption.PartRowName = TEXT("DefaultLeg");
        OutPartOption.TierRowName = GetTestPartTierRowName(
            FCString::Atoi(
                *PartString.RightChop(UE_ARRAY_COUNT(LegTierPrefix) - 1))
        );
        OutPartOption.PartClass = OutPartOption.TierRowName.IsNone()
            ? nullptr
            : LoadTestLegPartClass();
        return OutPartOption.PartClass != nullptr;
    }

    return false;
}

enum class ECMStartingPartType : uint8
{
    Empty,
    Leg,
    Head
};

const TArray<ECMStartingPartType>* FindStartingPartLayout(int32 PlayerCount)
{
    static const TArray<ECMStartingPartType> TwoPlayerLayout = {
        ECMStartingPartType::Leg,
        ECMStartingPartType::Empty,
        ECMStartingPartType::Head,
        ECMStartingPartType::Empty
    };
    static const TArray<ECMStartingPartType> ThreePlayerLayout = {
        ECMStartingPartType::Leg,
        ECMStartingPartType::Empty,
        ECMStartingPartType::Head,
        ECMStartingPartType::Empty,
        ECMStartingPartType::Empty,
        ECMStartingPartType::Leg
    };
    static const TArray<ECMStartingPartType> FourPlayerLayout = {
        ECMStartingPartType::Leg,
        ECMStartingPartType::Empty,
        ECMStartingPartType::Leg,
        ECMStartingPartType::Head,
        ECMStartingPartType::Empty,
        ECMStartingPartType::Leg,
        ECMStartingPartType::Head,
        ECMStartingPartType::Leg
    };

    switch (PlayerCount)
    {
    case 2:
        return &TwoPlayerLayout;
    case 3:
        return &ThreePlayerLayout;
    case 4:
        return &FourPlayerLayout;
    default:
        return nullptr;
    }
}
}

void ACMChimera::SpawnStartingPartsForPlayers(int32 PlayerCount)
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const TArray<ECMStartingPartType>* Layout =
        FindStartingPartLayout(PlayerCount);
    if (!Layout || Layout->Num() != ActiveSegmentCount)
    {
        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Starting Parts Skipped] Unsupported or mismatched layout. Players=%d Segments=%d"),
            PlayerCount,
            ActiveSegmentCount);
        return;
    }

    FDebugPartSpawnOption LegOption;
    FDebugPartSpawnOption HeadOption;
    if (!ResolveNamedDebugPart(TEXT("LegTier3"), LegOption)
        || !ResolveNamedDebugPart(TEXT("DefaultHead"), HeadOption))
    {
        UE_LOG(LogChimeraLineBody, Error,
            TEXT("[Starting Parts Failed] Default Leg or Head Blueprint could not be loaded."));
        return;
    }

    for (int32 FlatSlotIndex = 0;
        FlatSlotIndex < ActiveSegmentCount * CMControl::PartSlotsPerSegment;
        ++FlatSlotIndex)
    {
        if (!PartSlotPoints.IsValidIndex(FlatSlotIndex)
            || !PartSlotPoints[FlatSlotIndex])
        {
            continue;
        }

        if (AActor* ExistingPart = PartSlotPoints[FlatSlotIndex]->DetachPart())
        {
            ExistingPart->Destroy();
        }
    }

    int32 AttachedCount = 0;
    for (int32 SegmentIndex = 0;
        SegmentIndex < Layout->Num();
        ++SegmentIndex)
    {
        const ECMStartingPartType PartType = (*Layout)[SegmentIndex];
        if (PartType == ECMStartingPartType::Empty)
        {
            continue;
        }

        const FDebugPartSpawnOption& PartOption =
            PartType == ECMStartingPartType::Leg ? LegOption : HeadOption;
        for (int32 PartSlotIndex = 0;
            PartSlotIndex < CMControl::PartSlotsPerSegment;
            ++PartSlotIndex)
        {
            const int32 FlatSlotIndex =
                SegmentIndex * CMControl::PartSlotsPerSegment
                + PartSlotIndex;
            if (!PartSlotPoints.IsValidIndex(FlatSlotIndex)
                || !PartSlotPoints[FlatSlotIndex])
            {
                continue;
            }

            ACMPartActorBase* PartActor =
                ACMPartActorBase::SpawnPartFromDataRows(
                    this,
                    PartOption.PartClass,
                    PartOption.PartRowName,
                    PartOption.TierRowName,
                    FTransform(FRotator::ZeroRotator, GetActorLocation()),
                    this
                );
            if (!PartActor)
            {
                continue;
            }

            if (PartSlotPoints[FlatSlotIndex]->AttachPart(PartActor))
            {
                ++AttachedCount;
            }
            else
            {
                PartActor->Destroy();
            }
        }
    }

    UE_LOG(LogChimeraLineBody, Display,
        TEXT("[Starting Parts Ready] Players=%d Segments=%d Attached=%d"),
        PlayerCount,
        ActiveSegmentCount,
        AttachedCount);
}

void ACMChimera::SpawnRandomDebugParts()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    ClearRandomDebugParts();

    TArray<FDebugPartSpawnOption> RegisteredPartOptions;
    AddTestLegTierPartOptions(RegisteredPartOptions);
    if (UClass* ArmPartClass = LoadTestArmPartClass())
    {
        RegisteredPartOptions.Add({ ArmPartClass, NAME_None, TEXT("Tier1") });
    }
    if (UClass* SpringArmPartClass = LoadTestSpringArmPartClass())
    {
        RegisteredPartOptions.Add({
            SpringArmPartClass,
            NAME_None,
            TEXT("Tier1")
        });
    }

    if (RegisteredPartOptions.IsEmpty())
    {
        UE_LOG(LogChimeraLineBody, Error,
            TEXT("[Random Parts Failed] No production Part Blueprint could be loaded."));
        return;
    }

    int32 AttachedCount = 0;
    int32 NextPartOptionIndex = FMath::RandHelper(
        RegisteredPartOptions.Num()
    );
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

        const FDebugPartSpawnOption& SelectedPartOption =
            RegisteredPartOptions[NextPartOptionIndex];
        NextPartOptionIndex = (NextPartOptionIndex + 1)
            % RegisteredPartOptions.Num();
        ACMPartActorBase* PartActor =
            ACMPartActorBase::SpawnPartFromDataRows(
                this,
                SelectedPartOption.PartClass,
                SelectedPartOption.PartRowName,
                SelectedPartOption.TierRowName,
                FTransform(FRotator::ZeroRotator, GetActorLocation()),
                this
            );
        if (!PartActor)
        {
            continue;
        }

        PartActor->Tags.AddUnique(CheatSpawnedRandomPartTag);
        if (PartSlotPoints[FlatIndex]->AttachPart(PartActor))
        {
            ++AttachedCount;
        }
        else
        {
            PartActor->Destroy();
        }
    }

    UE_LOG(LogChimeraLineBody, Warning,
        TEXT("[Random Parts Ready] Attached %d production Parts from %d registered option(s)."),
        AttachedCount,
        RegisteredPartOptions.Num());
}

bool ACMChimera::SpawnDebugPartAtSlot(
    int32 FlatSlotIndex,
    FName PartName
)
{
    const int32 ActiveSlotCount =
        ActiveSegmentCount * CMControl::PartSlotsPerSegment;
    if (!HasAuthority()
        || !GetWorld()
        || FlatSlotIndex < 0
        || FlatSlotIndex >= ActiveSlotCount
        || !PartSlotPoints.IsValidIndex(FlatSlotIndex)
        || !PartSlotPoints[FlatSlotIndex])
    {
        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Attach Part Failed] Slot must be between 1 and %d."),
            ActiveSlotCount);
        return false;
    }

    FDebugPartSpawnOption PartOption;
    if (!ResolveNamedDebugPart(PartName, PartOption))
    {
        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Attach Part Failed] Unknown Part=%s. Use DefaultArm, DefaultHead, SpringArm, or LegTier1..5."),
            *PartName.ToString());
        return false;
    }

    UCMPartSlotComponent* PartSlot = PartSlotPoints[FlatSlotIndex];
    if (AActor* ExistingPart = PartSlot->DetachPart())
    {
        ExistingPart->Destroy();
    }

    ACMPartActorBase* PartActor =
        ACMPartActorBase::SpawnPartFromDataRows(
            this,
            PartOption.PartClass,
            PartOption.PartRowName,
            PartOption.TierRowName,
            FTransform(FRotator::ZeroRotator, GetActorLocation()),
            this
        );
    if (!PartActor)
    {
        return false;
    }

    PartActor->Tags.AddUnique(CheatSpawnedRandomPartTag);
    if (!PartSlot->AttachPart(PartActor))
    {
        PartActor->Destroy();
        return false;
    }

    const FCMPartSlotAddress SlotAddress = PartSlot->GetSlotAddress();
    UE_LOG(LogChimeraLineBody, Warning,
        TEXT("[Attach Part Ready] Slot=%d Address=(%d,%d) Part=%s Class=%s"),
        FlatSlotIndex + 1,
        SlotAddress.SegmentIndex,
        SlotAddress.PartSlotIndex,
        *PartName.ToString(),
        *GetNameSafe(PartOption.PartClass));
    return true;
}

void ACMChimera::FillAllDebugSlotsWithPart(FName PartName)
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const int32 ActiveSlotCount =
        ActiveSegmentCount * CMControl::PartSlotsPerSegment;
    int32 AttachedCount = 0;
    for (int32 FlatSlotIndex = 0;
        FlatSlotIndex < ActiveSlotCount;
        ++FlatSlotIndex)
    {
        AttachedCount += SpawnDebugPartAtSlot(FlatSlotIndex, PartName)
            ? 1
            : 0;
    }

    UE_LOG(LogChimeraLineBody, Warning,
        TEXT("[Fill All Slots Ready] Part=%s Attached=%d/%d"),
        *PartName.ToString(),
        AttachedCount,
        ActiveSlotCount);
}

void ACMChimera::ClearRandomDebugParts()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    TArray<ACMPartActorBase*> RandomParts;
    for (TActorIterator<ACMPartActorBase> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(CheatSpawnedRandomPartTag))
        {
            RandomParts.Add(*It);
        }
    }

    int32 RemovedCount = 0;
    for (ACMPartActorBase* PartActor : RandomParts)
    {
        if (!IsValid(PartActor))
        {
            continue;
        }

        if (UCMPartSlotComponent* PartSlot =
                PartActor->GetAttachedPartSlot())
        {
            PartSlot->DetachPart();
        }

        PartActor->Destroy();
        ++RemovedCount;
    }

    UE_LOG(LogChimeraLineBody, Warning,
        TEXT("[Random Parts Cleared] Removed %d cheat-spawned production Parts."),
        RemovedCount);
}

void ACMChimera::SpawnTestLegParts()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    ClearTestLegParts();

    UClass* ResolvedLegPartClass = LoadTestLegPartClass();
    if (!ResolvedLegPartClass)
    {
        UE_LOG(LogChimeraLineBody, Error,
            TEXT("[Test Leg Parts Failed] Could not load BP_CMLegPart."));
        return;
    }

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
        ACMLegPart* LegPart = GetWorld()->SpawnActor<ACMLegPart>(
            ResolvedLegPartClass,
            GetActorLocation(),
            FRotator::ZeroRotator,
            SpawnParameters
        );
        if (!LegPart)
        {
            continue;
        }

        LegPart->Tags.AddUnique(CheatSpawnedLegPartTag);
        if (PartSlotPoints[FlatIndex]->AttachPart(LegPart))
        {
            ++AttachedCount;
        }
        else
        {
            LegPart->Destroy();
        }
    }

    UE_LOG(LogChimeraLineBody, Warning,
        TEXT("[Test Leg Parts Ready] Attached %d production Leg Parts to empty active slots."),
        AttachedCount);
}

void ACMChimera::ClearTestLegParts()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    TArray<ACMLegPart*> TestLegParts;
    for (TActorIterator<ACMLegPart> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(CheatSpawnedLegPartTag))
        {
            TestLegParts.Add(*It);
        }
    }

    int32 RemovedCount = 0;
    for (ACMLegPart* LegPart : TestLegParts)
    {
        if (!IsValid(LegPart))
        {
            continue;
        }

        if (UCMPartSlotComponent* PartSlot =
                LegPart->GetAttachedPartSlot())
        {
            PartSlot->DetachPart();
        }

        LegPart->Destroy();
        ++RemovedCount;
    }

    UE_LOG(LogChimeraLineBody, Warning,
        TEXT("[Test Leg Parts Cleared] Removed %d cheat-spawned Leg Parts."),
        RemovedCount);
}

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
        if ((PressedPartSlotMask & PartSlotBit) == 0)
        {
            InteractionConsumedPartSlotMask &= ~PartSlotBit;
        }
        PressedPartSlotMask |= PartSlotBit;
    }
    else
    {
        PressedPartSlotMask &= ~PartSlotBit;
        InteractionConsumedPartSlotMask &= ~PartSlotBit;
    }

    if (MovementCoordinator)
    {
        if (bPressed && IsBasicArmPartSlot(PartSlotAddress))
        {
            UCMPartSlotComponent* PartSlot =
                GetPartSlotComponent(PartSlotAddress);
            ACMArmPart* ArmPart = PartSlot
                ? Cast<ACMArmPart>(PartSlot->GetAttachedPart())
                : nullptr;
            if (ArmPart)
            {
                MovementCoordinator->TryBeginArmAnchor(*this, *ArmPart);
                if (MovementCoordinator->IsArmHoldingInteractable(PartSlotAddress))
                {
                    InteractionConsumedPartSlotMask |= PartSlotBit;
                }
            }
        }
        else if (!bPressed)
        {
            MovementCoordinator->EndArmAnchor(PartSlotAddress);
        }
    }

    ForceNetUpdate();
}

bool ACMChimera::IsBasicArmPartSlot(
    const FCMPartSlotAddress& PartSlotAddress
) const
{
    const UCMPartSlotComponent* PartSlot =
        GetPartSlotComponent(PartSlotAddress);
    const AActor* AttachedPart = PartSlot
        ? PartSlot->GetAttachedPart()
        : nullptr;
    return AttachedPart
        && AttachedPart->IsA<ACMArmPart>()
        && !AttachedPart->IsA<ACMSpringArmPart>();
}

bool ACMChimera::ShouldActivateBasicArmOnRelease(
    const FCMPartSlotAddress& PartSlotAddress
) const
{
    return CMControl::IsValidPartSlot(PartSlotAddress, ActiveSegmentCount)
        && (InteractionConsumedPartSlotMask
            & (1u << CMControl::ToFlatPartSlotIndex(PartSlotAddress))) == 0
        && IsBasicArmPartSlot(PartSlotAddress)
        && (!MovementCoordinator
            || !MovementCoordinator->IsArmHoldingInteractable(
                PartSlotAddress));
}

void ACMChimera::ClearPressedControlParts()
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bHadPressedPart = PressedPartSlotMask != 0;
    PressedPartSlotMask = 0;
    InteractionConsumedPartSlotMask = 0;

    for (TActorIterator<ACMControlBody> It(GetWorld()); It; ++It)
    {
        It->ClearPressedControlSlots();
    }

    if (bHadPressedPart)
    {
        ForceNetUpdate();
    }
}
