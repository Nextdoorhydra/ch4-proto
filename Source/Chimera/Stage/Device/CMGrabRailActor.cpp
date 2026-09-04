#include "Stage/Device/CMGrabRailActor.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Stage/Device/Component/CMRailMovementComponent.h"

ACMGrabRailActor::ACMGrabRailActor()
{
    SetNetUpdateFrequency(30.0f);
    Rail = CreateDefaultSubobject<USplineComponent>(TEXT("Rail"));
    Rail->SetupAttachment(SceneRoot);
    Rail->SetLocationAtSplinePoint(1, FVector(300, 0, 0), ESplineCoordinateSpace::Local);
    RailBody = CreateDefaultSubobject<UBoxComponent>(TEXT("RailBody"));
    RailBody->SetupAttachment(SceneRoot);
    RailBody->SetBoxExtent(FVector(40));
    RailBody->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    RailBody->SetMobility(EComponentMobility::Movable);
    MovingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MovingMesh"));
    MovingMesh->SetupAttachment(RailBody);
    MovingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GrabHandle = CreateDefaultSubobject<USphereComponent>(TEXT("GrabHandle"));
    GrabHandle->SetupAttachment(RailBody);
    GrabHandle->SetRelativeLocation(FVector(0, -50, 0));
    GrabHandle->SetSphereRadius(20);
    GrabHandle->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    RailMovement = CreateDefaultSubobject<UCMRailMovementComponent>(TEXT("RailMovement"));
}

void ACMGrabRailActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (!HasActorBegunPlay()) UCMRailMovementComponent::PreviewPose(Rail, RailBody,
        RailMovement->InitialProgress, RailMovement->bFollowRailRotation, RailMovement->RotationOffset);
}

void ACMGrabRailActor::BeginPlay()
{
    RailMovement->ConfigureRail(Rail, RailBody, GrabHandle);
    Super::BeginPlay();
}

bool ACMGrabRailActor::QueryArmHold_Implementation(ACMArmPart* Arm, FCMArmHoldSpec& OutSpec) const
{
    return IsElementActive() && RailMovement->QueryArmHold(Arm, OutSpec);
}

bool ACMGrabRailActor::BeginArmHold_Implementation(ACMArmPart* Arm)
{
    return IsElementActive() && RailMovement->BeginArmHold(Arm);
}

void ACMGrabRailActor::EndArmHold_Implementation(ACMArmPart* Arm)
{
    RailMovement->EndArmHold(Arm);
}

void ACMGrabRailActor::HandleElementActiveChanged_Implementation(bool bIsActive)
{
    Super::HandleElementActiveChanged_Implementation(bIsActive);
    RailMovement->SetInteractionEnabled(bIsActive);
}

void ACMGrabRailActor::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    RailMovement->ResetRail();
}
