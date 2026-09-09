#include "Stage/Obstacle/CMArmHoldableBox.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Arm/CMArmPart.h"
#include "UObject/ConstructorHelpers.h"

ACMArmHoldableBox::ACMArmHoldableBox()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
    SetRootComponent(BoxMesh);
    BoxMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    BoxMesh->SetSimulatePhysics(true);
    BoxMesh->SetEnableGravity(true);
    BoxMesh->SetLinearDamping(0.5f);
    BoxMesh->SetAngularDamping(1.0f);
    BoxMesh->SetIsReplicated(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMeshAsset.Succeeded())
    {
        BoxMesh->SetStaticMesh(CubeMeshAsset.Object);
    }
}

void ACMArmHoldableBox::BeginPlay()
{
    Super::BeginPlay();
    InitialTransform = GetActorTransform();
    BoxMesh->SetMassOverrideInKg(
        NAME_None,
        FMath::Max(MassInKg, 1.0f),
        true);
}

void ACMArmHoldableBox::ResetForCheckpoint()
{
    if (!HasAuthority() || !BoxMesh)
    {
        return;
    }

    if (IsValid(HoldingArm))
    {
        HoldingArm->EndGroundAnchor();
    }
    HoldingArm = nullptr;
    BoxMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
    BoxMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    SetActorTransform(
        InitialTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    BoxMesh->WakeAllRigidBodies();
    ForceNetUpdate();
}

void ACMArmHoldableBox::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMArmHoldableBox, HoldingArm);
}

bool ACMArmHoldableBox::QueryArmHold_Implementation(
    ACMArmPart* ArmPart,
    FCMArmHoldSpec& OutSpec
) const
{
    if (!HasAuthority() || !IsValid(ArmPart) || HoldingArm
        || !BoxMesh || !BoxMesh->IsSimulatingPhysics())
    {
        return false;
    }

    const FVector ArmLocation = ArmPart->GetPartMesh()
        ? ArmPart->GetPartMesh()->GetComponentLocation()
        : ArmPart->GetActorLocation();
    // 중심이 아니라 팔에서 가장 가까운 충돌 표면을 잡아
    // 상자 크기가 바뀌어도 손 IK가 자연스럽게 보이도록 한다.
    FVector HoldLocation = BoxMesh->Bounds.Origin;
    BoxMesh->GetClosestPointOnCollision(ArmLocation, HoldLocation);

    OutSpec.Priority = ArmHoldPriority;
    OutSpec.HoldLocation = HoldLocation;
    OutSpec.HoldNormal = (ArmLocation - HoldLocation).GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    OutSpec.TargetComponent = BoxMesh;
    // 상자는 손을 따라 실제로 이동해야 하므로 Physics Handle을 요청한다.
    OutSpec.bUsePhysicsHandle = true;
    return true;
}

bool ACMArmHoldableBox::BeginArmHold_Implementation(ACMArmPart* ArmPart)
{
    if (!HasAuthority() || !IsValid(ArmPart) || HoldingArm)
    {
        return false;
    }

    HoldingArm = ArmPart;
    ForceNetUpdate();
    return true;
}

void ACMArmHoldableBox::EndArmHold_Implementation(ACMArmPart* ArmPart)
{
    if (!HasAuthority() || HoldingArm != ArmPart)
    {
        return;
    }

    HoldingArm = nullptr;
    ForceNetUpdate();
}
