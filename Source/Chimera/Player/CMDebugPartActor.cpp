#include "Player/CMDebugPartActor.h"

#include "Ability/CMDebugPartLogAbility.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/CMPartSlotComponent.h"
#include "Player/CMPlayerState.h"

ACMDebugPartActor::ACMDebugPartActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
}

void ACMDebugPartActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMDebugPartActor, PartType);
    DOREPLIFETIME(ACMDebugPartActor, AttachedSlotAddress);
}

void ACMDebugPartActor::InitializeDebugPart(ECMPartSlotType InPartType)
{
    if (HasAuthority())
    {
        PartType = InPartType;
    }
}

ECMPartSlotType ACMDebugPartActor::GetPartType_Implementation() const
{
    return PartType;
}

TSubclassOf<UGameplayAbility>
ACMDebugPartActor::GetGrantedAbilityClass_Implementation() const
{
    return UCMDebugPartLogAbility::StaticClass();
}

void ACMDebugPartActor::OnAttachedToPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    if (HasAuthority() && PartSlot)
    {
        AttachedSlotAddress = PartSlot->GetSlotAddress();
    }
}

void ACMDebugPartActor::OnDetachedFromPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    if (HasAuthority())
    {
        AttachedSlotAddress = FCMPartSlotAddress();
    }
}

const FCMPartSlotAddress& ACMDebugPartActor::GetAttachedSlotAddress() const
{
    return AttachedSlotAddress;
}

FString ACMDebugPartActor::GetPartTypeName() const
{
    switch (PartType)
    {
    case ECMPartSlotType::Head:
        return TEXT("Head");
    case ECMPartSlotType::Arm:
        return TEXT("Arm");
    case ECMPartSlotType::Leg:
        return TEXT("Leg");
    case ECMPartSlotType::Organ:
        return TEXT("Organ");
    default:
        return TEXT("Any");
    }
}

void ACMDebugPartActor::SetContributingPlayerState(
    ACMPlayerState* PlayerState
)
{
    PendingContributingPlayerState = PlayerState;
}

ACMPlayerState* ACMDebugPartActor::ConsumeContributingPlayerState()
{
    ACMPlayerState* PlayerState = PendingContributingPlayerState.Get();
    PendingContributingPlayerState.Reset();
    return PlayerState;
}
