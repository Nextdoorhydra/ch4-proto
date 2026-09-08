#include "Parts/Leg/CMLegPart.h"

#include "Ability/CMLegGameplayAbility.h"
#include "Components/CMBloodTransferComponent.h"
#include "Data/Part/CMPartLegArmTableRow.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Player/CMPartSlotComponent.h"
#include "Sound/CMSoundPlayback.h"
#include "Sound/CMSoundTags.h"
#include "Stage/Trigger/Component/CMMechanismWeightComponent.h"

namespace
{
float GetLegServerWorldTime(const UWorld* World)
{
    if (const AGameStateBase* GameState = World ? World->GetGameState() : nullptr)
    {
        return GameState->GetServerWorldTimeSeconds();
    }
    return World ? World->GetTimeSeconds() : 0.0f;
}
}

ACMLegPart::ACMLegPart()
{
    PartType = ECMPartSlotType::Leg;
    GrantedAbilityClass = UCMLegGameplayAbility::StaticClass();
    PartRowName = TEXT("DefaultLeg");
    BloodTransferComponent =
        CreateDefaultSubobject<UCMBloodTransferComponent>(
            TEXT("BloodTransferComponent"));
    BloodTransferComponent->InitializeTransfer(PartMesh, TEXT("Human.Red"));
    MechanismWeightComponent = CreateDefaultSubobject<UCMMechanismWeightComponent>(TEXT("MechanismWeight"));
    MechanismWeightComponent->MechanismWeight = 0.0f;
}

void ACMLegPart::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMLegPart, PlantSnapshot);
}

void ACMLegPart::OnDetachedFromPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    CancelProceduralStep();
    Super::OnDetachedFromPartSlot_Implementation(PartSlot);
}

float ACMLegPart::GetStaminaCost() const
{
    return StaminaCost;
}

float ACMLegPart::GetActionDuration() const
{
    return ActionDuration;
}

void ACMLegPart::SetPendingReverseMovement(bool bReverseMovement)
{
    bPendingReverseMovement = bReverseMovement;
}

bool ACMLegPart::ConsumePendingReverseMovement()
{
    const bool bWasReverse = bPendingReverseMovement;
    bPendingReverseMovement = false;
    return bWasReverse;
}

void ACMLegPart::BeginProceduralStep(
    const bool bReverseMovement,
    const FVector GroundLocation,
    const FVector GroundNormal,
    const float Duration,
    const ECMLegPlantTrigger Trigger
)
{
    if (!HasAuthority()
        || PlantSnapshot.State == ECMLegPlantState::Swing
        || PlantSnapshot.State == ECMLegPlantState::Landing
        || PlantSnapshot.State == ECMLegPlantState::Recover)
    {
        return;
    }
    BeginPlantTransition(
        bReverseMovement
            ? ECMLegStepDirection::Reverse
            : ECMLegStepDirection::Forward,
        Trigger,
        GroundLocation,
        GroundNormal,
        Duration);
}

void ACMLegPart::BeginVisualReplant(
    const FVector GroundLocation,
    const FVector GroundNormal,
    const float Duration
)
{
    if (!HasAuthority()
        || !IsOperational()
        || PlantSnapshot.State != ECMLegPlantState::Planted)
    {
        return;
    }

    BeginPlantTransition(
        ECMLegStepDirection::None,
        ECMLegPlantTrigger::ReachRecovery,
        GroundLocation,
        GroundNormal,
        Duration);
}

void ACMLegPart::InitializePlantedContact(
    const FVector GroundLocation,
    const FVector GroundNormal
)
{
    if (!HasAuthority()
        || !IsOperational()
        || PlantSnapshot.State != ECMLegPlantState::Free
        || GroundLocation.ContainsNaN()
        || GroundNormal.ContainsNaN())
    {
        return;
    }

    const FVector SafeNormal = GroundNormal.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    if (SafeNormal.IsNearlyZero() || SafeNormal.Z <= 0.0f)
    {
        return;
    }

    PlantSnapshot.State = ECMLegPlantState::Planted;
    PlantSnapshot.Trigger = ECMLegPlantTrigger::Initialization;
    PlantSnapshot.StepDirection = ECMLegStepDirection::None;
    PlantSnapshot.bContactValid = true;
    PlantSnapshot.StartGroundLocation = GroundLocation;
    PlantSnapshot.StartGroundNormal = SafeNormal;
    PlantSnapshot.GroundLocation = GroundLocation;
    PlantSnapshot.GroundNormal = SafeNormal;
    PlantSnapshot.ServerStartTime = GetLegServerWorldTime(GetWorld());
    PlantSnapshot.Duration = 0.0f;
    ++PlantSnapshot.Sequence;

    if (BloodTransferComponent)
    {
        BloodTransferComponent->ProcessContactSample(
            GroundLocation,
            SafeNormal,
            0.0f);
    }

    StepDirection = PlantSnapshot.StepDirection;
    StepGroundLocation = PlantSnapshot.GroundLocation;
    StepGroundNormal = PlantSnapshot.GroundNormal;
    BroadcastPlantState();
    ForceNetUpdate();
}

void ACMLegPart::BeginPlantTransition(
    const ECMLegStepDirection NewStepDirection,
    const ECMLegPlantTrigger Trigger,
    const FVector GroundLocation,
    const FVector GroundNormal,
    const float Duration
)
{
    if (!HasAuthority()
        || GroundLocation.ContainsNaN()
        || GroundNormal.ContainsNaN())
    {
        return;
    }

    const FVector SafeNormal = GroundNormal.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    if (SafeNormal.IsNearlyZero() || SafeNormal.Z <= 0.0f)
    {
        return;
    }

    const FVector StartLocation = PlantSnapshot.bContactValid
        ? FVector(PlantSnapshot.GroundLocation)
        : GetCurrentPlantStartLocation();
    const FVector StartNormal = PlantSnapshot.bContactValid
        ? FVector(PlantSnapshot.GroundNormal).GetSafeNormal(
            SMALL_NUMBER,
            SafeNormal)
        : SafeNormal;
    const float SafeDuration = FMath::Max(Duration, 0.01f);
    const float CurrentTime = GetLegServerWorldTime(GetWorld());

    if (BloodTransferComponent)
    {
        const float ContactSpeed = FVector::Distance(
            StartLocation,
            GroundLocation) / SafeDuration;
        BloodTransferComponent->ProcessContactSample(
            StartLocation,
            StartNormal,
            ContactSpeed);
    }

    PlantSnapshot.State = ECMLegPlantState::Swing;
    PlantSnapshot.Trigger = Trigger;
    PlantSnapshot.StepDirection = NewStepDirection;
    PlantSnapshot.bContactValid = true;
    PlantSnapshot.StartGroundLocation = StartLocation;
    PlantSnapshot.StartGroundNormal = StartNormal;
    PlantSnapshot.GroundLocation = GroundLocation;
    PlantSnapshot.GroundNormal = SafeNormal;
    PlantSnapshot.ServerStartTime = CurrentTime;
    PlantSnapshot.Duration = SafeDuration;
    ++PlantSnapshot.Sequence;

    StepDirection = PlantSnapshot.StepDirection;
    StepGroundLocation = PlantSnapshot.GroundLocation;
    StepGroundNormal = PlantSnapshot.GroundNormal;
    BroadcastPlantState();
    ForceNetUpdate();
}

void ACMLegPart::EndProceduralStep()
{
    if (!HasAuthority()
        || (PlantSnapshot.State != ECMLegPlantState::Swing
            && PlantSnapshot.State != ECMLegPlantState::Landing))
    {
        return;
    }

    if (BloodTransferComponent && PlantSnapshot.bContactValid)
    {
        const float ContactSpeed = FVector::Distance(
            FVector(PlantSnapshot.StartGroundLocation),
            FVector(PlantSnapshot.GroundLocation)) /
            FMath::Max(PlantSnapshot.Duration, 0.01f);
        BloodTransferComponent->ProcessContactSample(
            PlantSnapshot.GroundLocation,
            PlantSnapshot.GroundNormal,
            ContactSpeed);
    }

    const bool bShouldPlayFootstep = PlantSnapshot.bContactValid
        && (PlantSnapshot.Trigger == ECMLegPlantTrigger::PlayerInput
            || PlantSnapshot.Trigger == ECMLegPlantTrigger::ReachRecovery);
    const FVector_NetQuantize10 FootstepLocation =
        PlantSnapshot.GroundLocation;

    PlantSnapshot.State = PlantSnapshot.bContactValid
        ? ECMLegPlantState::Planted
        : ECMLegPlantState::Free;
    PlantSnapshot.StepDirection = ECMLegStepDirection::None;
    PlantSnapshot.Trigger = PlantSnapshot.State == ECMLegPlantState::Planted
        ? PlantSnapshot.Trigger
        : ECMLegPlantTrigger::None;
    PlantSnapshot.ServerStartTime = GetLegServerWorldTime(GetWorld());
    PlantSnapshot.Duration = 0.0f;
    ++PlantSnapshot.Sequence;

    StepDirection = PlantSnapshot.StepDirection;
    StepGroundLocation = PlantSnapshot.GroundLocation;
    StepGroundNormal = PlantSnapshot.GroundNormal;
    BroadcastPlantState();
    ForceNetUpdate();

    if (bShouldPlayFootstep)
    {
        MulticastPlayFootstep(FootstepLocation);
    }
}

void ACMLegPart::CancelProceduralStep(
    const ECMLegPlantTrigger Trigger
)
{
    if (!HasAuthority()
        || PlantSnapshot.State == ECMLegPlantState::Free)
    {
        return;
    }

    PlantSnapshot.State = ECMLegPlantState::Recover;
    PlantSnapshot.Trigger = Trigger;
    PlantSnapshot.StepDirection = ECMLegStepDirection::None;
    PlantSnapshot.bContactValid = false;
    PlantSnapshot.ServerStartTime = GetLegServerWorldTime(GetWorld());
    PlantSnapshot.Duration = 0.08f;
    ++PlantSnapshot.Sequence;

    StepDirection = PlantSnapshot.StepDirection;
    StepGroundLocation = PlantSnapshot.GroundLocation;
    StepGroundNormal = PlantSnapshot.GroundNormal;
    BroadcastPlantState();
    ForceNetUpdate();
}

void ACMLegPart::AdvancePlantState()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const float CurrentTime = GetLegServerWorldTime(GetWorld());
    const float Duration = FMath::Max(PlantSnapshot.Duration, 0.01f);
    const float Phase = FMath::Clamp(
        (CurrentTime - PlantSnapshot.ServerStartTime) / Duration,
        0.0f,
        1.0f);
    if (PlantSnapshot.State == ECMLegPlantState::Swing
        && Phase >= 0.75f)
    {
        PlantSnapshot.State = ECMLegPlantState::Landing;
        ++PlantSnapshot.Sequence;
        BroadcastPlantState();
        ForceNetUpdate();
    }
    else if (PlantSnapshot.State == ECMLegPlantState::Landing
        && PlantSnapshot.StepDirection == ECMLegStepDirection::None
        && Phase >= 1.0f)
    {
        // Visual replant has no ActiveLegSteps owner. It may complete here
        // because its target was already server-traced and carries no force.
        EndProceduralStep();
    }
    else if (PlantSnapshot.State == ECMLegPlantState::Recover
        && Phase >= 1.0f)
    {
        PlantSnapshot.State = ECMLegPlantState::Free;
        PlantSnapshot.Trigger = ECMLegPlantTrigger::None;
        PlantSnapshot.bContactValid = false;
        PlantSnapshot.Duration = 0.0f;
        ++PlantSnapshot.Sequence;
        BroadcastPlantState();
        ForceNetUpdate();
    }
}

float ACMLegPart::GetSideSign() const
{
    const FCMPartSlotAddress SlotAddress = GetAttachedSlotAddress();
    return SlotAddress.PartSlotIndex == 0 ? -1.0f : 1.0f;
}

FVector ACMLegPart::GetCurrentPlantStartLocation() const
{
    if (const UCMPartSlotComponent* PartSlot = GetAttachedPartSlot())
    {
        return PartSlot->GetComponentLocation();
    }
    return GetActorLocation();
}

void ACMLegPart::BroadcastPlantState()
{
    OnStepStateChanged.Broadcast(
        PlantSnapshot.StepDirection,
        PlantSnapshot.GroundLocation,
        PlantSnapshot.GroundNormal,
        PlantSnapshot.Duration);
}

void ACMLegPart::OnRep_PlantSnapshot()
{
    if (LastAppliedPlantSequence != INDEX_NONE
        && PlantSnapshot.Sequence <= LastAppliedPlantSequence)
    {
        return;
    }
    LastAppliedPlantSequence = PlantSnapshot.Sequence;
    StepDirection = PlantSnapshot.StepDirection;
    StepGroundLocation = PlantSnapshot.GroundLocation;
    StepGroundNormal = PlantSnapshot.GroundNormal;
    OnStepStateChanged.Broadcast(
        PlantSnapshot.StepDirection,
        PlantSnapshot.GroundLocation,
        PlantSnapshot.GroundNormal,
        PlantSnapshot.Duration);
}

void ACMLegPart::OnRep_StepState()
{
    OnRep_PlantSnapshot();
}

void ACMLegPart::MulticastPlayFootstep_Implementation(
    const FVector_NetQuantize10 GroundLocation)
{
    FCMSoundPlayback::PlaySFXAtLocation(
        this,
        GroundLocation,
        CMSoundTags::Body_Footstep);
}

float ACMLegPart::GetStepPhase() const
{
    const UWorld* World = GetWorld();
    if (PlantSnapshot.State == ECMLegPlantState::Free)
    {
        return 0.0f;
    }
    if (PlantSnapshot.State == ECMLegPlantState::Planted)
    {
        return 1.0f;
    }
    return World
        ? FMath::Clamp(
            (GetLegServerWorldTime(World) - PlantSnapshot.ServerStartTime)
                / FMath::Max(PlantSnapshot.Duration, 0.01f),
            0.0f,
            1.0f)
        : 0.0f;
}

void ACMLegPart::ApplyPartData(const FCMPartLegArmTableRow& PartRow)
{
    MechanismWeightComponent->MechanismWeight = FMath::Max(PartRow.Weight, 0.0f);
    StaminaCost = FMath::Max(PartRow.StaminaCost, 0.0f);
    ActionDuration = FMath::Max(PartRow.ActionDuration, 0.01f);
}
