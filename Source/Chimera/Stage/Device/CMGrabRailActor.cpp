#include "Stage/Device/CMGrabRailActor.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Parts/Arm/CMArmPart.h"
#include "Stage/Device/Component/CMInteractionHighlightComponent.h"
#include "Stage/Device/Component/CMRailMovementComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraGrabRail, Log, All);

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
    RailBody->SetNotifyRigidBodyCollision(true);
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
    InteractionHighlight = CreateDefaultSubobject<UCMInteractionHighlightComponent>(TEXT("InteractionHighlight"));
}

void ACMGrabRailActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (!HasActorBegunPlay())
    {
        TArray<USplineComponent*> RailSegments;
        GatherRailSegments(RailSegments);
        const int32 InitialSegmentIndex = FMath::Clamp(
            RailMovement->InitialSegmentIndex,
            0,
            FMath::Max(RailSegments.Num() - 1, 0));
        if (RailSegments.IsValidIndex(InitialSegmentIndex))
        {
            UCMRailMovementComponent::PreviewPose(
                RailSegments[InitialSegmentIndex],
                RailBody,
                RailMovement->InitialProgress,
                RailMovement->bFollowRailRotation,
                RailMovement->RotationOffset);
        }
    }
}

void ACMGrabRailActor::BeginPlay()
{
    TArray<USplineComponent*> RailSegments;
    GatherRailSegments(RailSegments);
    RailMovement->ConfigureRailGraph(RailSegments, RailBody, GrabHandle, MovingMesh);
    RailBody->OnComponentHit.AddDynamic(this, &ThisClass::HandleRailBodyHit);
    InteractionHighlight->AddHighlightTarget(MovingMesh);
    Super::BeginPlay();

    if (HasAuthority())
    {
        GetWorldTimerManager().SetTimer(InteractionHintTimerHandle, this,
            &ThisClass::RefreshInteractionHighlight,
            FMath::Max(InteractionHintRefreshInterval, 0.02f), true, 0.0f);
    }
}

void ACMGrabRailActor::GatherRailSegments(
    TArray<USplineComponent*>& OutRailSegments) const
{
    OutRailSegments.Reset();
    TInlineComponentArray<USplineComponent*> SplineComponents(this);
    SplineComponents.Sort([](const USplineComponent& Left, const USplineComponent& Right)
    {
        return Left.GetName() < Right.GetName();
    });

    if (SplineComponents.Remove(Rail) > 0)
    {
        OutRailSegments.Add(Rail);
    }

    for (USplineComponent* SplineComponent : SplineComponents)
    {
        if (!IsValid(SplineComponent) || SplineComponent->IsClosedLoop()
            || SplineComponent->GetNumberOfSplinePoints() < 2
            || SplineComponent->GetSplineLength() <= UE_KINDA_SMALL_NUMBER)
        {
            UE_LOG(LogChimeraGrabRail, Warning,
                TEXT("[Rail Branch Invalid] Actor=%s Branch=%s requires an open spline with at least two distinct points."),
                *GetName(), *SplineComponent->GetName());
            continue;
        }

        OutRailSegments.Add(SplineComponent);
    }
}

void ACMGrabRailActor::HandleRailBodyHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (!HasAuthority() || !IsElementActive() || !IsValid(OtherActor)
        || OtherActor == this || LastCollisionPushFrame == GFrameCounter)
    {
        return;
    }

    const FVector PusherLocation = IsValid(OtherComponent)
        ? OtherComponent->Bounds.Origin
        : OtherActor->GetActorLocation();
    FVector PushDirection = RailBody->Bounds.Origin - PusherLocation;
    PushDirection.Z = 0.0f;
    if (RailMovement->TryCollisionPush(
        OtherActor,
        PushDirection,
        GetWorld()->GetDeltaSeconds()))
    {
        LastCollisionPushFrame = GFrameCounter;
    }
}

void ACMGrabRailActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(InteractionHintTimerHandle);
    Super::EndPlay(EndPlayReason);
}

bool ACMGrabRailActor::QueryArmHold_Implementation(ACMArmPart* Arm, FCMArmHoldSpec& OutSpec) const
{
    return IsElementActive() && RailMovement->QueryArmHold(Arm, OutSpec);
}

bool ACMGrabRailActor::BeginArmHold_Implementation(ACMArmPart* Arm)
{
    const bool bStarted = IsElementActive() && RailMovement->BeginArmHold(Arm);
    if (bStarted) RefreshInteractionHighlight();
    return bStarted;
}

void ACMGrabRailActor::EndArmHold_Implementation(ACMArmPart* Arm)
{
    RailMovement->EndArmHold(Arm);
    RefreshInteractionHighlight();
}

void ACMGrabRailActor::ResetForCheckpoint()
{
    ResetElement();
}

void ACMGrabRailActor::HandleElementActiveChanged_Implementation(bool bIsActive)
{
    Super::HandleElementActiveChanged_Implementation(bIsActive);
    RailMovement->SetInteractionEnabled(bIsActive);
    RefreshInteractionHighlight();
}

void ACMGrabRailActor::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    RailMovement->ResetRail();
}

void ACMGrabRailActor::RefreshInteractionHighlight()
{
    if (!HasAuthority()) return;

    bool bCanInteract = false;
    if (IsElementActive())
    {
        for (TActorIterator<ACMArmPart> It(GetWorld()); It; ++It)
        {
            if (RailMovement->CanArmHold(*It))
            {
                bCanInteract = true;
                break;
            }
        }
    }

    InteractionHighlight->SetHighlighted(bCanInteract);
}
