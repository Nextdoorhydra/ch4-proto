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
const FName ArmUpperLeftBoneName(TEXT("upperarm_l"));
const FName ArmLowerLeftBoneName(TEXT("lowerarm_l"));
const FName ArmHandLeftBoneName(TEXT("hand_l"));
const FName ArmUpperRightBoneName(TEXT("upperarm_r"));
const FName ArmLowerRightBoneName(TEXT("lowerarm_r"));
const FName ArmHandRightBoneName(TEXT("hand_r"));

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

bool HasReferenceBone(
    const USkeletalMeshComponent& Mesh,
    const FName BoneName
)
{
    const USkeletalMesh* SkeletalMesh = Mesh.GetSkeletalMeshAsset();
    return SkeletalMesh
        && SkeletalMesh->GetRefSkeleton().FindBoneIndex(BoneName) != INDEX_NONE;
}

bool HasArmReferenceChain(
    const USkeletalMeshComponent& Mesh,
    const FName UpperBone,
    const FName LowerBone,
    const FName HandBone
)
{
    return HasReferenceBone(Mesh, UpperBone)
        && HasReferenceBone(Mesh, LowerBone)
        && HasReferenceBone(Mesh, HandBone);
}

void ConfigureArmSideScale(
    ACMPartActorBase& Part,
    const UCMPartSlotComponent& PartSlot
)
{
    USceneComponent* PartRoot = Part.GetRootComponent();
    USkeletalMeshComponent* PartMesh = Part.GetPartMesh();
    if (!PartRoot || !PartMesh)
    {
        return;
    }

    FName UpperBone;
    FName LowerBone;
    FName HandBone;
    bool bUsesMirroredLeftChain = false;
    if (!PartSlot.ResolveArmReferenceBoneNames(
            *PartMesh,
            UpperBone,
            LowerBone,
            HandBone,
            bUsesMirroredLeftChain))
    {
        return;
    }

    FVector RelativeScale = PartRoot->GetRelativeScale3D();
    double YMagnitude = FMath::Abs(RelativeScale.Y);
    if (YMagnitude <= UE_SMALL_NUMBER)
    {
        YMagnitude = 1.0;
    }
    RelativeScale.Y = bUsesMirroredLeftChain ? -YMagnitude : YMagnitude;
    PartRoot->SetRelativeScale3D(RelativeScale);
}

USceneComponent* ResolveRigAnchor(
    const UCMPartSlotComponent& PartSlot,
    const ECMPartSlotType PartType
)
{
    if (PartType == ECMPartSlotType::Leg)
    {
        return PartSlot.GetLegRigControlAnchor();
    }
    if (PartType == ECMPartSlotType::Arm)
    {
        if (USceneComponent* ArmAnchor = PartSlot.GetArmRigControlAnchor())
        {
            return ArmAnchor;
        }
        return const_cast<UCMPartSlotComponent*>(&PartSlot);
    }
    return nullptr;
}

bool ResolveMountBoneName(
    const USkeletalMeshComponent& PartMesh,
    const UCMPartSlotComponent& PartSlot,
    const ECMPartSlotType PartType,
    FName& OutMountBone
)
{
    if (PartType == ECMPartSlotType::Leg)
    {
        OutMountBone = LegThighBoneName;
        return HasReferenceBone(PartMesh, OutMountBone);
    }

    if (PartType == ECMPartSlotType::Arm)
    {
        FName LowerBone;
        FName HandBone;
        bool bUsesMirroredLeftChain = false;
        return PartSlot.ResolveArmReferenceBoneNames(
            PartMesh,
            OutMountBone,
            LowerBone,
            HandBone,
            bUsesMirroredLeftChain);
    }

    return false;
}

void AlignPartMountBoneToRigAnchor(
    AActor& PartActor,
    const UCMPartSlotComponent& PartSlot,
    const ECMPartSlotType PartType
)
{
    ACMPartActorBase* Part = Cast<ACMPartActorBase>(&PartActor);
    USceneComponent* PartRoot = Part ? Part->GetRootComponent() : nullptr;
    USkeletalMeshComponent* PartMesh = Part ? Part->GetPartMesh() : nullptr;
    USceneComponent* Anchor = ResolveRigAnchor(PartSlot, PartType);
    if (!Part || !PartRoot || !PartMesh || !Anchor)
    {
        return;
    }

    FName MountBone = NAME_None;
    if (!ResolveMountBoneName(*PartMesh, PartSlot, PartType, MountBone))
    {
        UE_LOG(LogChimeraPartSlot, Warning,
            TEXT("[Attach Alignment] Missing mount bone for Part=%s Type=%d Slot=(%d,%d)."),
            *GetNameSafe(&PartActor),
            static_cast<int32>(PartType),
            PartSlot.SegmentIndex,
            PartSlot.PartSlotIndex);
        return;
    }

    FTransform MountReferenceTransform = FTransform::Identity;
    if (!TryGetReferenceComponentTransform(
            *PartMesh,
            MountBone,
            MountReferenceTransform))
    {
        return;
    }

    const FTransform MountToPartRoot =
        MountReferenceTransform * PartMesh->GetRelativeTransform();
    const FTransform AnchorRelativeTransform =
        Anchor->GetComponentTransform().GetRelativeTransform(
            PartSlot.GetComponentTransform());
    FTransform PartRootTransform(
        AnchorRelativeTransform.GetRotation()
            * MountToPartRoot.GetRotation().Inverse(),
        FVector::ZeroVector,
        PartRoot->GetRelativeScale3D()
    );
    const FVector MountLocationWithZeroRoot =
        (MountToPartRoot * PartRootTransform).GetLocation();
    PartRootTransform.SetLocation(
        AnchorRelativeTransform.GetLocation() - MountLocationWithZeroRoot
    );
    PartRoot->SetRelativeTransform(PartRootTransform);
}

void ApplyMountedPartTransform(
    AActor& PartActor,
    const UCMPartSlotComponent& PartSlot,
    const ECMPartSlotType PartType
)
{
    ACMPartActorBase* Part = Cast<ACMPartActorBase>(&PartActor);
    if (PartType == ECMPartSlotType::Arm && Part)
    {
        ConfigureArmSideScale(*Part, PartSlot);
    }

    if (PartType == ECMPartSlotType::Leg
        || PartType == ECMPartSlotType::Arm)
    {
        AlignPartMountBoneToRigAnchor(PartActor, PartSlot, PartType);
    }
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
    ApplyMountedPartTransform(*AttachedPart, *this, PartType);

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

void UCMPartSlotComponent::SetArmRigControlAnchor(
    USceneComponent* InControlAnchor
)
{
    ArmRigControlAnchor = InControlAnchor;
}

USceneComponent* UCMPartSlotComponent::GetArmRigControlAnchor() const
{
    return ArmRigControlAnchor;
}

bool UCMPartSlotComponent::ResolveArmReferenceBoneNames(
    const USkeletalMeshComponent& Mesh,
    FName& OutUpperBone,
    FName& OutLowerBone,
    FName& OutHandBone,
    bool& bOutUsesMirroredLeftChain
) const
{
    const bool bRightSlot = PartSlotIndex == 1;
    const bool bHasLeftChain = HasArmReferenceChain(
        Mesh,
        ArmUpperLeftBoneName,
        ArmLowerLeftBoneName,
        ArmHandLeftBoneName);
    const bool bHasRightChain = HasArmReferenceChain(
        Mesh,
        ArmUpperRightBoneName,
        ArmLowerRightBoneName,
        ArmHandRightBoneName);

    bOutUsesMirroredLeftChain = false;
    if (bRightSlot
        && bPreferNativeRightArmChain
        && bHasRightChain)
    {
        OutUpperBone = ArmUpperRightBoneName;
        OutLowerBone = ArmLowerRightBoneName;
        OutHandBone = ArmHandRightBoneName;
        return true;
    }

    if (bHasLeftChain)
    {
        OutUpperBone = ArmUpperLeftBoneName;
        OutLowerBone = ArmLowerLeftBoneName;
        OutHandBone = ArmHandLeftBoneName;
        bOutUsesMirroredLeftChain = bRightSlot;
        return true;
    }

    // Fallback for a right-only Arm asset or diagnostic mesh.
    if (bHasRightChain)
    {
        OutUpperBone = ArmUpperRightBoneName;
        OutLowerBone = ArmLowerRightBoneName;
        OutHandBone = ArmHandRightBoneName;
        return true;
    }

    return false;
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
        const ECMPartSlotType PartType =
            ICMPartInterface::Execute_GetPartType(AttachedPart);
        ApplyMountedPartTransform(*AttachedPart, *this, PartType);
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
