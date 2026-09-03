#include "Animation/CMProceduralAnimationTestBody.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Parts/Leg/CMLegPart.h"
#include "Player/CMPartInterface.h"
#include "ReferenceSkeleton.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMProceduralAnimationTestBody, Log, All);

namespace
{
const FName LegThighBoneName(TEXT("thigh_l"));
const FName LegCalfBoneName(TEXT("calf_l"));
const FName LegFootBoneName(TEXT("foot_l"));
const FKey ManualArmControlKeys[] = {
    EKeys::Q,
    EKeys::W,
    EKeys::E,
    EKeys::R
};

int32 FindManualArmControlKeyIndex(const FKey Key)
{
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ManualArmControlKeys); ++Index)
    {
        if (ManualArmControlKeys[Index] == Key)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

bool TryGetTestBodyReferenceComponentTransform(
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
    const TArray<FTransform>& ReferencePose =
        ReferenceSkeleton.GetRefBonePose();
    if (!ReferencePose.IsValidIndex(BoneIndex))
    {
        return false;
    }

    OutTransform = ReferencePose[BoneIndex];
    BoneIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
    while (ReferencePose.IsValidIndex(BoneIndex))
    {
        OutTransform *= ReferencePose[BoneIndex];
        BoneIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
    }
    return true;
}

bool HasTestBodyReferenceBone(
    const USkeletalMeshComponent& Mesh,
    const FName BoneName
)
{
    const USkeletalMesh* SkeletalMesh = Mesh.GetSkeletalMeshAsset();
    return SkeletalMesh
        && SkeletalMesh->GetRefSkeleton().FindBoneIndex(BoneName) != INDEX_NONE;
}


USceneComponent* ResolveTestBodyRigAnchor(
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

bool ResolveTestBodyMountBoneName(
    const USkeletalMeshComponent& PartMesh,
    const UCMPartSlotComponent& PartSlot,
    const ECMPartSlotType PartType,
    FName& OutMountBone,
    bool& bOutUsesMirroredLeftChain
)
{
    bOutUsesMirroredLeftChain = false;
    if (PartType == ECMPartSlotType::Leg)
    {
        OutMountBone = LegThighBoneName;
        return HasTestBodyReferenceBone(PartMesh, OutMountBone);
    }
    if (PartType == ECMPartSlotType::Arm)
    {
        FName LowerBone;
        FName HandBone;
        return PartSlot.ResolveArmReferenceBoneNames(
            PartMesh,
            OutMountBone,
            LowerBone,
            HandBone,
            bOutUsesMirroredLeftChain);
    }
    return false;
}

FVector ResolvePartScaleForSlot(
    ACMPartActorBase& Part,
    const UCMPartSlotComponent& PartSlot,
    FVector RequestedScale
)
{
    const ECMPartSlotType PartType =
        ICMPartInterface::Execute_GetPartType(&Part);
    if (PartType != ECMPartSlotType::Arm || !Part.GetPartMesh())
    {
        return RequestedScale;
    }

    FName MountBone;
    bool bUsesMirroredLeftChain = false;
    if (ResolveTestBodyMountBoneName(
            *Part.GetPartMesh(),
            PartSlot,
            PartType,
            MountBone,
            bUsesMirroredLeftChain))
    {
        double YMagnitude = FMath::Abs(RequestedScale.Y);
        if (YMagnitude <= UE_SMALL_NUMBER)
        {
            YMagnitude = 1.0;
        }
        RequestedScale.Y = bUsesMirroredLeftChain ? -YMagnitude : YMagnitude;
    }
    return RequestedScale;
}

bool TryCalculatePartAttachmentTransform(
    const USkeletalMeshComponent& PartMesh,
    const FName MountBone,
    const FTransform& MeshToAttachmentRoot,
    const FTransform& AnchorRelativeTransform,
    const FTransform& ExistingAttachmentTransform,
    FTransform& OutAttachmentTransform
)
{
    FTransform MountReferenceTransform = FTransform::Identity;
    if (!TryGetTestBodyReferenceComponentTransform(
            PartMesh,
            MountBone,
            MountReferenceTransform))
    {
        return false;
    }

    const FTransform MountToAttachmentRoot =
        MountReferenceTransform * MeshToAttachmentRoot;
    FTransform AttachmentTransform(
        AnchorRelativeTransform.GetRotation()
            * MountToAttachmentRoot.GetRotation().Inverse(),
        FVector::ZeroVector,
        ExistingAttachmentTransform.GetScale3D());
    const FVector MountLocationWithZeroRoot =
        (MountToAttachmentRoot * AttachmentTransform).GetLocation();
    AttachmentTransform.SetLocation(
        AnchorRelativeTransform.GetLocation() - MountLocationWithZeroRoot);
    OutAttachmentTransform = AttachmentTransform;
    return true;
}

bool TryGetLegReferenceLengths(
    const USkeletalMeshComponent& PartMesh,
    float& OutUpperLength,
    float& OutLowerLength
)
{
    FTransform ThighTransform = FTransform::Identity;
    FTransform CalfTransform = FTransform::Identity;
    FTransform FootTransform = FTransform::Identity;
    if (!TryGetTestBodyReferenceComponentTransform(
            PartMesh,
            LegThighBoneName,
            ThighTransform)
        || !TryGetTestBodyReferenceComponentTransform(
            PartMesh,
            LegCalfBoneName,
            CalfTransform)
        || !TryGetTestBodyReferenceComponentTransform(
            PartMesh,
            LegFootBoneName,
            FootTransform))
    {
        return false;
    }

    OutUpperLength = FVector::Distance(
        ThighTransform.GetLocation(),
        CalfTransform.GetLocation());
    OutLowerLength = FVector::Distance(
        CalfTransform.GetLocation(),
        FootTransform.GetLocation());
    return OutUpperLength > UE_SMALL_NUMBER
        && OutLowerLength > UE_SMALL_NUMBER;
}

void AlignPartPreviewMountToSlotAnchor(
    UChildActorComponent& Preview,
    const UCMPartSlotComponent& PartSlot
)
{
    ACMPartActorBase* Part = Cast<ACMPartActorBase>(Preview.GetChildActor());
    if (!Part || !Part->GetPartMesh() || !Part->GetRootComponent())
    {
        return;
    }

    const ECMPartSlotType PartType =
        ICMPartInterface::Execute_GetPartType(Part);
    USceneComponent* Anchor = ResolveTestBodyRigAnchor(PartSlot, PartType);
    if (!Anchor)
    {
        return;
    }

    FName MountBone = NAME_None;
    bool bUsesMirroredLeftChain = false;
    if (!ResolveTestBodyMountBoneName(
            *Part->GetPartMesh(),
            PartSlot,
            PartType,
            MountBone,
            bUsesMirroredLeftChain))
    {
        return;
    }

    const FTransform MeshToPreview = Part->GetPartMesh()->GetRelativeTransform()
        * Part->GetRootComponent()->GetRelativeTransform();
    FTransform PreviewTransform = FTransform::Identity;
    const FTransform AnchorRelativeTransform =
        Anchor->GetComponentTransform().GetRelativeTransform(
            PartSlot.GetComponentTransform());
    if (TryCalculatePartAttachmentTransform(
            *Part->GetPartMesh(),
            MountBone,
            MeshToPreview,
            AnchorRelativeTransform,
            Preview.GetRelativeTransform(),
            PreviewTransform))
    {
        Preview.SetRelativeTransform(PreviewTransform);
    }
}

}

ACMProceduralAnimationTestBody::ACMProceduralAnimationTestBody()
{
    constexpr int32 TestBodySegmentCount = 8;
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    InputPriority = 100;

    PhysicsBody = CreateDefaultSubobject<UBoxComponent>(TEXT("PhysicsBody"));
    SetRootComponent(PhysicsBody);
    PhysicsBody->SetBoxExtent(FVector(
        BodyCollisionHalfLength,
        BodyCollisionHalfWidth,
        BodyCollisionHalfHeight
    ));
    PhysicsBody->SetCollisionProfileName(TEXT("PhysicsActor"));
    PhysicsBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PhysicsBody->SetGenerateOverlapEvents(false);
    // Construction may place the centered box partially inside the floor.
    // Physics is enabled in BeginPlay only after that overlap is resolved.
    PhysicsBody->SetSimulatePhysics(false);
    PhysicsBody->SetEnableGravity(bEnableBodyGravity);
    PhysicsBody->SetLinearDamping(BodyLinearDamping);
    PhysicsBody->SetAngularDamping(BodyAngularDamping);
    BodySegments.Add(PhysicsBody);

    for (int32 Index = 1; Index < TestBodySegmentCount; ++Index)
    {
        UBoxComponent* SegmentBody = CreateDefaultSubobject<UBoxComponent>(
            *FString::Printf(TEXT("BodySegment_%02d"), Index + 1)
        );
        SegmentBody->SetupAttachment(PhysicsBody);
        SegmentBody->SetBoxExtent(FVector(
            BodyCollisionHalfLength,
            BodyCollisionHalfWidth,
            BodyCollisionHalfHeight
        ));
        SegmentBody->SetCollisionProfileName(TEXT("PhysicsActor"));
        SegmentBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        SegmentBody->SetGenerateOverlapEvents(false);
        SegmentBody->SetSimulatePhysics(false);
        SegmentBody->SetEnableGravity(bEnableBodyGravity);
        SegmentBody->SetLinearDamping(BodyLinearDamping);
        SegmentBody->SetAngularDamping(BodyAngularDamping);
        BodySegments.Add(SegmentBody);
    }

    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
    VisualRoot->SetupAttachment(PhysicsBody);

    TopViewCamera = CreateDefaultSubobject<UCameraComponent>(
        TEXT("TopViewCamera")
    );
    TopViewCamera->SetupAttachment(PhysicsBody);
    TopViewCamera->SetUsingAbsoluteLocation(true);
    TopViewCamera->SetUsingAbsoluteRotation(true);
    TopViewCamera->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
    TopViewCamera->SetFieldOfView(TopViewCameraFieldOfView);

    BodyControlTarget = CreateDefaultSubobject<USceneComponent>(
        TEXT("BodyControlTarget")
    );
    BodyControlTarget->SetupAttachment(PhysicsBody);

    for (int32 Index = 0; Index < BodySegments.Num(); ++Index)
    {
        const FName LeftSlotName = Index == 0
            ? TEXT("LeftPartSlot")
            : *FString::Printf(TEXT("LeftPartSlot_%02d"), Index + 1);
        UCMPartSlotComponent* SegmentLeftSlot =
            CreateDefaultSubobject<UCMPartSlotComponent>(LeftSlotName);
        SegmentLeftSlot->SetupAttachment(BodySegments[Index]);
        SegmentLeftSlot->SetRelativeLocation(FVector(
            0.0,
            -BodySlotLateralOffset,
            -BodyCollisionHalfHeight + InitialGroundClearance
        ));
        SegmentLeftSlot->InitializeSlotAddress(Index, 0);
        LeftPartSlots.Add(SegmentLeftSlot);
        PartSlots.Add(SegmentLeftSlot);

        const FName LeftRigAnchorName = Index == 0
            ? TEXT("LeftLegRigAnchor")
            : *FString::Printf(TEXT("LeftLegRigAnchor_%02d"), Index + 1);
        USceneComponent* SegmentLeftRigAnchor =
            CreateDefaultSubobject<USceneComponent>(LeftRigAnchorName);
        SegmentLeftRigAnchor->SetupAttachment(SegmentLeftSlot);
        SegmentLeftRigAnchor->SetRelativeLocation(FVector(
            0.0,
            BodySlotLateralOffset,
            BodyCollisionHalfHeight - InitialGroundClearance));
        SegmentLeftRigAnchor->bEditableWhenInherited = true;
        SegmentLeftSlot->SetLegRigControlAnchor(SegmentLeftRigAnchor);
        LegRigControlAnchors.Add(SegmentLeftRigAnchor);

        const FName LeftArmRigAnchorName = Index == 0
            ? TEXT("LeftArmRigAnchor")
            : *FString::Printf(TEXT("LeftArmRigAnchor_%02d"), Index + 1);
        USceneComponent* SegmentLeftArmRigAnchor =
            CreateDefaultSubobject<USceneComponent>(LeftArmRigAnchorName);
        SegmentLeftArmRigAnchor->SetupAttachment(SegmentLeftSlot);
        SegmentLeftArmRigAnchor->SetRelativeLocationAndRotation(
            FVector::ZeroVector,
            FRotator::ZeroRotator);
        SegmentLeftArmRigAnchor->bEditableWhenInherited = true;
        SegmentLeftSlot->SetArmRigControlAnchor(SegmentLeftArmRigAnchor);
        ArmRigControlAnchors.Add(SegmentLeftArmRigAnchor);
        if (Index == 0)
        {
            LeftPartSlot = SegmentLeftSlot;
        }

        const FName RightSlotName = Index == 0
            ? TEXT("RightPartSlot")
            : *FString::Printf(TEXT("RightPartSlot_%02d"), Index + 1);
        UCMPartSlotComponent* SegmentRightSlot =
            CreateDefaultSubobject<UCMPartSlotComponent>(RightSlotName);
        SegmentRightSlot->SetupAttachment(BodySegments[Index]);
        SegmentRightSlot->SetRelativeLocation(FVector(
            0.0,
            BodySlotLateralOffset,
            -BodyCollisionHalfHeight + InitialGroundClearance
        ));
        SegmentRightSlot->InitializeSlotAddress(Index, 1);
        RightPartSlots.Add(SegmentRightSlot);
        PartSlots.Add(SegmentRightSlot);


        const FName RightRigAnchorName = Index == 0
            ? TEXT("RightLegRigAnchor")
            : *FString::Printf(TEXT("RightLegRigAnchor_%02d"), Index + 1);
        USceneComponent* SegmentRightRigAnchor =
            CreateDefaultSubobject<USceneComponent>(RightRigAnchorName);
        SegmentRightRigAnchor->SetupAttachment(SegmentRightSlot);
        SegmentRightRigAnchor->SetRelativeLocation(FVector(
            0.0,
            -BodySlotLateralOffset,
            BodyCollisionHalfHeight - InitialGroundClearance));
        SegmentRightRigAnchor->bEditableWhenInherited = true;
        SegmentRightSlot->SetLegRigControlAnchor(SegmentRightRigAnchor);
        LegRigControlAnchors.Add(SegmentRightRigAnchor);

        const FName RightArmRigAnchorName = Index == 0
            ? TEXT("RightArmRigAnchor")
            : *FString::Printf(TEXT("RightArmRigAnchor_%02d"), Index + 1);
        USceneComponent* SegmentRightArmRigAnchor =
            CreateDefaultSubobject<USceneComponent>(RightArmRigAnchorName);
        SegmentRightArmRigAnchor->SetupAttachment(SegmentRightSlot);
        SegmentRightArmRigAnchor->SetRelativeLocationAndRotation(
            FVector::ZeroVector,
            FRotator::ZeroRotator);
        SegmentRightArmRigAnchor->bEditableWhenInherited = true;
        SegmentRightSlot->SetArmRigControlAnchor(SegmentRightArmRigAnchor);
        ArmRigControlAnchors.Add(SegmentRightArmRigAnchor);
        if (Index == 0)
        {
            RightPartSlot = SegmentRightSlot;
        }

        const FName LeftPreviewName = Index == 0
            ? TEXT("LeftPartPreview")
            : *FString::Printf(TEXT("LeftPartPreview_%02d"), Index + 1);
        UChildActorComponent* SegmentLeftPreview =
            CreateDefaultSubobject<UChildActorComponent>(LeftPreviewName);
        SegmentLeftPreview->SetupAttachment(SegmentLeftSlot);
        LeftPartPreviews.Add(SegmentLeftPreview);
        PartPreviews.Add(SegmentLeftPreview);
        if (Index == 0)
        {
            LeftPartPreview = SegmentLeftPreview;
        }

        const FName RightPreviewName = Index == 0
            ? TEXT("RightPartPreview")
            : *FString::Printf(TEXT("RightPartPreview_%02d"), Index + 1);
        UChildActorComponent* SegmentRightPreview =
            CreateDefaultSubobject<UChildActorComponent>(RightPreviewName);
        SegmentRightPreview->SetupAttachment(SegmentRightSlot);
        SegmentRightPreview->SetRelativeScale3D(RightPartRelativeScale);
        RightPartPreviews.Add(SegmentRightPreview);
        PartPreviews.Add(SegmentRightPreview);
        if (Index == 0)
        {
            RightPartPreview = SegmentRightPreview;
        }
    }

    static ConstructorHelpers::FClassFinder<ACMPartActorBase> DefaultLegClass(
        TEXT("/Game/Chimera/Character/Part/Leg/BP_CMLegPart")
    );
    if (DefaultLegClass.Succeeded())
    {
        LeftPartClass = DefaultLegClass.Class;
        RightPartClass = DefaultLegClass.Class;
        for (UChildActorComponent* Preview : LeftPartPreviews)
        {
            Preview->SetChildActorClass(LeftPartClass);
        }
        for (UChildActorComponent* Preview : RightPartPreviews)
        {
            Preview->SetChildActorClass(RightPartClass);
        }
    }

    static ConstructorHelpers::FClassFinder<ACMPartActorBase> DefaultArmClass(
        TEXT("/Game/Chimera/Character/Part/Arm/BP_CMArmPart")
    );
    if (DefaultArmClass.Succeeded())
    {
        ArmPartClass = DefaultArmClass.Class;
    }

    static ConstructorHelpers::FClassFinder<ACMPartActorBase> DefaultHeadClass(
        TEXT("/Game/Chimera/Character/Part/Head/BluePrint/"
            "BP_CMHead01HeadPart")
    );
    if (DefaultHeadClass.Succeeded())
    {
        HeadPartClass = DefaultHeadClass.Class;
    }
}

void ACMProceduralAnimationTestBody::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);
    RefreshBodyAssembly();
    RefreshPartPreviews();
}

void ACMProceduralAnimationTestBody::PreInitializeComponents()
{
    // Preview children are editor authoring aids and must not enter gameplay.
    DestroyPartPreviews();
    Super::PreInitializeComponents();
}

void ACMProceduralAnimationTestBody::BeginPlay()
{
    Super::BeginPlay();

    DestroyPartPreviews();
    UpdateTopViewCamera();
    ActivateTopViewCamera();

    if (!HasAuthority())
    {
        for (UBoxComponent* SegmentBody : BodySegments)
        {
            if (SegmentBody)
            {
                SegmentBody->SetSimulatePhysics(false);
            }
        }
        return;
    }

    ResolveInitialGroundPenetration();
    for (UBoxComponent* SegmentBody : BodySegments)
    {
        if (!SegmentBody)
        {
            continue;
        }
        SegmentBody->SetEnableGravity(bEnableBodyGravity);
        SegmentBody->SetSimulatePhysics(true);
        SegmentBody->SetLinearDamping(BodyLinearDamping);
        SegmentBody->SetAngularDamping(BodyAngularDamping);
        SegmentBody->SetMassOverrideInKg(
            NAME_None,
            FMath::Max(SimulatedBodyMassKg
                / FMath::Max(static_cast<float>(BodySegments.Num()), 1.0f),
                1.0f),
            true
        );
        ConfigureBodyRotationLock(SegmentBody);
        SegmentBody->WakeAllRigidBodies();
    }
    ConfigureSegmentConstraints();

    if (bSpawnConfiguredPartsOnBeginPlay)
    {
        RespawnConfiguredParts();
    }
    BindManualArmTestInput();
}

void ACMProceduralAnimationTestBody::Destroyed()
{
    DestroyManualArmGroundConstraints();
    if (HasAuthority())
    {
        ClearSpawnedParts();
    }
    DestroySegmentConstraints();

    Super::Destroyed();
}

void ACMProceduralAnimationTestBody::SetOneSidedLeanTestMode(
    const bool bEnabled
)
{
    bSimulateOneSidedBodyLean = bEnabled;
    if (HasActorBegunPlay())
    {
        RefreshBodyAssembly();
        for (UBoxComponent* SegmentBody : BodySegments)
        {
            ConfigureBodyRotationLock(SegmentBody);
        }
        ConfigureSegmentConstraints();
    }
}

int32 ACMProceduralAnimationTestBody::GetSimulatedPlantedLegCount() const
{
    int32 PlantedCount = 0;
    for (const UCMPartSlotComponent* PartSlot : PartSlots)
    {
        const ACMLegPart* LegPart = PartSlot
            ? Cast<ACMLegPart>(PartSlot->GetAttachedPart())
            : nullptr;
        if (LegPart && LegPart->GetPlantState()
            == ECMLegPlantState::Planted)
        {
            ++PlantedCount;
        }
    }
    return PlantedCount;
}

int32 ACMProceduralAnimationTestBody::GetSimulatedOperationalLegCount() const
{
    int32 OperationalCount = 0;
    for (const UCMPartSlotComponent* PartSlot : PartSlots)
    {
        const ACMLegPart* LegPart = PartSlot
            ? Cast<ACMLegPart>(PartSlot->GetAttachedPart())
            : nullptr;
        if (LegPart && LegPart->IsOperational())
        {
            ++OperationalCount;
        }
    }
    return OperationalCount;
}

int32 ACMProceduralAnimationTestBody::GetSimulatedOperationalArmCount() const
{
    int32 OperationalCount = 0;
    for (const UCMPartSlotComponent* PartSlot : PartSlots)
    {
        const ACMArmPart* ArmPart = PartSlot
            ? Cast<ACMArmPart>(PartSlot->GetAttachedPart())
            : nullptr;
        if (ArmPart && ArmPart->IsOperational())
        {
            ++OperationalCount;
        }
    }
    return OperationalCount;
}

void ACMProceduralAnimationTestBody::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateTopViewCamera();
    ActivateTopViewCamera();

    if (HasAuthority())
    {
        UpdateMouseAimedHeads();
    }

    if (!HasAuthority() || !bSimulatePartInputEveryTick)
    {
        return;
    }

    AdvanceSimulatedMovementCycle(DeltaSeconds);
    if (bSimulateOneSidedBodyLean)
    {
        ApplySimulatedOneSidedLean(DeltaSeconds);
    }
    if (!bSimulatedInputPaused)
    {
        AdvanceSimulatedInputPhase(DeltaSeconds);
    }

    // Active Part actions continue independently of new button attempts,
    // matching ACMChimera's movement coordinator.
    for (UCMPartSlotComponent* PartSlot : PartSlots)
    {
        UpdateSimulatedPartAction(PartSlot, DeltaSeconds);
    }
    UpdateSimulatedLegReachRecovery();

    if (bSimulatedInputPaused)
    {
        SimulatePausedClockwiseRotation();
        ApplySimulatedPauseBraking(DeltaSeconds);
        return;
    }

    // Four players each own four keys. Players 0-1 own the left side and
    // players 2-3 own the right side, exactly like the four-player assignment
    // policy. Advance one player per Tick so leg activations are visibly
    // staggered instead of all four feet starting in the same frame.
    constexpr int32 SimulatedPlayerCount = 4;
    constexpr int32 KeysPerPlayer = 4;
    SimulatedCooperationClock += FMath::Max(DeltaSeconds, 0.0f);
    const int32 PlayerIndex = SimulatedInputPlayerCursor;
    SimulatedInputPlayerCursor = (SimulatedInputPlayerCursor + 1)
        % SimulatedPlayerCount;
    {
        const int32 KeyIndex = SimulatedPlayerKeyIndices[PlayerIndex];
        UCMPartSlotComponent* PartSlot = GetSimulatedPlayerPartSlot(
            PlayerIndex,
            KeyIndex
        );
        if (!PartSlot)
        {
            return;
        }

        // Arm Parts are controlled exclusively by the local Q/W/E/R test
        // input. Advance past them without synthesizing an Arm button press.
        if (Cast<ACMArmPart>(PartSlot->GetAttachedPart()))
        {
            SimulatedPlayerKeyIndices[PlayerIndex] =
                (KeyIndex + 1) % KeysPerPlayer;
            return;
        }

        const bool bRightSidePlayer = PlayerIndex >= 2;
        const bool bReverseMovement = bSimulatedTurnActive
            && ((SimulatedTurnSign < 0 && !bRightSidePlayer)
                || (SimulatedTurnSign > 0 && bRightSidePlayer));
        ++SimulatedButtonPressCount;
        if (bReverseMovement)
        {
            ++SimulatedReverseButtonPressCount;
        }
        if (SimulatePartButtonPress(PartSlot, bReverseMovement))
        {
            ++SimulatedAcceptedInputCount;
            SimulatedPlayerKeyIndices[PlayerIndex] =
                (KeyIndex + 1) % KeysPerPlayer;

            const ACMLegPart* LegPart = Cast<ACMLegPart>(
                PartSlot->GetAttachedPart()
            );
            if (!LegPart)
            {
                return;
            }

            RegisterSimulatedCooperativeInput(
                !bRightSidePlayer,
                LegPart->GetMovementImpulse()
                    * FMath::Max(SimulatedLegForceScale, 0.0f),
                bReverseMovement
            );
        }
    }
}

void ACMProceduralAnimationTestBody::UpdateTopViewCamera()
{
    if (!TopViewCamera)
    {
        return;
    }

    FVector BodyCentre = FVector::ZeroVector;
    int32 ValidSegmentCount = 0;
    for (const UBoxComponent* SegmentBody : BodySegments)
    {
        if (IsValid(SegmentBody))
        {
            BodyCentre += SegmentBody->GetComponentLocation();
            ++ValidSegmentCount;
        }
    }
    if (ValidSegmentCount > 0)
    {
        BodyCentre /= static_cast<float>(ValidSegmentCount);
    }
    else
    {
        BodyCentre = GetActorLocation();
    }

    TopViewCamera->SetWorldLocation(
        BodyCentre + FVector::UpVector
            * FMath::Max(TopViewCameraHeight, 100.0f));
    TopViewCamera->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
    TopViewCamera->SetFieldOfView(FMath::Clamp(
        TopViewCameraFieldOfView,
        5.0f,
        170.0f));
}

void ACMProceduralAnimationTestBody::ActivateTopViewCamera()
{
    if (!bAutoActivateTopViewCamera || bTopViewCameraActivated)
    {
        return;
    }

    APlayerController* PlayerController = GetWorld()
        ? GetWorld()->GetFirstPlayerController()
        : nullptr;
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        return;
    }

    TopViewCamera->SetActive(true);
    PlayerController->SetViewTargetWithBlend(this, 0.0f);
    PlayerController->bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    PlayerController->SetInputMode(InputMode);
    bTopViewCameraActivated = PlayerController->GetViewTarget() == this;
}

void ACMProceduralAnimationTestBody::UpdateMouseAimedHeads()
{
    APlayerController* PlayerController = GetWorld()
        ? GetWorld()->GetFirstPlayerController()
        : nullptr;
    FVector MouseRayOrigin;
    FVector MouseRayDirection;
    if (!PlayerController
        || !PlayerController->IsLocalController()
        || !PlayerController->DeprojectMousePositionToWorld(
            MouseRayOrigin,
            MouseRayDirection)
        || FMath::IsNearlyZero(MouseRayDirection.Z))
    {
        return;
    }

    const float MaximumYaw = FMath::Clamp(
        SimulatedHeadYaw,
        0.0f,
        89.0f);
    for (UCMPartSlotComponent* PartSlot : PartSlots)
    {
        ACMHeadPartActor* HeadPart = PartSlot
            ? Cast<ACMHeadPartActor>(PartSlot->GetAttachedPart())
            : nullptr;
        if (!IsValid(HeadPart) || !HeadPart->IsOperational())
        {
            continue;
        }

        const FVector HeadLocation = HeadPart->GetActorLocation();
        const float RayDistance = (HeadLocation.Z - MouseRayOrigin.Z)
            / MouseRayDirection.Z;
        if (RayDistance <= 0.0f)
        {
            continue;
        }

        FVector AimDirection = MouseRayOrigin
            + MouseRayDirection * RayDistance
            - HeadLocation;
        AimDirection.Z = 0.0f;
        if (!AimDirection.Normalize())
        {
            continue;
        }

        const USceneComponent* BodySegment = PartSlot->GetAttachParent();
        FVector ReferenceForward = BodySegment
            ? BodySegment->GetForwardVector()
            : HeadPart->GetActorForwardVector();
        ReferenceForward.Z = 0.0f;
        ReferenceForward.Normalize();
        const float RelativeYaw = FMath::Clamp(
            FMath::FindDeltaAngleDegrees(
                ReferenceForward.Rotation().Yaw,
                AimDirection.Rotation().Yaw),
            -MaximumYaw,
            MaximumYaw);
        HeadPart->SetProceduralLookRotation(FRotator(
            0.0f,
            RelativeYaw,
            0.0f));
    }
}

void ACMProceduralAnimationTestBody::ApplySimulatedOneSidedLean(
    const float DeltaSeconds
)
{
    const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
    const float TargetRollRadians = FMath::DegreesToRadians(
        FMath::Clamp(SimulatedOneSidedLeanSide, -1.0f, 1.0f)
        * FMath::Clamp(SimulatedOneSidedLeanAngleDegrees, 0.0f, 80.0f)
    );
    const float MaxAngularSpeedRadians = FMath::DegreesToRadians(
        FMath::Max(SimulatedOneSidedLeanMaxAngularSpeedDegrees, 0.0f)
    );
    const float Response = FMath::Max(SimulatedOneSidedLeanResponse, 0.0f);
    if (MaxAngularSpeedRadians <= UE_SMALL_NUMBER || Response <= 0.0f)
    {
        return;
    }

    for (UBoxComponent* SegmentBody : BodySegments)
    {
        if (!SegmentBody || !SegmentBody->IsSimulatingPhysics())
        {
            continue;
        }

        FRotator SegmentRotation = SegmentBody->GetComponentRotation();
        const float CurrentRollDegrees = FRotator::NormalizeAxis(
            SegmentRotation.Roll
        );
        const float CurrentRollRadians = FMath::DegreesToRadians(
            CurrentRollDegrees
        );
        const float RollErrorRadians = FMath::FindDeltaAngleRadians(
            CurrentRollRadians,
            TargetRollRadians
        );
        const FVector RollAxis = SegmentBody->GetForwardVector();
        FVector AngularVelocity =
            SegmentBody->GetPhysicsAngularVelocityInRadians();
        const float CurrentRollSpeed = FVector::DotProduct(
            AngularVelocity,
            RollAxis
        );
        const float DesiredRollSpeed = FMath::Clamp(
            RollErrorRadians * Response,
            -MaxAngularSpeedRadians,
            MaxAngularSpeedRadians
        );
        AngularVelocity += RollAxis * FMath::Clamp(
            DesiredRollSpeed - CurrentRollSpeed,
            -MaxAngularSpeedRadians * FMath::Max(DeltaSeconds, 0.0f),
            MaxAngularSpeedRadians * FMath::Max(DeltaSeconds, 0.0f)
        );
        SegmentBody->SetPhysicsAngularVelocityInRadians(
            AngularVelocity,
            false
        );

        // A transient automation world may not integrate Chaos bodies between
        // the explicit actor Tick and World::Tick. Apply the same bounded
        // servo pose directly so the diagnostic still exercises the actual
        // slot transforms and planted-foot reach window. This path is opt-in;
        // normal gameplay keeps fully physics-driven body motion.
        const float MaxRollStepDegrees = FMath::RadiansToDegrees(
            MaxAngularSpeedRadians * SafeDeltaSeconds
        );
        const float TargetRollDegrees = FMath::RadiansToDegrees(
            TargetRollRadians
        );
        SegmentRotation.Roll = FMath::FixedTurn(
            CurrentRollDegrees,
            TargetRollDegrees,
            MaxRollStepDegrees
        );
        SegmentBody->SetWorldRotation(
            SegmentRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
    }

    // Keep the rotated collision boxes just clear of the floor. Without this
    // small diagnostic lift, a box tilted around its centre intersects the
    // floor at its lower corner and Chaos can produce a launch impulse.
    if (InitialGroundClearance <= 0.0f)
    {
        return;
    }
    float RequiredLift = 0.0f;
    const float AbsRollRadians = FMath::Abs(TargetRollRadians);
    for (const UBoxComponent* SegmentBody : BodySegments)
    {
        if (!SegmentBody || !SegmentBody->IsSimulatingPhysics())
        {
            continue;
        }

        const FVector Extent = SegmentBody->GetScaledBoxExtent();
        const float RotatedHalfHeight = Extent.Z
            * FMath::Abs(FMath::Cos(AbsRollRadians))
            + Extent.Y * FMath::Abs(FMath::Sin(AbsRollRadians));
        FHitResult GroundHit;
        const FVector ProbePoint = SegmentBody->GetComponentLocation()
            - FVector::UpVector * Extent.Z;
        const float GroundZ = TraceGroundAtPoint(
            ProbePoint,
            nullptr,
            GroundHit
        ) ? GroundHit.ImpactPoint.Z : 0.0f;
        RequiredLift = FMath::Max(
            RequiredLift,
            GroundZ + RotatedHalfHeight + InitialGroundClearance
                - SegmentBody->GetComponentLocation().Z
        );
    }
    if (RequiredLift <= 0.0f)
    {
        return;
    }
    for (UBoxComponent* SegmentBody : BodySegments)
    {
        if (!SegmentBody || !SegmentBody->IsSimulatingPhysics())
        {
            continue;
        }
        SegmentBody->SetWorldLocation(
            SegmentBody->GetComponentLocation()
                + FVector::UpVector * RequiredLift,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
    }
}

void ACMProceduralAnimationTestBody::ApplySimulatedPauseBraking(
    const float DeltaSeconds
)
{
    const float BrakingRate = FMath::Max(
        SimulatedPausePlanarBraking,
        0.0f
    );
    if (BrakingRate <= UE_SMALL_NUMBER)
    {
        return;
    }

    const float PlanarVelocityRetention = FMath::Exp(
        -BrakingRate * FMath::Max(DeltaSeconds, 0.0f)
    );
    for (UBoxComponent* SegmentBody : BodySegments)
    {
        if (!SegmentBody || !SegmentBody->IsSimulatingPhysics())
        {
            continue;
        }

        FVector LinearVelocity = SegmentBody->GetPhysicsLinearVelocity();
        LinearVelocity.X *= PlanarVelocityRetention;
        LinearVelocity.Y *= PlanarVelocityRetention;
        SegmentBody->SetPhysicsLinearVelocity(LinearVelocity, false);
    }
}

void ACMProceduralAnimationTestBody::SimulatePausedClockwiseRotation()
{
    const int32 TurnSegmentCount =
        SimulatedPauseTurnSegmentIndices.Num();
    if (TurnSegmentCount <= 0)
    {
        return;
    }

    const int32 CallsPerCycle = TurnSegmentCount * 2;
    SimulatedPauseTurnStepCursor = FMath::Clamp(
        SimulatedPauseTurnStepCursor,
        0,
        CallsPerCycle - 1
    );
    const int32 StepIndex = SimulatedPauseTurnStepCursor;
    SimulatedPauseTurnStepCursor = (StepIndex + 1) % CallsPerCycle;

    const int32 SegmentIndex =
        SimulatedPauseTurnSegmentIndices[StepIndex / 2];
    if (!BodySegments.IsValidIndex(SegmentIndex))
    {
        return;
    }

    // Positive yaw is clockwise when viewed from above: push the authored
    // left leg forward and the mirrored right leg backward. Calling one side
    // per Tick keeps the six selected legs visibly sequential.
    const bool bRightSide = (StepIndex % 2) != 0;
    UCMPartSlotComponent* PartSlot = bRightSide
        ? (RightPartSlots.IsValidIndex(SegmentIndex)
            ? RightPartSlots[SegmentIndex]
            : nullptr)
        : (LeftPartSlots.IsValidIndex(SegmentIndex)
            ? LeftPartSlots[SegmentIndex]
            : nullptr);
    if (!IsValid(PartSlot))
    {
        return;
    }

    ++SimulatedPauseTurnButtonPressCount;
    if (SimulatePartButtonPress(PartSlot, bRightSide))
    {
        ++SimulatedPauseTurnAcceptedInputCount;
    }
}

void ACMProceduralAnimationTestBody::StopSimulatedNonTurnLegActions()
{
    for (UCMPartSlotComponent* PartSlot : PartSlots)
    {
        if (!IsValid(PartSlot))
        {
            continue;
        }
        const int32 SegmentIndex = PartSlot->GetSlotAddress().SegmentIndex;
        if (SimulatedPauseTurnSegmentIndices.Contains(SegmentIndex))
        {
            continue;
        }

        ACMLegPart* LegPart = Cast<ACMLegPart>(
            PartSlot->GetAttachedPart());
        if (!IsValid(LegPart))
        {
            continue;
        }
        SimulatedLegForceActions.Remove(LegPart);
        if (LegPart->GetStepDirection() != ECMLegStepDirection::None)
        {
            LegPart->CancelProceduralStep(
                ECMLegPlantTrigger::Emergency
            );
        }
    }
}

void ACMProceduralAnimationTestBody::AdvanceSimulatedMovementCycle(
    const float DeltaSeconds
)
{
    SimulatedMovementCycleClock += FMath::Max(DeltaSeconds, 0.0f);
    const float PhaseDuration = bSimulatedInputPaused
        ? FMath::Max(SimulatedInputPauseDuration, 0.0f)
        : FMath::Max(SimulatedInputMoveDuration, 0.01f);
    if (SimulatedMovementCycleClock < PhaseDuration)
    {
        return;
    }

    SimulatedMovementCycleClock = 0.0f;
    bSimulatedInputPaused = !bSimulatedInputPaused;
    if (bSimulatedInputPaused)
    {
        // Inputs waiting for an opposite-side partner must not survive the
        // observation pause and fire when the next movement phase begins.
        PendingLeftCooperativeInputs.Reset();
        PendingRightCooperativeInputs.Reset();
        SimulatedPauseTurnStepCursor = 0;
        StopSimulatedNonTurnLegActions();
    }
}

void ACMProceduralAnimationTestBody::AdvanceSimulatedInputPhase(
    const float DeltaSeconds
)
{
    const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
    SimulatedTurnClock += SafeDeltaSeconds;
    if (bSimulatedTurnActive)
    {
        if (SimulatedTurnClock >= SimulatedTurnDuration)
        {
            SimulatedTurnClock = 0.0f;
            bSimulatedTurnActive = false;
            SimulatedTurnSign *= -1;
        }
    }
    else if (SimulatedTurnClock >= SimulatedTurnDelay)
    {
        SimulatedTurnClock = 0.0f;
        bSimulatedTurnActive = true;
    }
}

void ACMProceduralAnimationTestBody::BindManualArmTestInput()
{
    APlayerController* PlayerController = GetWorld()
        ? GetWorld()->GetFirstPlayerController()
        : nullptr;
    if (!PlayerController)
    {
        return;
    }

    EnableInput(PlayerController);
    if (!InputComponent)
    {
        return;
    }

    for (const FKey& Key : ManualArmControlKeys)
    {
        InputComponent->BindKey(
            Key,
            IE_Pressed,
            this,
            &ThisClass::ManualArmControlKeyPressed);
        InputComponent->BindKey(
            Key,
            IE_Released,
            this,
            &ThisClass::ManualArmControlKeyReleased);
    }
}

UCMPartSlotComponent*
ACMProceduralAnimationTestBody::GetManualArmPartSlot(
    const int32 ArmIndex
) const
{
    if (ArmIndex < 0)
    {
        return nullptr;
    }

    const int32 SegmentListIndex = ArmIndex / 2;
    if (!SimulatedArmSegmentIndices.IsValidIndex(SegmentListIndex))
    {
        return nullptr;
    }

    const int32 SegmentIndex =
        SimulatedArmSegmentIndices[SegmentListIndex];
    const bool bRightSide = (ArmIndex % 2) != 0;
    const TArray<TObjectPtr<UCMPartSlotComponent>>& SideSlots =
        bRightSide ? RightPartSlots : LeftPartSlots;
    return SideSlots.IsValidIndex(SegmentIndex)
        ? SideSlots[SegmentIndex]
        : nullptr;
}

void ACMProceduralAnimationTestBody::ManualArmControlKeyPressed(
    const FKey Key
)
{
    const int32 ArmIndex = FindManualArmControlKeyIndex(Key);
    UCMPartSlotComponent* PartSlot = GetManualArmPartSlot(ArmIndex);
    ACMArmPart* ArmPart = PartSlot
        ? Cast<ACMArmPart>(PartSlot->GetAttachedPart())
        : nullptr;
    if (IsValid(ArmPart) && ArmPart->IsOperational())
    {
        TryBeginManualArmHold(*ArmPart, *PartSlot);
    }
}

void ACMProceduralAnimationTestBody::ManualArmControlKeyReleased(
    const FKey Key
)
{
    const int32 ArmIndex = FindManualArmControlKeyIndex(Key);
    UCMPartSlotComponent* PartSlot = GetManualArmPartSlot(ArmIndex);
    ACMArmPart* ArmPart = PartSlot
        ? Cast<ACMArmPart>(PartSlot->GetAttachedPart())
        : nullptr;
    if (!IsValid(ArmPart) || !ArmPart->IsOperational())
    {
        return;
    }

    if (ArmPart->IsHolding())
    {
        EndManualArmHold(*ArmPart);
    }
    if (!ArmPart->IsSwinging())
    {
        ArmPart->BeginSwing();
    }
}

bool ACMProceduralAnimationTestBody::TryBeginManualArmHold(
    ACMArmPart& ArmPart,
    const UCMPartSlotComponent& PartSlot
)
{
    if (ArmPart.IsHolding())
    {
        return true;
    }
    if (ArmPart.IsSwinging())
    {
        ArmPart.EndSwing();
    }

    // Build the hold target from the authored Arm shoulder mount. This keeps
    // gameplay tracing aligned with the same anchor used to remove the source-
    // character pivot during attachment, even when designers offset the Arm
    // anchor independently from the generic Part slot.
    const USceneComponent* ArmMountAnchor = PartSlot.GetArmRigControlAnchor();
    FVector GroundTracePoint = ArmMountAnchor
        ? ArmMountAnchor->GetComponentLocation()
        : PartSlot.GetComponentLocation();
    const USceneComponent* BodySegment = PartSlot.GetAttachParent();
    if (BodySegment)
    {
        FVector OutwardDirection = GroundTracePoint
            - BodySegment->GetComponentLocation();
        OutwardDirection.Z = 0.0f;
        if (OutwardDirection.Normalize())
        {
            GroundTracePoint += OutwardDirection
                * FMath::Max(ArmPart.GetHoldRange() * 0.65f, 20.0f);
        }
    }

    FHitResult GroundHit;
    if (!TraceGroundAtPoint(GroundTracePoint, &ArmPart, GroundHit))
    {
        UE_LOG(LogCMProceduralAnimationTestBody, Warning,
            TEXT("Manual Arm hold rejected: Key target for %s found no ground at %s."),
            *GetNameSafe(&ArmPart),
            *GroundTracePoint.ToCompactString());
        return false;
    }

    UPrimitiveComponent* SegmentBody = Cast<UPrimitiveComponent>(
        PartSlot.GetAttachParent());
    if (!SegmentBody || !SegmentBody->IsSimulatingPhysics())
    {
        return false;
    }

    UPhysicsConstraintComponent* Constraint =
        NewObject<UPhysicsConstraintComponent>(this);
    if (!Constraint)
    {
        return false;
    }
    AddInstanceComponent(Constraint);
    Constraint->RegisterComponent();
    Constraint->SetWorldLocation(GroundHit.ImpactPoint);
    Constraint->SetDisableCollision(true);
    Constraint->SetLinearXLimit(
        ELinearConstraintMotion::LCM_Locked,
        0.0f);
    Constraint->SetLinearYLimit(
        ELinearConstraintMotion::LCM_Locked,
        0.0f);
    Constraint->SetLinearZLimit(
        ELinearConstraintMotion::LCM_Locked,
        0.0f);
    Constraint->SetAngularSwing1Limit(
        EAngularConstraintMotion::ACM_Free,
        0.0f);
    Constraint->SetAngularSwing2Limit(
        EAngularConstraintMotion::ACM_Free,
        0.0f);
    Constraint->SetAngularTwistLimit(
        EAngularConstraintMotion::ACM_Free,
        0.0f);
    Constraint->SetConstrainedComponents(
        SegmentBody,
        NAME_None,
        nullptr,
        NAME_None);
    ManualArmGroundConstraints.Add(&ArmPart, Constraint);

    ArmPart.BeginGroundAnchor(
        GroundHit.ImpactPoint,
        GroundHit.ImpactNormal);
    return true;
}

void ACMProceduralAnimationTestBody::EndManualArmHold(ACMArmPart& ArmPart)
{
    if (TWeakObjectPtr<UPhysicsConstraintComponent>* ConstraintPtr =
            ManualArmGroundConstraints.Find(&ArmPart))
    {
        if (UPhysicsConstraintComponent* Constraint = ConstraintPtr->Get())
        {
            Constraint->BreakConstraint();
            Constraint->DestroyComponent();
        }
        ManualArmGroundConstraints.Remove(&ArmPart);
    }
    ArmPart.EndGroundAnchor();
}

void ACMProceduralAnimationTestBody::DestroyManualArmGroundConstraints()
{
    for (TPair<TWeakObjectPtr<ACMArmPart>,
            TWeakObjectPtr<UPhysicsConstraintComponent>>& Pair
        : ManualArmGroundConstraints)
    {
        if (UPhysicsConstraintComponent* Constraint = Pair.Value.Get())
        {
            Constraint->BreakConstraint();
            Constraint->DestroyComponent();
        }
    }
    ManualArmGroundConstraints.Reset();
}

void ACMProceduralAnimationTestBody::RespawnConfiguredParts()
{
    if (!HasAuthority())
    {
        return;
    }

    ClearSpawnedParts();
    for (int32 SegmentIndex = 0;
        SegmentIndex < BodySegments.Num();
        ++SegmentIndex)
    {
        UCMPartSlotComponent* LeftSlot = LeftPartSlots.IsValidIndex(
            SegmentIndex) ? LeftPartSlots[SegmentIndex] : nullptr;
        UCMPartSlotComponent* RightSlot = RightPartSlots.IsValidIndex(
            SegmentIndex) ? RightPartSlots[SegmentIndex] : nullptr;
        ACMPartActorBase* SpawnedLeft = SpawnPart(
            ResolvePartClassForSegment(SegmentIndex, false),
            LeftSlot,
            FVector::OneVector
        );
        ACMPartActorBase* SpawnedRight = SpawnPart(
            ResolvePartClassForSegment(SegmentIndex, true),
            RightSlot,
            RightPartRelativeScale
        );
        if (SegmentIndex == 0)
        {
            LeftPart = SpawnedLeft;
            RightPart = SpawnedRight;
        }
        if (SpawnedLeft)
        {
            SpawnedParts.Add(SpawnedLeft);
        }
        if (SpawnedRight)
        {
            SpawnedParts.Add(SpawnedRight);
        }
    }

    // Match the production coordinator's initial contact pass. A leg must
    // start Planted before a one-sided lean can exercise reach recovery.
    for (UCMPartSlotComponent* PartSlot : PartSlots)
    {
        if (!IsValid(PartSlot))
        {
            continue;
        }
        ACMLegPart* LegPart = Cast<ACMLegPart>(
            PartSlot->GetAttachedPart());
        if (!IsValid(LegPart)
            || !LegPart->IsOperational()
            || LegPart->GetPlantState() != ECMLegPlantState::Free)
        {
            continue;
        }

        FHitResult GroundHit;
        if (TraceReachableLegGround(
                *PartSlot,
                *LegPart,
                0.0f,
                GroundHit))
        {
            LegPart->InitializePlantedContact(
                GroundHit.ImpactPoint,
                GroundHit.ImpactNormal
            );
        }
    }
}

void ACMProceduralAnimationTestBody::AddBodyImpulse(
    FVector Impulse,
    bool bVelocityChange
)
{
    if (HasAuthority() && !Impulse.IsNearlyZero())
    {
        for (UBoxComponent* SegmentBody : BodySegments)
        {
            if (SegmentBody && SegmentBody->IsSimulatingPhysics())
            {
                SegmentBody->AddImpulse(
                    Impulse / FMath::Max(BodySegments.Num(), 1),
                    NAME_None,
                    bVelocityChange
                );
            }
        }
    }
}

bool ACMProceduralAnimationTestBody::IsSupportedPartType(
    ECMPartSlotType PartType
)
{
    return PartType == ECMPartSlotType::Head
        || PartType == ECMPartSlotType::Arm
        || PartType == ECMPartSlotType::Leg;
}

ACMPartActorBase* ACMProceduralAnimationTestBody::SpawnPart(
    TSubclassOf<ACMPartActorBase> PartClass,
    UCMPartSlotComponent* PartSlot,
    const FVector& RelativeScale
)
{
    if (!PartClass || !IsValid(PartSlot) || !GetWorld())
    {
        return nullptr;
    }

    ACMPartActorBase* PartDefault = PartClass->GetDefaultObject<
        ACMPartActorBase>();
    const ECMPartSlotType PartType = PartDefault
        ? ICMPartInterface::Execute_GetPartType(PartDefault)
        : ECMPartSlotType::Any;
    if (!IsSupportedPartType(PartType))
    {
        UE_LOG(LogCMProceduralAnimationTestBody, Warning,
            TEXT("Rejected unsupported test Part class %s (type %d)."),
            *GetNameSafe(PartClass),
            static_cast<int32>(PartType));
        return nullptr;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ACMPartActorBase* SpawnedPart = GetWorld()->SpawnActor<
        ACMPartActorBase>(
            PartClass,
            PartSlot->GetComponentTransform(),
            SpawnParameters
        );
    if (!IsValid(SpawnedPart))
    {
        return nullptr;
    }

    // Apply the final side-aware scale before attachment so the mount-bone
    // alignment performed by UCMPartSlotComponent uses the final transform.
    SpawnedPart->SetActorScale3D(
        ResolvePartScaleForSlot(*SpawnedPart, *PartSlot, RelativeScale));
    if (!PartSlot->AttachPart(SpawnedPart))
    {
        SpawnedPart->Destroy();
        return nullptr;
    }

    // Attached Parts are visual/procedural actors on this test body. The box
    // proxy is the sole physical collision shape, avoiding self-depenetration.
    SpawnedPart->SetActorEnableCollision(false);
    return SpawnedPart;
}

void ACMProceduralAnimationTestBody::ClearSpawnedParts()
{
    DestroyManualArmGroundConstraints();
    SimulatedLegReplantPaths.Reset();
    SimulatedLegForceActions.Reset();
    const auto DetachAndDestroy = [](UCMPartSlotComponent* PartSlot)
    {
        if (!IsValid(PartSlot))
        {
            return;
        }

        if (AActor* Part = PartSlot->DetachPart())
        {
            Part->Destroy();
        }
    };

    for (UCMPartSlotComponent* PartSlot : PartSlots)
    {
        DetachAndDestroy(PartSlot);
    }
    SpawnedParts.Reset();
    LeftPart = nullptr;
    RightPart = nullptr;
}

void ACMProceduralAnimationTestBody::RefreshBodyAssembly()
{
    for (int32 Index = 0; Index < BodySegments.Num(); ++Index)
    {
        UBoxComponent* SegmentBody = BodySegments[Index];
        if (!IsValid(SegmentBody))
        {
            continue;
        }

        SegmentBody->SetBoxExtent(FVector(
            BodyCollisionHalfLength,
            BodyCollisionHalfWidth,
            BodyCollisionHalfHeight
        ));
        SegmentBody->SetEnableGravity(bEnableBodyGravity);
        if (Index == 0)
        {
            SegmentBody->SetRelativeLocation(FVector(
                0.0,
                0.0,
                BodyCollisionVerticalOffset
            ));
        }
        else
        {
            SegmentBody->SetRelativeLocation(FVector(
                -BodySegmentSpacing * Index,
                0.0,
                0.0
            ));
        }

        const float SlotZ = -BodyCollisionHalfHeight
            + InitialGroundClearance;
        if (LeftPartSlots.IsValidIndex(Index)
            && LeftPartSlots[Index])
        {
            UCMPartSlotComponent* LeftSlot = LeftPartSlots[Index];
            LeftSlot->SetRelativeLocation(FVector(
                0.0,
                -BodySlotLateralOffset,
                SlotZ
            ));
            if (USceneComponent* Anchor =
                    LeftSlot->GetLegRigControlAnchor())
            {
                Anchor->SetRelativeLocationAndRotation(
                    -LeftSlot->GetRelativeLocation(),
                    FRotator::ZeroRotator);
            }
        }
        if (RightPartSlots.IsValidIndex(Index)
            && RightPartSlots[Index])
        {
            UCMPartSlotComponent* RightSlot = RightPartSlots[Index];
            RightSlot->SetRelativeLocation(FVector(
                0.0,
                BodySlotLateralOffset,
                SlotZ
            ));
            if (USceneComponent* Anchor =
                    RightSlot->GetLegRigControlAnchor())
            {
                Anchor->SetRelativeLocationAndRotation(
                    -RightSlot->GetRelativeLocation(),
                    FRotator::ZeroRotator);
            }
        }
    }
}

void ACMProceduralAnimationTestBody::RefreshPartPreviews()
{
    const auto RefreshPreview = [](
        UChildActorComponent* Preview,
        UCMPartSlotComponent* PartSlot,
        const TSubclassOf<ACMPartActorBase> PartClass,
        const FVector& RelativeScale)
    {
        if (!IsValid(Preview) || !IsValid(PartSlot))
        {
            return;
        }

        Preview->SetChildActorClass(PartClass);
        Preview->SetRelativeLocationAndRotation(
            FVector::ZeroVector,
            FRotator::ZeroRotator
        );
        Preview->SetRelativeScale3D(RelativeScale);
        if (ACMPartActorBase* PreviewActor =
                Cast<ACMPartActorBase>(Preview->GetChildActor()))
        {
            PreviewActor->SetActorEnableCollision(false);
            Preview->SetRelativeScale3D(
                ResolvePartScaleForSlot(
                    *PreviewActor,
                    *PartSlot,
                    RelativeScale));
        }
        AlignPartPreviewMountToSlotAnchor(*Preview, *PartSlot);
    };

    for (int32 Index = 0; Index < LeftPartPreviews.Num(); ++Index)
    {
        RefreshPreview(
            LeftPartPreviews[Index],
            LeftPartSlots.IsValidIndex(Index) ? LeftPartSlots[Index] : nullptr,
            ResolvePartClassForSegment(Index, false),
            FVector::OneVector);
    }
    for (int32 Index = 0; Index < RightPartPreviews.Num(); ++Index)
    {
        RefreshPreview(
            RightPartPreviews[Index],
            RightPartSlots.IsValidIndex(Index)
                ? RightPartSlots[Index]
                : nullptr,
            ResolvePartClassForSegment(Index, true),
            RightPartRelativeScale
        );
    }
}

TSubclassOf<ACMPartActorBase>
ACMProceduralAnimationTestBody::ResolvePartClassForSegment(
    const int32 SegmentIndex,
    const bool bRightSide
) const
{
    if (HeadPartClass
        && SegmentIndex == SimulatedHeadSegmentIndex
        && bRightSide == bSimulatedHeadOnRightSide)
    {
        return HeadPartClass;
    }
    if (ArmPartClass && SimulatedArmSegmentIndices.Contains(SegmentIndex))
    {
        return ArmPartClass;
    }
    return bRightSide ? RightPartClass : LeftPartClass;
}

void ACMProceduralAnimationTestBody::DestroyPartPreviews()
{
    for (UChildActorComponent* Preview : PartPreviews)
    {
        if (IsValid(Preview))
        {
            Preview->SetChildActorClass(nullptr);
        }
    }
}

void ACMProceduralAnimationTestBody::DestroySegmentConstraints()
{
    for (UPhysicsConstraintComponent* Constraint : SegmentConstraints)
    {
        if (Constraint)
        {
            Constraint->DestroyComponent();
        }
    }
    SegmentConstraints.Reset();
}

void ACMProceduralAnimationTestBody::ConfigureSegmentConstraints()
{
    if (!HasAuthority() || BodySegments.Num() < 2)
    {
        return;
    }

    DestroySegmentConstraints();
    SegmentConstraints.Reserve(BodySegments.Num() - 1);
    for (int32 Index = 0; Index < BodySegments.Num() - 1; ++Index)
    {
        UBoxComponent* FrontBody = BodySegments[Index];
        UBoxComponent* RearBody = BodySegments[Index + 1];
        if (!FrontBody || !RearBody
            || !FrontBody->IsSimulatingPhysics()
            || !RearBody->IsSimulatingPhysics())
        {
            continue;
        }

        UPhysicsConstraintComponent* Constraint =
            NewObject<UPhysicsConstraintComponent>(
                this,
                MakeUniqueObjectName(
                    this,
                    UPhysicsConstraintComponent::StaticClass(),
                    *FString::Printf(TEXT("SegmentConstraint_%d"), Index)
                )
            );
        if (!Constraint)
        {
            continue;
        }

        AddInstanceComponent(Constraint);
        Constraint->SetupAttachment(PhysicsBody);
        Constraint->ComponentName1.ComponentName = FrontBody->GetFName();
        Constraint->ComponentName2.ComponentName = RearBody->GetFName();
        Constraint->OverrideComponent1 = FrontBody;
        Constraint->OverrideComponent2 = RearBody;
        Constraint->SetWorldLocation(
            (FrontBody->GetComponentLocation()
                + RearBody->GetComponentLocation()) * 0.5f
        );
        Constraint->SetLinearXLimit(
            ELinearConstraintMotion::LCM_Locked,
            0.0f
        );
        Constraint->SetLinearYLimit(
            ELinearConstraintMotion::LCM_Locked,
            0.0f
        );
        Constraint->SetLinearZLimit(
            ELinearConstraintMotion::LCM_Locked,
            0.0f
        );
        Constraint->SetAngularSwing1Limit(
            EAngularConstraintMotion::ACM_Limited,
            50.0f
        );
        Constraint->SetAngularSwing2Limit(
            EAngularConstraintMotion::ACM_Limited,
            22.0f
        );
        Constraint->SetAngularTwistLimit(
            bSimulateOneSidedBodyLean
                ? EAngularConstraintMotion::ACM_Free
                : EAngularConstraintMotion::ACM_Limited,
            8.0f
        );
        Constraint->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
        Constraint->SetOrientationDriveTwistAndSwing(false, false);
        Constraint->SetAngularVelocityDriveTwistAndSwing(false, true);
        Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
        Constraint->SetAngularDriveParams(0.0f, 5.0f, 0.0f);
        Constraint->SetAngularDriveAccelerationMode(true);
        Constraint->SetDisableCollision(true);
        Constraint->RegisterComponent();
        SegmentConstraints.Add(Constraint);
    }
}

void ACMProceduralAnimationTestBody::ConfigureBodyRotationLock(
    UBoxComponent* SegmentBody
)
{
    if (!SegmentBody)
    {
        return;
    }

    FBodyInstance& BodyInstance = SegmentBody->BodyInstance;
    BodyInstance.bLockXRotation = bLockBodyRoll
        && !bSimulateOneSidedBodyLean;
    BodyInstance.bLockYRotation = false;
    BodyInstance.bLockZRotation = false;
    BodyInstance.SetDOFLock(EDOFMode::SixDOF);
}

void ACMProceduralAnimationTestBody::ResolveInitialGroundPenetration()
{
    if (!bResolveInitialGroundPenetration
        || !IsValid(PhysicsBody)
        || !GetWorld())
    {
        return;
    }

    // Check every segment before enabling Chaos. On a slope, correcting only
    // the root can leave a rear segment embedded in the terrain and the first
    // depenetration impulse can launch the whole constrained chain.
    float RequiredLift = 0.0f;
    for (const UBoxComponent* SegmentBody : BodySegments)
    {
        if (!IsValid(SegmentBody))
        {
            continue;
        }

        const FVector SegmentCenter = SegmentBody->GetComponentLocation();
        const float SegmentHalfHeight =
            SegmentBody->GetScaledBoxExtent().Z;
        FHitResult GroundHit;
        const FVector ProbePoint = SegmentCenter
            - FVector::UpVector * SegmentHalfHeight;
        if (!TraceGroundAtPoint(ProbePoint, nullptr, GroundHit))
        {
            continue;
        }

        const float MinimumCenterZ = GroundHit.ImpactPoint.Z
            + SegmentHalfHeight
            + InitialGroundClearance;
        RequiredLift = FMath::Max(
            RequiredLift,
            MinimumCenterZ - SegmentCenter.Z
        );
    }
    if (RequiredLift <= 0.0f)
    {
        return;
    }

    FVector CorrectedLocation = GetActorLocation();
    CorrectedLocation.Z += RequiredLift;
    SetActorLocation(
        CorrectedLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );
}

UCMPartSlotComponent*
ACMProceduralAnimationTestBody::GetSimulatedPlayerPartSlot(
    const int32 PlayerIndex,
    const int32 KeyIndex
) const
{
    constexpr int32 PlayersPerSide = 2;
    constexpr int32 KeysPerPlayer = 4;
    if (PlayerIndex < 0 || PlayerIndex >= PlayersPerSide * 2
        || KeyIndex < 0 || KeyIndex >= KeysPerPlayer)
    {
        return nullptr;
    }

    const bool bRightSidePlayer = PlayerIndex >= PlayersPerSide;
    const int32 PlayerIndexOnSide = PlayerIndex % PlayersPerSide;
    const int32 SegmentIndex = PlayerIndexOnSide
        + KeyIndex * PlayersPerSide;
    const TArray<TObjectPtr<UCMPartSlotComponent>>& SideSlots =
        bRightSidePlayer ? RightPartSlots : LeftPartSlots;
    return SideSlots.IsValidIndex(SegmentIndex)
        ? SideSlots[SegmentIndex]
        : nullptr;
}

void ACMProceduralAnimationTestBody::UpdateSimulatedPartAction(
    UCMPartSlotComponent* PartSlot,
    const float DeltaSeconds
)
{
    if (!IsValid(PartSlot))
    {
        return;
    }

    ACMPartActorBase* Part = Cast<ACMPartActorBase>(
        PartSlot->GetAttachedPart()
    );
    if (!IsValid(Part) || !Part->IsOperational())
    {
        return;
    }

    if (ACMLegPart* LegPart = Cast<ACMLegPart>(Part))
    {
        LegPart->AdvancePlantState();
        FSimulatedLegForceAction* ForceAction =
            SimulatedLegForceActions.Find(LegPart);
        if (ForceAction)
        {
            ApplySimulatedLegForce(
                *LegPart,
                *PartSlot,
                ForceAction->bReverseMovement
            );
            ForceAction->RemainingTime -= FMath::Max(DeltaSeconds, 0.0f);
            if (ForceAction->RemainingTime <= 0.0f)
            {
                SimulatedLegForceActions.Remove(LegPart);
            }
        }
        return;
    }

    if (ACMArmPart* ArmPart = Cast<ACMArmPart>(Part))
    {
        if (ArmPart->IsSwinging() && ArmPart->GetSwingPhase() >= 1.0f)
        {
            ArmPart->EndSwing();
        }
        return;
    }

    if (ACMHeadPartActor* HeadPart = Cast<ACMHeadPartActor>(Part))
    {
        return;
    }
}

void ACMProceduralAnimationTestBody::UpdateSimulatedLegReachRecovery()
{
    for (UCMPartSlotComponent* PartSlot : PartSlots)
    {
        if (!IsValid(PartSlot))
        {
            continue;
        }
        ACMLegPart* LegPart = Cast<ACMLegPart>(
            PartSlot->GetAttachedPart());
        if (!IsValid(LegPart)
            || !LegPart->IsOperational())
        {
            continue;
        }

        UPrimitiveComponent* BodySegment = Cast<UPrimitiveComponent>(
            PartSlot->GetAttachParent());
        FVector PlanarVelocity = BodySegment
            ? BodySegment->GetPhysicsLinearVelocityAtPoint(
                PartSlot->GetComponentLocation())
            : FVector::ZeroVector;
        PlanarVelocity.Z = 0.0f;
        const float FastSpeedThreshold = FMath::Max(
            SimulatedFastLegSpeedThreshold,
            1.0f);
        const float FastMovementAlpha = FMath::GetMappedRangeValueClamped(
            FVector2D(FastSpeedThreshold * 0.5f, FastSpeedThreshold),
            FVector2D(0.0f, 1.0f),
            PlanarVelocity.Size());
        float ReleaseDistance = SimulatedLegReplantReleaseDistance;
        if (bSimulateOneSidedBodyLean)
        {
            ReleaseDistance = FMath::Min(
                ReleaseDistance,
                SimulatedOneSidedLeanReplantReleaseDistance);
        }
        else if (bSimulatedInputPaused)
        {
            ReleaseDistance = FMath::Min(
                ReleaseDistance,
                SimulatedPauseTurnReplantReleaseDistance);
        }
        float UpperLength = 0.0f;
        float LowerLength = 0.0f;
        if (LegPart->GetPartMesh()
            && TryGetLegReferenceLengths(
                *LegPart->GetPartMesh(),
                UpperLength,
                LowerLength))
        {
            ReleaseDistance = FMath::Min(
                ReleaseDistance,
                (UpperLength + LowerLength) * 0.45f);
        }
        ReleaseDistance = FMath::Max(ReleaseDistance, 1.0f);

        FSimulatedLegReplantPath& ReplantPath =
            SimulatedLegReplantPaths.FindOrAdd(LegPart);
        FVector ReferenceLocation = PartSlot->GetComponentLocation();
        ReferenceLocation.Z = 0.0f;
        if (!ReplantPath.bInitialized)
        {
            ReplantPath.bInitialized = true;
            ReplantPath.LastReferenceLocation = ReferenceLocation;
        }

        const float TravelDistance = FVector::Distance(
            ReferenceLocation,
            ReplantPath.LastReferenceLocation);
        ReplantPath.LastReferenceLocation = ReferenceLocation;
        ReplantPath.DistanceTowardNextStep = FMath::Fmod(
            FMath::Max(ReplantPath.DistanceTowardNextStep, 0.0f),
            ReleaseDistance);
        const float AccumulatedDistance =
            ReplantPath.DistanceTowardNextStep + TravelDistance;
        const int32 NewStepCount = FMath::FloorToInt(
            AccumulatedDistance / ReleaseDistance);
        if (NewStepCount > 0)
        {
            ReplantPath.PendingStepCount += NewStepCount;
        }
        ReplantPath.DistanceTowardNextStep = AccumulatedDistance
            - NewStepCount * ReleaseDistance;

        FVector ForwardDirection = PartSlot->GetForwardVector();
        ForwardDirection.Z = 0.0f;
        ForwardDirection.Normalize();
        const float ForwardLead = FMath::Clamp(
            FVector::DotProduct(PlanarVelocity, ForwardDirection)
                * SimulatedFastLegTargetLeadTime
                * FastMovementAlpha,
            -SimulatedLegStepDistance,
            SimulatedLegStepDistance);

        FHitResult DesiredGroundHit;
        if (LegPart->GetPlantState() == ECMLegPlantState::Free)
        {
            if (TraceReachableLegGround(
                    *PartSlot,
                    *LegPart,
                    ForwardLead,
                    DesiredGroundHit))
            {
                LegPart->InitializePlantedContact(
                    DesiredGroundHit.ImpactPoint,
                    DesiredGroundHit.ImpactNormal);
            }
        }
        if (LegPart->GetPlantState() != ECMLegPlantState::Planted
            || ReplantPath.PendingStepCount <= 0
            || !TraceReachableLegGround(
                *PartSlot,
                *LegPart,
                ForwardLead,
                DesiredGroundHit))
        {
            continue;
        }

        const FVector GroundNormal = LegPart->GetStepGroundNormal()
            .GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
        SimulatedMaxReachError = FMath::Max(
            SimulatedMaxReachError,
            FVector::VectorPlaneProject(
                DesiredGroundHit.ImpactPoint
                    - LegPart->GetStepGroundLocation(),
                GroundNormal).Size());

        // Reach recovery is visual-only: never call ApplySimulatedLegForce
        // or advance a body segment from this path.
        const float BaseReplantDuration = FMath::Max(
            SimulatedLegReplantDuration,
            0.05f);
        const float FastReplantDuration = FMath::Clamp(
            SimulatedFastLegReplantDuration,
            0.05f,
            BaseReplantDuration);
        float ReplantDuration = FMath::Lerp(
            BaseReplantDuration,
            FastReplantDuration,
            FastMovementAlpha);
        const float PlanarSpeed = PlanarVelocity.Size();
        if (PlanarSpeed > UE_SMALL_NUMBER)
        {
            ReplantDuration = FMath::Min(
                ReplantDuration,
                FMath::Max(
                    ReleaseDistance / PlanarSpeed,
                    1.0f / 60.0f));
        }
        LegPart->BeginVisualReplant(
            DesiredGroundHit.ImpactPoint,
            DesiredGroundHit.ImpactNormal,
            ReplantDuration);
        --ReplantPath.PendingStepCount;
        ++SimulatedVisualReplantCount;
    }
}

bool ACMProceduralAnimationTestBody::TraceReachableLegGround(
    const UCMPartSlotComponent& PartSlot,
    const ACMLegPart& LegPart,
    const float ForwardOffset,
    FHitResult& OutHit
) const
{
    const USceneComponent* Anchor = PartSlot.GetLegRigControlAnchor();
    const USceneComponent* BodySegment = PartSlot.GetAttachParent();
    const USkeletalMeshComponent* PartMesh = LegPart.GetPartMesh();
    if (!Anchor || !BodySegment || !PartMesh)
    {
        return false;
    }

    FVector SideDirection = PartSlot.GetComponentLocation()
        - BodySegment->GetComponentLocation();
    SideDirection.Z = 0.0f;
    if (!SideDirection.Normalize())
    {
        return false;
    }

    FVector ForwardDirection = PartSlot.GetForwardVector();
    ForwardDirection.Z = 0.0f;
    if (!ForwardDirection.Normalize())
    {
        return false;
    }

    float UpperLength = 0.0f;
    float LowerLength = 0.0f;
    if (!TryGetLegReferenceLengths(
            *PartMesh,
            UpperLength,
            LowerLength))
    {
        return false;
    }

    const FVector HipLocation = Anchor->GetComponentLocation();
    FVector PlanarOffset = SideDirection * UpperLength
        + ForwardDirection * ForwardOffset;
    FVector DesiredFootPoint = HipLocation + PlanarOffset;
    if (!TraceGroundAtPoint(DesiredFootPoint, &LegPart, OutHit))
    {
        return false;
    }

    const float SafeReach = (UpperLength + LowerLength) * 0.9f;
    const float VerticalDistance = FMath::Abs(
        HipLocation.Z - OutHit.ImpactPoint.Z);
    const float MaximumPlanarReach = FMath::Sqrt(FMath::Max(
        FMath::Square(SafeReach) - FMath::Square(VerticalDistance),
        0.0f));
    if (PlanarOffset.SizeSquared()
        <= FMath::Square(MaximumPlanarReach))
    {
        return true;
    }

    PlanarOffset = PlanarOffset.GetSafeNormal() * MaximumPlanarReach;
    DesiredFootPoint = HipLocation + PlanarOffset;
    return TraceGroundAtPoint(DesiredFootPoint, &LegPart, OutHit);
}

bool ACMProceduralAnimationTestBody::TraceGroundAtPoint(
    const FVector& DesiredFootPoint,
    const AActor* IgnoredPart,
    FHitResult& OutHit
) const
{
    if (!GetWorld())
    {
        return false;
    }

    const FVector Start = DesiredFootPoint
        + FVector::UpVector * GroundTraceHeight;
    const FVector End = DesiredFootPoint
        - FVector::UpVector * GroundTraceDepth;

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(CMProceduralTestBodyGround),
        false,
        this
    );
    if (IgnoredPart)
    {
        QueryParams.AddIgnoredActor(IgnoredPart);
    }
    for (const UCMPartSlotComponent* PartSlot : PartSlots)
    {
        if (PartSlot && PartSlot->GetAttachedPart())
        {
            QueryParams.AddIgnoredActor(PartSlot->GetAttachedPart());
        }
    }

    const auto SelectWalkableGround = [this, &OutHit](
        const TArray<FHitResult>& CandidateHits)
    {
        bool bFound = false;
        float BestMetric = MAX_FLT;
        for (const FHitResult& Hit : CandidateHits)
        {
            const AActor* HitActor = Hit.GetActor();
            if (HitActor && HitActor->GetAttachParentActor() == this)
            {
                continue;
            }
            if (Hit.bStartPenetrating
                || Hit.ImpactPoint.ContainsNaN()
                || Hit.ImpactNormal.ContainsNaN())
            {
                continue;
            }

            const FVector SafeNormal = Hit.ImpactNormal.GetSafeNormal();
            if (SafeNormal.IsNearlyZero()
                || SafeNormal.Z < MinimumGroundNormalZ)
            {
                continue;
            }

            float Metric = Hit.Time;
            if (!FMath::IsFinite(Metric) || Metric < 0.0f)
            {
                Metric = Hit.Distance;
            }
            if (!FMath::IsFinite(Metric))
            {
                Metric = 0.0f;
            }
            if (!bFound || Metric < BestMetric)
            {
                OutHit = Hit;
                OutHit.ImpactNormal = SafeNormal;
                BestMetric = Metric;
                bFound = true;
            }
        }
        return bFound;
    };

    TArray<FHitResult> ChannelHits;
    GetWorld()->SweepMultiByChannel(
        ChannelHits,
        Start,
        End,
        FQuat::Identity,
        GroundTraceChannel,
        FCollisionShape::MakeSphere(GroundCheckRadius),
        QueryParams);
    if (SelectWalkableGround(ChannelHits))
    {
        return true;
    }

    TArray<FHitResult> WorldStaticHits;
    FCollisionObjectQueryParams ObjectQuery;
    ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
    GetWorld()->SweepMultiByObjectType(
        WorldStaticHits,
        Start,
        End,
        FQuat::Identity,
        ObjectQuery,
        FCollisionShape::MakeSphere(GroundCheckRadius),
        QueryParams);
    return SelectWalkableGround(WorldStaticHits);
}

bool ACMProceduralAnimationTestBody::SimulatePartButtonPress(
    UCMPartSlotComponent* PartSlot,
    const bool bReverseMovement
)
{
    if (!IsValid(PartSlot))
    {
        return false;
    }

    ACMPartActorBase* Part = Cast<ACMPartActorBase>(
        PartSlot->GetAttachedPart()
    );
    if (!IsValid(Part) || !Part->IsOperational())
    {
        return false;
    }

    if (ACMLegPart* LegPart = Cast<ACMLegPart>(Part))
    {
        FHitResult GroundHit;
        const bool bHitGround = TraceReachableLegGround(
            *PartSlot,
            *LegPart,
            SimulatedLegStepDistance
                * (bReverseMovement ? -1.0f : 1.0f),
            GroundHit);
        if (!bHitGround)
        {
            return false;
        }
        if (LegPart->GetPlantState() == ECMLegPlantState::Free)
        {
            LegPart->InitializePlantedContact(
                GroundHit.ImpactPoint,
                GroundHit.ImpactNormal);
        }
        FSimulatedLegForceAction& ForceAction =
            SimulatedLegForceActions.FindOrAdd(LegPart);
        ForceAction.RemainingTime = FMath::Max(
            ForceAction.RemainingTime,
            LegPart->GetActionDuration());
        ForceAction.bReverseMovement = bReverseMovement;
        return true;
    }

    if (ACMArmPart* ArmPart = Cast<ACMArmPart>(Part))
    {
        return false;
    }

    return Cast<ACMHeadPartActor>(Part) != nullptr;
}

void ACMProceduralAnimationTestBody::ApplySimulatedLegForce(
    const ACMLegPart& LegPart,
    const UCMPartSlotComponent& PartSlot,
    const bool bReverseMovement
)
{
    UBoxComponent* SegmentBody = Cast<UBoxComponent>(
        PartSlot.GetAttachParent()
    );
    if (!SegmentBody || !SegmentBody->IsSimulatingPhysics())
    {
        return;
    }

    FVector ForwardDirection = PartSlot.GetForwardVector();
    ForwardDirection.Z = 0.0f;
    ForwardDirection.Normalize();
    if (ForwardDirection.IsNearlyZero())
    {
        return;
    }
    if (bReverseMovement)
    {
        ForwardDirection *= -1.0f;
    }

    const float PushDuration = FMath::Max(
        LegPart.GetActionDuration(),
        0.01f
    );
    float ForceScale = FMath::Max(SimulatedLegForceScale, 0.0f);
    const int32 SegmentIndex = PartSlot.GetSlotAddress().SegmentIndex;
    if (bSimulatedInputPaused
        && SimulatedPauseTurnSegmentIndices.Contains(SegmentIndex))
    {
        ForceScale *= FMath::Max(SimulatedPauseTurnForceScale, 0.0f);
    }

    const FVector PushForce = ForwardDirection
        * LegPart.GetMovementImpulse()
        * ForceScale
        / PushDuration;
    FVector ForceLocation = PartSlot.GetComponentLocation();
    ForceLocation.Z = SegmentBody->GetCenterOfMass().Z;
    SegmentBody->AddForceAtLocation(PushForce, ForceLocation);
}

void ACMProceduralAnimationTestBody::RegisterSimulatedCooperativeInput(
    const bool bIsLeft,
    const float MovementImpulse,
    const bool bReverseMovement
)
{
    if (MovementImpulse <= UE_SMALL_NUMBER)
    {
        return;
    }

    const float CurrentTime = SimulatedCooperationClock;
    const auto PurgeExpired = [CurrentTime](
        TArray<FPendingSimulatedCooperativeInput>& PendingInputs)
    {
        PendingInputs.RemoveAll([CurrentTime](
            const FPendingSimulatedCooperativeInput& PendingInput)
        {
            return PendingInput.ExpireTime <= CurrentTime;
        });
    };
    PurgeExpired(PendingLeftCooperativeInputs);
    PurgeExpired(PendingRightCooperativeInputs);

    FPendingSimulatedCooperativeInput PendingInput;
    PendingInput.RemainingImpulse = MovementImpulse;
    PendingInput.bReverseMovement = bReverseMovement;
    PendingInput.ExpireTime = CurrentTime + FMath::Max(
        SimulatedCooperationInputWindow,
        0.01f
    );
    (bIsLeft ? PendingLeftCooperativeInputs : PendingRightCooperativeInputs)
        .Add(PendingInput);

    while (!PendingLeftCooperativeInputs.IsEmpty()
        && !PendingRightCooperativeInputs.IsEmpty())
    {
        int32 LeftIndex = INDEX_NONE;
        int32 RightIndex = INDEX_NONE;
        for (int32 CandidateLeft = 0;
            CandidateLeft < PendingLeftCooperativeInputs.Num();
            ++CandidateLeft)
        {
            const bool bReverse = PendingLeftCooperativeInputs[
                CandidateLeft].bReverseMovement;
            RightIndex = PendingRightCooperativeInputs.IndexOfByPredicate(
                [bReverse](const FPendingSimulatedCooperativeInput& Input)
                {
                    return Input.bReverseMovement == bReverse;
                }
            );
            if (RightIndex != INDEX_NONE)
            {
                LeftIndex = CandidateLeft;
                break;
            }
        }
        if (LeftIndex == INDEX_NONE || RightIndex == INDEX_NONE)
        {
            return;
        }

        FPendingSimulatedCooperativeInput& LeftInput =
            PendingLeftCooperativeInputs[LeftIndex];
        FPendingSimulatedCooperativeInput& RightInput =
            PendingRightCooperativeInputs[RightIndex];
        const float MatchedImpulse = FMath::Min(
            LeftInput.RemainingImpulse,
            RightInput.RemainingImpulse
        );
        ApplySimulatedCooperativeImpulse(
            MatchedImpulse * 2.0f
                * (LeftInput.bReverseMovement ? -1.0f : 1.0f)
        );
        LeftInput.RemainingImpulse -= MatchedImpulse;
        RightInput.RemainingImpulse -= MatchedImpulse;
        if (LeftInput.RemainingImpulse <= UE_SMALL_NUMBER)
        {
            PendingLeftCooperativeInputs.RemoveAt(LeftIndex);
        }
        if (RightInput.RemainingImpulse <= UE_SMALL_NUMBER)
        {
            PendingRightCooperativeInputs.RemoveAt(RightIndex);
        }
    }
}

void ACMProceduralAnimationTestBody::ApplySimulatedCooperativeImpulse(
    const float SignedForwardImpulse
)
{
    const float DirectionSign = FMath::Sign(SignedForwardImpulse);
    float ForwardImpulseMagnitude = FMath::Abs(SignedForwardImpulse);
    if (FMath::IsNearlyZero(DirectionSign)
        || ForwardImpulseMagnitude <= UE_SMALL_NUMBER)
    {
        return;
    }

    float TotalMass = 0.0f;
    TArray<UBoxComponent*> SimulatedSegments;
    for (UBoxComponent* SegmentBody : BodySegments)
    {
        if (SegmentBody && SegmentBody->IsSimulatingPhysics())
        {
            TotalMass += FMath::Max(SegmentBody->GetMass(), 0.01f);
            SimulatedSegments.Add(SegmentBody);
        }
    }
    if (TotalMass <= UE_SMALL_NUMBER)
    {
        return;
    }

    const float TotalImpulseLimit = FMath::Max(
        SimulatedMaximumCooperativePlanarImpulse,
        0.0f
    ) * SimulatedSegments.Num();
    ForwardImpulseMagnitude = FMath::Min(
        ForwardImpulseMagnitude,
        TotalImpulseLimit);
    if (ForwardImpulseMagnitude <= UE_SMALL_NUMBER)
    {
        return;
    }

    for (UBoxComponent* SegmentBody : SimulatedSegments)
    {
        FVector SegmentForwardDirection = SegmentBody->GetForwardVector();
        SegmentForwardDirection.Z = 0.0f;
        if (!SegmentForwardDirection.Normalize())
        {
            continue;
        }

        const float MassFraction = FMath::Max(
            SegmentBody->GetMass(),
            0.01f
        ) / TotalMass;
        SegmentBody->AddImpulse(
            SegmentForwardDirection
            * ForwardImpulseMagnitude
            * DirectionSign
            * MassFraction
        );
    }
}
