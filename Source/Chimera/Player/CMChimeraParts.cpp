#include "Player/CMChimera.h"

#include "Movement/CMLineBodyMovementCoordinator.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Arm/CMSpringArmPart.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Leg/CMLegPart.h"
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
        ACMPartActorBase* PartActor =
            Cast<ACMPartActorBase>(PartSlot->GetAttachedPart());
        if (PartActor)
        {
            PartActor->SetContributingPlayerState(
                ContributingPlayerState
            );
        }

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
        if (PartActor && !bActivated)
        {
            PartActor->ConsumeContributingPlayerState();
        }
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

bool ACMChimera::TryActivateLegPart(
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState
)
{
    if (!HasAuthority()
        || !CMControl::IsValidPartSlot(
            PartSlotAddress,
            ActiveSegmentCount))
    {
        return false;
    }

    const int32 SegmentIndex = PartSlotAddress.SegmentIndex;
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
            SegmentIndex,
            PartSlot,
            ContributingPlayerState,
            LegPart->GetMovementImpulseMultiplier()
        );
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

#if !UE_BUILD_SHIPPING
void ACMChimera::ActivateDebugLegPart(
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState
)
{
    TryActivateLegPart(
        PartSlotAddress,
        ContributingPlayerState
    );
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
namespace
{
const FName CheatSpawnedRandomPartTag(TEXT("CM.CheatSpawnedRandomPart"));
const FName CheatSpawnedLegPartTag(TEXT("CM.CheatSpawnedLegPart"));

UClass* LoadTestLegPartClass()
{
    static TSoftClassPtr<ACMLegPart> TestLegPartClass(
        FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/BP_CMLegPart.BP_CMLegPart_C")
        )
    );
    return TestLegPartClass.LoadSynchronous();
}

UClass* LoadTestLegTierPartClass(int32 Tier)
{
    static const TSoftClassPtr<ACMLegPart> TestLegTierPartClasses[] =
    {
        TSoftClassPtr<ACMLegPart>(FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/BP_CMLegTier1Part.BP_CMLegTier1Part_C"))),
        TSoftClassPtr<ACMLegPart>(FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/BP_CMLegTier2Part.BP_CMLegTier2Part_C"))),
        TSoftClassPtr<ACMLegPart>(FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/BP_CMLegTier3Part.BP_CMLegTier3Part_C"))),
        TSoftClassPtr<ACMLegPart>(FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/BP_CMLegTier4Part.BP_CMLegTier4Part_C"))),
        TSoftClassPtr<ACMLegPart>(FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/BP_CMLegTier5Part.BP_CMLegTier5Part_C")))
    };

    const int32 TierIndex = Tier - 1;
    return TierIndex >= 0
        && TierIndex < UE_ARRAY_COUNT(TestLegTierPartClasses)
        ? TestLegTierPartClasses[TierIndex].LoadSynchronous()
        : nullptr;
}

void AddTestLegTierPartClasses(TArray<UClass*>& OutPartClasses)
{
    for (int32 Tier = 1; Tier <= 5; ++Tier)
    {
        if (UClass* LoadedClass = LoadTestLegTierPartClass(Tier))
        {
            OutPartClasses.Add(LoadedClass);
        }
    }
}

UClass* LoadTestArmPartClass()
{
    static TSoftClassPtr<ACMArmPart> TestArmPartClass(
        FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/BP_CMArmPart.BP_CMArmPart_C")
        )
    );
    return TestArmPartClass.LoadSynchronous();
}

UClass* LoadTestSpringArmPartClass()
{
    static TSoftClassPtr<ACMSpringArmPart> TestSpringArmPartClass(
        FSoftObjectPath(
            TEXT("/Game/Chimera/Character/Part/BP_CMSpringArmPart.BP_CMSpringArmPart_C")
        )
    );
    return TestSpringArmPartClass.LoadSynchronous();
}

UClass* LoadNamedDebugPartClass(FName PartName)
{
    if (PartName == TEXT("Arm"))
    {
        return LoadTestArmPartClass();
    }
    if (PartName == TEXT("SpringArm"))
    {
        return LoadTestSpringArmPartClass();
    }

    const FString PartString = PartName.ToString();
    constexpr TCHAR LegTierPrefix[] = TEXT("LegTier");
    if (PartString.StartsWith(LegTierPrefix, ESearchCase::IgnoreCase))
    {
        return LoadTestLegTierPartClass(
            FCString::Atoi(*PartString.RightChop(UE_ARRAY_COUNT(LegTierPrefix) - 1))
        );
    }

    return nullptr;
}
}

void ACMChimera::SpawnRandomDebugParts()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    ClearRandomDebugParts();

    TArray<UClass*> RegisteredPartClasses;
    AddTestLegTierPartClasses(RegisteredPartClasses);
    if (UClass* ArmPartClass = LoadTestArmPartClass())
    {
        RegisteredPartClasses.Add(ArmPartClass);
    }
    if (UClass* SpringArmPartClass = LoadTestSpringArmPartClass())
    {
        RegisteredPartClasses.Add(SpringArmPartClass);
    }

    if (RegisteredPartClasses.IsEmpty())
    {
        UE_LOG(LogChimeraLineBody, Error,
            TEXT("[Random Parts Failed] No production Part Blueprint could be loaded."));
        return;
    }

    int32 AttachedCount = 0;
    int32 NextPartClassIndex = FMath::RandHelper(
        RegisteredPartClasses.Num()
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

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        UClass* SelectedPartClass =
            RegisteredPartClasses[NextPartClassIndex];
        NextPartClassIndex = (NextPartClassIndex + 1)
            % RegisteredPartClasses.Num();
        ACMPartActorBase* PartActor =
            GetWorld()->SpawnActor<ACMPartActorBase>(
                SelectedPartClass,
                GetActorLocation(),
                FRotator::ZeroRotator,
                SpawnParameters
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
        TEXT("[Random Parts Ready] Attached %d production Parts from %d registered Blueprint class(es)."),
        AttachedCount,
        RegisteredPartClasses.Num());
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

    UClass* PartClass = LoadNamedDebugPartClass(PartName);
    if (!PartClass)
    {
        UE_LOG(LogChimeraLineBody, Warning,
            TEXT("[Attach Part Failed] Unknown Part=%s. Use Arm, SpringArm, or LegTier1..5."),
            *PartName.ToString());
        return false;
    }

    UCMPartSlotComponent* PartSlot = PartSlotPoints[FlatSlotIndex];
    if (AActor* ExistingPart = PartSlot->DetachPart())
    {
        ExistingPart->Destroy();
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACMPartActorBase* PartActor =
        GetWorld()->SpawnActor<ACMPartActorBase>(
            PartClass,
            GetActorLocation(),
            FRotator::ZeroRotator,
            SpawnParameters
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
        *GetNameSafe(PartClass));
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
