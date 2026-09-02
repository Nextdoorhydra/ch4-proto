#include "Player/CMPartSlotComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpec.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Player/CMPartInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPartSlot, Log, All);

namespace
{
const FName LegThighBoneName(TEXT("thigh_l"));

bool TryGetReferenceComponentTransform(
    const USkeletalMeshComponent& Mesh,
    const FName BoneName,
    FTransform& OutTransform
)
{
    const USkeletalMesh* SkeletalMesh = Mesh.GetSkeletalMeshAsset();
    if (!SkeletalMesh)
    {
        return false;
    }

    const FReferenceSkeleton& ReferenceSkeleton =
        SkeletalMesh->GetRefSkeleton();
    int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);
    if (BoneIndex == INDEX_NONE)
    {
        return false;
    }

    const TArray<FTransform>& ReferencePose =
        ReferenceSkeleton.GetRefBonePose();
    OutTransform = ReferencePose[BoneIndex];
    BoneIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
    while (ReferencePose.IsValidIndex(BoneIndex))
    {
        OutTransform *= ReferencePose[BoneIndex];
        BoneIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
    }
    return true;
}

void AlignLegThighToRigAnchor(
    AActor& PartActor,
    const UCMPartSlotComponent& PartSlot
)
{
    ACMPartActorBase* Part = Cast<ACMPartActorBase>(&PartActor);
    USceneComponent* Anchor = PartSlot.GetLegRigControlAnchor();
    USceneComponent* PartRoot = Part ? Part->GetRootComponent() : nullptr;
    USkeletalMeshComponent* PartMesh = Part ? Part->GetPartMesh() : nullptr;
    if (!Anchor || !PartRoot || !PartMesh)
    {
        return;
    }

    FTransform ThighReferenceTransform = FTransform::Identity;
    if (!TryGetReferenceComponentTransform(
            *PartMesh,
            LegThighBoneName,
            ThighReferenceTransform))
    {
        return;
    }

    const FTransform ThighToPartRoot =
        ThighReferenceTransform * PartMesh->GetRelativeTransform();
    const FTransform AnchorRelativeTransform =
        Anchor->GetComponentTransform().GetRelativeTransform(
            PartSlot.GetComponentTransform());
    FTransform PartRootTransform(
        AnchorRelativeTransform.GetRotation()
            * ThighToPartRoot.GetRotation().Inverse(),
        FVector::ZeroVector,
        PartRoot->GetRelativeScale3D()
    );
    const FVector ThighLocationWithZeroRoot =
        (ThighToPartRoot * PartRootTransform).GetLocation();
    PartRootTransform.SetLocation(
        AnchorRelativeTransform.GetLocation() - ThighLocationWithZeroRoot
    );
    PartRoot->SetRelativeTransform(PartRootTransform);
}
}

UCMPartSlotComponent::UCMPartSlotComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    bEditableWhenInherited = true;
    SetIsReplicatedByDefault(true);

#if WITH_EDITORONLY_DATA
    bVisualizeComponent = true;
#endif
}

void UCMPartSlotComponent::InitializeSlotAddress(
    int32 InSegmentIndex,
    int32 InPartSlotIndex
)
{
    SegmentIndex = InSegmentIndex;
    PartSlotIndex = InPartSlotIndex;
}

FCMPartSlotAddress UCMPartSlotComponent::GetSlotAddress() const
{
    FCMPartSlotAddress Address;
    Address.SegmentIndex = SegmentIndex;
    Address.PartSlotIndex = PartSlotIndex;
    return Address;
}

void UCMPartSlotComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCMPartSlotComponent, AttachedPart);
}

bool UCMPartSlotComponent::AttachPart(AActor* PartActor)
{
    AActor* ChimeraOwner = GetOwner();
    if (!ChimeraOwner
        || !ChimeraOwner->HasAuthority()
        || !IsValid(PartActor)
        || IsValid(AttachedPart)
        || !PartActor->GetClass()->ImplementsInterface(
            UCMPartInterface::StaticClass()))
    {
        return false;
    }

    const ECMPartSlotType PartType =
        ICMPartInterface::Execute_GetPartType(PartActor);
    if (AllowedPartType != ECMPartSlotType::Any
        && AllowedPartType != PartType)
    {
        UE_LOG(LogChimeraPartSlot, Warning,
            TEXT("[Attach Rejected] Slot=(%d,%d) allows %d but Part=%s is %d."),
            SegmentIndex,
            PartSlotIndex,
            static_cast<int32>(AllowedPartType),
            *GetNameSafe(PartActor),
            static_cast<int32>(PartType));
        return false;
    }

    AttachedPart = PartActor;
    AttachedPart->OnDestroyed.AddDynamic(
        this,
        &UCMPartSlotComponent::HandleAttachedPartDestroyed
    );
    AttachedPart->AttachToComponent(
        this,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale
    );
    if (PartType == ECMPartSlotType::Leg)
    {
        AlignLegThighToRigAnchor(*AttachedPart, *this);
    }

    if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent())
    {
        const TSubclassOf<UGameplayAbility> AbilityClass =
            ICMPartInterface::Execute_GetGrantedAbilityClass(AttachedPart);
        if (AbilityClass)
        {
            FGameplayAbilitySpec AbilitySpec(
                AbilityClass,
                1,
                INDEX_NONE,
                AttachedPart
            );
            GrantedAbilityHandle = ASC->GiveAbility(AbilitySpec);
        }
    }

    ICMPartInterface::Execute_OnAttachedToPartSlot(AttachedPart, this);
    OnAttachedPartChanged.Broadcast(this, AttachedPart);
    ChimeraOwner->ForceNetUpdate();

    UE_LOG(LogChimeraPartSlot, Log,
        TEXT("[Part Attached] Slot=(%d,%d) Part=%s AbilityHandleValid=%s"),
        SegmentIndex,
        PartSlotIndex,
        *GetNameSafe(AttachedPart),
        GrantedAbilityHandle.IsValid() ? TEXT("true") : TEXT("false"));
    return true;
}

AActor* UCMPartSlotComponent::DetachPart()
{
    AActor* ChimeraOwner = GetOwner();
    if (!ChimeraOwner || !ChimeraOwner->HasAuthority()
        || !IsValid(AttachedPart))
    {
        return nullptr;
    }

    AActor* DetachedPart = AttachedPart;
    ICMPartInterface::Execute_OnDetachedFromPartSlot(DetachedPart, this);
    RemoveGrantedAbility();
    DetachedPart->OnDestroyed.RemoveDynamic(
        this,
        &UCMPartSlotComponent::HandleAttachedPartDestroyed
    );
    DetachedPart->DetachFromActor(
        FDetachmentTransformRules::KeepWorldTransform
    );
    AttachedPart = nullptr;

    OnAttachedPartChanged.Broadcast(this, nullptr);
    ChimeraOwner->ForceNetUpdate();

    UE_LOG(LogChimeraPartSlot, Log,
        TEXT("[Part Detached] Slot=(%d,%d) Part=%s"),
        SegmentIndex,
        PartSlotIndex,
        *GetNameSafe(DetachedPart));
    return DetachedPart;
}

bool UCMPartSlotComponent::TryActivateGrantedAbility()
{
    UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
    return ASC
        && GrantedAbilityHandle.IsValid()
        && ASC->TryActivateAbility(GrantedAbilityHandle);
}

AActor* UCMPartSlotComponent::GetAttachedPart() const
{
    return AttachedPart;
}

bool UCMPartSlotComponent::HasAttachedPart() const
{
    return IsValid(AttachedPart);
}

void UCMPartSlotComponent::SetLegRigControlAnchor(
    USceneComponent* InControlAnchor
)
{
    LegRigControlAnchor = InControlAnchor;
}

USceneComponent* UCMPartSlotComponent::GetLegRigControlAnchor() const
{
    return LegRigControlAnchor;
}

void UCMPartSlotComponent::OnRep_AttachedPart(AActor* PreviousPart)
{
    if (IsValid(PreviousPart)
        && PreviousPart->GetAttachParentActor() == GetOwner())
    {
        PreviousPart->DetachFromActor(
            FDetachmentTransformRules::KeepWorldTransform
        );
    }

    if (IsValid(AttachedPart))
    {
        AttachedPart->AttachToComponent(
            this,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale
        );
        if (ICMPartInterface::Execute_GetPartType(AttachedPart)
            == ECMPartSlotType::Leg)
        {
            AlignLegThighToRigAnchor(*AttachedPart, *this);
        }
    }

    OnAttachedPartChanged.Broadcast(this, AttachedPart);
}

void UCMPartSlotComponent::HandleAttachedPartDestroyed(AActor* DestroyedPart)
{
    if (DestroyedPart != AttachedPart || !GetOwner()
        || !GetOwner()->HasAuthority())
    {
        return;
    }

    RemoveGrantedAbility();
    AttachedPart = nullptr;
    OnAttachedPartChanged.Broadcast(this, nullptr);
    GetOwner()->ForceNetUpdate();
}

UAbilitySystemComponent*
UCMPartSlotComponent::GetOwnerAbilitySystemComponent() const
{
    const IAbilitySystemInterface* AbilityOwner =
        Cast<IAbilitySystemInterface>(GetOwner());
    return AbilityOwner ? AbilityOwner->GetAbilitySystemComponent() : nullptr;
}

void UCMPartSlotComponent::RemoveGrantedAbility()
{
    if (!GrantedAbilityHandle.IsValid())
    {
        return;
    }

    if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent())
    {
        ASC->CancelAbilityHandle(GrantedAbilityHandle);
        ASC->ClearAbility(GrantedAbilityHandle);
    }
    GrantedAbilityHandle = FGameplayAbilitySpecHandle();
}
