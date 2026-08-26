#include "Parts/Leg/CMLegPart.h"

#include "Ability/CMLegGameplayAbility.h"
#include "Data/Part/CMPartLegArmTableRow.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ACMLegPart::ACMLegPart()
{
    PartType = ECMPartSlotType::Leg;
    GrantedAbilityClass = UCMLegGameplayAbility::StaticClass();
    PartRowName = TEXT("DefaultLeg");
}

void ACMLegPart::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMLegPart, StepDirection);
    DOREPLIFETIME(ACMLegPart, StepGroundLocation);
    DOREPLIFETIME(ACMLegPart, StepGroundNormal);
    DOREPLIFETIME(ACMLegPart, StepStartTime);
    DOREPLIFETIME(ACMLegPart, StepDuration);
}

void ACMLegPart::OnDetachedFromPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    EndProceduralStep();
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
    const float Duration
)
{
    if (!HasAuthority())
    {
        return;
    }
    StepDirection = bReverseMovement
        ? ECMLegStepDirection::Reverse
        : ECMLegStepDirection::Forward;
    StepGroundLocation = GroundLocation;
    StepGroundNormal = GroundNormal.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    StepStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    StepDuration = FMath::Max(Duration, 0.01f);
    OnStepStateChanged.Broadcast(
        StepDirection,
        StepGroundLocation,
        StepGroundNormal,
        StepDuration);
    ForceNetUpdate();
}

void ACMLegPart::EndProceduralStep()
{
    if (!HasAuthority() || StepDirection == ECMLegStepDirection::None)
    {
        return;
    }
    StepDirection = ECMLegStepDirection::None;
    OnStepStateChanged.Broadcast(
        StepDirection,
        StepGroundLocation,
        StepGroundNormal,
        0.0f);
    ForceNetUpdate();
}

float ACMLegPart::GetStepPhase() const
{
    const UWorld* World = GetWorld();
    return StepDirection != ECMLegStepDirection::None && World
        ? FMath::Clamp(
            (World->GetTimeSeconds() - StepStartTime)
                / FMath::Max(StepDuration, 0.01f),
            0.0f,
            1.0f)
        : 0.0f;
}

void ACMLegPart::OnRep_StepState()
{
    OnStepStateChanged.Broadcast(
        StepDirection,
        StepGroundLocation,
        StepGroundNormal,
        StepDuration);
}

void ACMLegPart::ApplyPartData(const FCMPartLegArmTableRow& PartRow)
{
    StaminaCost = FMath::Max(PartRow.StaminaCost, 0.0f);
    ActionDuration = FMath::Max(PartRow.ActionDuration, 0.01f);
}
