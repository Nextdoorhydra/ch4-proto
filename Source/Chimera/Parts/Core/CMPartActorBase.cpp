#include "Parts/Core/CMPartActorBase.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/CMPartSlotComponent.h"

ACMPartActorBase::ACMPartActorBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PartMesh"));
    PartMesh->SetupAttachment(SceneRoot);
}

void ACMPartActorBase::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMPartActorBase, AttachedSlotAddress);
}

ECMPartSlotType ACMPartActorBase::GetPartType_Implementation() const
{
    return PartType;
}

TSubclassOf<UGameplayAbility>
ACMPartActorBase::GetGrantedAbilityClass_Implementation() const
{
    return GrantedAbilityClass;
}

void ACMPartActorBase::OnAttachedToPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    if (!HasAuthority() || !PartSlot)
    {
        return;
    }

    AttachedPartSlot = PartSlot;
    AttachedSlotAddress = PartSlot->GetSlotAddress();
    ForceNetUpdate();
}

void ACMPartActorBase::OnDetachedFromPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    if (!HasAuthority())
    {
        return;
    }

    AttachedPartSlot.Reset();
    AttachedSlotAddress = FCMPartSlotAddress();
    ForceNetUpdate();
}

UCMPartSlotComponent* ACMPartActorBase::GetAttachedPartSlot() const
{
    if (UCMPartSlotComponent* PartSlot = AttachedPartSlot.Get())
    {
        return PartSlot;
    }

    return SceneRoot
        ? Cast<UCMPartSlotComponent>(SceneRoot->GetAttachParent())
        : nullptr;
}

FCMPartSlotAddress ACMPartActorBase::GetAttachedSlotAddress() const
{
    return AttachedSlotAddress;
}
