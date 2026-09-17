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
    PrimaryComponentTick.TickInterval = 0.0f;
    SetIsReplicatedByDefault(true);
}

void UCMVisionInputComponent::SetActiveControlSlot(int32 SlotIndex)
{
    if (SlotIndex < 0 || !FindHeadForControlSlot(SlotIndex))
    {
        return;
    }

    if (ActiveControlSlotIndex == SlotIndex)
    {
        return;
    }

    ClearLocalAimPredictions();
    ActiveControlSlotIndex = SlotIndex;
    bActiveHeadChanged = true;
    ServerSetActiveControlSlot(SlotIndex);
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

}

void UCMVisionInputComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    ClearLocalAimPredictions();
    Super::EndPlay(EndPlayReason);
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
        ClearLocalAimPredictions();
        return;
    }

    TimeSinceLastAimSend += DeltaTime;

    TArray<ACMHeadPartActor*> ControlledHeads;
    GetControlledHeadParts(ControlledHeads);
    if (ControlledHeads.IsEmpty())
    {
        ClearLocalAimPredictions();
        bHasSentWorldTarget = false;
        bHasLocalAimRotation = false;
        TimeSinceLastAimSend = 0.0f;
        return;
    }

    UCMVisionComponent* ReferenceVision = nullptr;
    for (const ACMHeadPartActor* HeadPart : ControlledHeads)
    {
        UCMVisionComponent* VisionComponent = HeadPart
            ? HeadPart->GetVisionComponent()
            : nullptr;
        if (VisionComponent && VisionComponent->IsVisionActive())
        {
            ReferenceVision = VisionComponent;
            break;
        }
    }
    if (!ReferenceVision)
    {
        ClearLocalAimPredictions();
        bHasSentWorldTarget = false;
        bHasLocalAimRotation = false;
        TimeSinceLastAimSend = 0.0f;
        return;
    }

    FVector MouseRayOrigin;
    FVector MouseRayDirection;
    if (!PlayerController->DeprojectMousePositionToWorld(
            MouseRayOrigin,
            MouseRayDirection)
        || FMath::IsNearlyZero(MouseRayDirection.Z))
    {
        return;
    }

    const FVector VisionOrigin = ReferenceVision->GetVisionOrigin();
    const float IntersectionDistance =
        (VisionOrigin.Z - MouseRayOrigin.Z) / MouseRayDirection.Z;
    if (IntersectionDistance <= 0.0f)
    {
        return;
    }

    const FVector WorldTarget = MouseRayOrigin
        + MouseRayDirection * IntersectionDistance;
    FVector ReferenceDirection = WorldTarget
        - ReferenceVision->GetVisionOrigin();
    ReferenceDirection.Z = 0.0f;
    const float CurrentAimAngleDegrees = FMath::RadiansToDegrees(
        FMath::Atan2(ReferenceDirection.Y, ReferenceDirection.X)
    );
    if (bHasLocalAimRotation)
    {
        LocalAimRotationDegrees += FMath::FindDeltaAngleDegrees(
            LastLocalAimAngleDegrees,
            CurrentAimAngleDegrees
        );
    }
    else
    {
        LocalAimRotationDegrees = CurrentAimAngleDegrees;
        bHasLocalAimRotation = true;
    }
    LastLocalAimAngleDegrees = CurrentAimAngleDegrees;

    TSet<UCMVisionComponent*> CurrentPredictions;
    for (const ACMHeadPartActor* HeadPart : ControlledHeads)
    {
        UCMVisionComponent* VisionComponent = HeadPart
            ? HeadPart->GetVisionComponent()
            : nullptr;
        if (!VisionComponent || !VisionComponent->IsVisionActive())
        {
            continue;
        }

        FVector PredictedDirection =
            WorldTarget - VisionComponent->GetVisionOrigin();
        PredictedDirection.Z = 0.0f;
        VisionComponent->SetLocalPredictedAimDirection(PredictedDirection);
        CurrentPredictions.Add(VisionComponent);
    }
    ReplaceLocalAimPredictions(CurrentPredictions);

    const float SendInterval = FMath::Max(AimUpdateInterval, 0.01f);
    const float HeartbeatInterval = FMath::Max(
        AimHeartbeatInterval,
        SendInterval
    );
    const bool bTargetMoved = !bHasSentWorldTarget
        || FVector::DistSquared2D(WorldTarget, LastSentWorldTarget)
            >= FMath::Square(MinimumTargetMovement);
    const bool bAimRotated = !bHasSentWorldTarget
        || !FMath::IsNearlyEqual(
            LocalAimRotationDegrees,
            LastSentAimRotationDegrees,
            0.01f
        );
    const bool bHeartbeatDue = bHasSentWorldTarget
        && TimeSinceLastAimSend >= HeartbeatInterval;
    const bool bSendIntervalElapsed = !bHasSentWorldTarget
        || TimeSinceLastAimSend >= SendInterval;
    if (!bSendIntervalElapsed
        || (!bTargetMoved && !bAimRotated && !bHeartbeatDue
            && !bActiveHeadChanged))
    {
        return;
    }

    LastSentWorldTarget = WorldTarget;
    LastSentAimRotationDegrees = LocalAimRotationDegrees;
    bHasSentWorldTarget = true;
    bActiveHeadChanged = false;
    TimeSinceLastAimSend = 0.0f;
    ServerUpdateVisionTarget(WorldTarget, LocalAimRotationDegrees);
}

void UCMVisionInputComponent::ServerSetActiveControlSlot_Implementation(
    int32 SlotIndex
)
{
    if (SlotIndex >= 0 && FindHeadForControlSlot(SlotIndex))
    {
        ActiveControlSlotIndex = SlotIndex;
    }
}

void UCMVisionInputComponent::ServerUpdateVisionTarget_Implementation(
    FVector_NetQuantize100 WorldTarget,
    float AimRotationDegrees
)
{
    if (FVector(WorldTarget).ContainsNaN()
        || !FMath::IsFinite(AimRotationDegrees))
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
            FVector(WorldTarget) - VisionComponent->GetVisionOrigin();
        AimDirection.Z = 0.0f;
        VisionComponent->SetNetworkAimDirection(
            AimDirection,
            AimRotationDegrees
        );
        HeadPart->SetProceduralLookRotation(FRotator(
            0.0f,
            FMath::FindDeltaAngleDegrees(
                HeadPart->GetActorRotation().Yaw,
                AimRotationDegrees),
            0.0f));
    }
}

void UCMVisionInputComponent::GetControlledHeadParts(
    TArray<ACMHeadPartActor*>& OutHeadParts
)
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

    if (ActiveControlSlotIndex >= 0)
    {
        if (ACMHeadPartActor* ActiveHead = FindHeadForControlSlot(
                ActiveControlSlotIndex))
        {
            OutHeadParts.Add(ActiveHead);
            return;
        }
    }

    for (int32 SlotIndex = 0;
        SlotIndex < ControlBody->GetControlSlots().Num();
        ++SlotIndex)
    {
        if (ACMHeadPartActor* FirstHead = FindHeadForControlSlot(SlotIndex))
        {
            ActiveControlSlotIndex = SlotIndex;
            OutHeadParts.Add(FirstHead);
            return;
        }
    }
}

ACMHeadPartActor* UCMVisionInputComponent::FindHeadForControlSlot(
    int32 SlotIndex
) const
{
    const ACMPlayerController* PlayerController =
        Cast<ACMPlayerController>(GetOwner());
    const ACMControlBody* ControlBody = PlayerController
        ? PlayerController->GetPawn<ACMControlBody>()
        : nullptr;
    ACMChimera* Chimera = ControlBody
        ? ControlBody->GetSharedChimera()
        : nullptr;
    if (!ControlBody || !Chimera
        || !ControlBody->GetControlSlots().IsValidIndex(SlotIndex))
    {
        return nullptr;
    }

    UCMPartSlotComponent* PartSlot = Chimera->GetPartSlotComponent(
        ControlBody->GetControlSlots()[SlotIndex]);
    return PartSlot
        ? Cast<ACMHeadPartActor>(PartSlot->GetAttachedPart())
        : nullptr;
}

void UCMVisionInputComponent::ReplaceLocalAimPredictions(
    const TSet<UCMVisionComponent*>& CurrentPredictions
)
{
    for (const TWeakObjectPtr<UCMVisionComponent>& PreviousVision
        : LocallyPredictedVisions)
    {
        UCMVisionComponent* VisionComponent = PreviousVision.Get();
        if (VisionComponent && !CurrentPredictions.Contains(VisionComponent))
        {
            VisionComponent->ClearLocalAimPrediction();
        }
    }

    LocallyPredictedVisions.Reset();
    for (UCMVisionComponent* VisionComponent : CurrentPredictions)
    {
        LocallyPredictedVisions.Add(VisionComponent);
    }
}

void UCMVisionInputComponent::ClearLocalAimPredictions()
{
    for (const TWeakObjectPtr<UCMVisionComponent>& PredictedVision
        : LocallyPredictedVisions)
    {
        if (UCMVisionComponent* VisionComponent = PredictedVision.Get())
        {
            VisionComponent->ClearLocalAimPrediction();
        }
    }
    LocallyPredictedVisions.Reset();
}
