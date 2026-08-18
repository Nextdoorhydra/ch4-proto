#include "Vision/CMVisionInputComponent.h"

#include "Player/CMChimera.h"
#include "Player/CMControlBody.h"
#include "Player/CMPartSlotComponent.h"
#include "Player/CMPlayerController.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Parts/Head/CMVisionComponent.h"

UCMVisionInputComponent::UCMVisionInputComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = AimUpdateInterval;
    SetIsReplicatedByDefault(true);
}

void UCMVisionInputComponent::BeginPlay()
{
    Super::BeginPlay();

    ACMPlayerController* PlayerController =
        Cast<ACMPlayerController>(GetOwner());
    const bool bLocallyControlled = PlayerController
        && PlayerController->IsLocalController();
    SetComponentTickEnabled(bLocallyControlled);

    if (bLocallyControlled && bShowMouseCursor)
    {
        PlayerController->bShowMouseCursor = true;
    }

    PrimaryComponentTick.TickInterval = FMath::Max(
        AimUpdateInterval,
        0.01f
    );
}

void UCMVisionInputComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ACMPlayerController* PlayerController =
        Cast<ACMPlayerController>(GetOwner());
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        return;
    }

    TArray<ACMHeadPartActor*> ControlledHeads;
    GetControlledHeadParts(ControlledHeads);
    if (ControlledHeads.IsEmpty())
    {
        return;
    }

    FHitResult CursorHit;
    if (!PlayerController->GetHitResultUnderCursor(
        VisionGroundTraceChannel,
        false,
        CursorHit))
    {
        return;
    }

    const FVector WorldTarget = CursorHit.ImpactPoint;
    if (bHasSentWorldTarget
        && FVector::DistSquared2D(WorldTarget, LastSentWorldTarget)
            < FMath::Square(MinimumTargetMovement))
    {
        return;
    }

    LastSentWorldTarget = WorldTarget;
    bHasSentWorldTarget = true;
    ServerUpdateVisionTarget(WorldTarget);
}

void UCMVisionInputComponent::ServerUpdateVisionTarget_Implementation(
    FVector_NetQuantize100 WorldTarget
)
{
    if (FVector(WorldTarget).ContainsNaN())
    {
        return;
    }

    TArray<ACMHeadPartActor*> ControlledHeads;
    GetControlledHeadParts(ControlledHeads);
    for (ACMHeadPartActor* HeadPart : ControlledHeads)
    {
        UCMVisionComponent* VisionComponent = HeadPart
            ? HeadPart->GetVisionComponent()
            : nullptr;
        if (!VisionComponent || !VisionComponent->IsVisionActive())
        {
            continue;
        }

        FVector AimDirection =
            FVector(WorldTarget) - VisionComponent->GetComponentLocation();
        AimDirection.Z = 0.0f;
        VisionComponent->SetAimDirection(AimDirection);
    }
}

void UCMVisionInputComponent::GetControlledHeadParts(
    TArray<ACMHeadPartActor*>& OutHeadParts
) const
{
    OutHeadParts.Reset();

    const ACMPlayerController* PlayerController =
        Cast<ACMPlayerController>(GetOwner());
    const ACMControlBody* ControlBody = PlayerController
        ? PlayerController->GetPawn<ACMControlBody>()
        : nullptr;
    ACMChimera* Chimera = ControlBody
        ? ControlBody->GetSharedChimera()
        : nullptr;
    if (!ControlBody || !Chimera)
    {
        return;
    }

    for (const FCMPartSlotAddress& SlotAddress
        : ControlBody->GetControlSlots())
    {
        UCMPartSlotComponent* PartSlot =
            Chimera->GetPartSlotComponent(SlotAddress);
        ACMHeadPartActor* HeadPart = PartSlot
            ? Cast<ACMHeadPartActor>(PartSlot->GetAttachedPart())
            : nullptr;
        if (HeadPart)
        {
            OutHeadParts.AddUnique(HeadPart);
        }
    }
}
