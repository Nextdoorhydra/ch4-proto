#include "Parts/Head/CMVisionComponent.h"

#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Player/CMPartSlotComponent.h"
#include "Vision/CMVisionManagerSubsystem.h"

UCMVisionComponent::UCMVisionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCMVisionComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCMVisionComponent, bVisionActive);
    DOREPLIFETIME(UCMVisionComponent, VisionAngleDegrees);
    DOREPLIFETIME(UCMVisionComponent, VisionDistance);
    DOREPLIFETIME(UCMVisionComponent, AimDirection);
}

void UCMVisionComponent::ConfigureVision(
    float InAngleDegrees,
    float InDistance
)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    VisionAngleDegrees = FMath::Clamp(InAngleDegrees, 0.0f, 360.0f);
    VisionDistance = FMath::Max(InDistance, 0.0f);
    GetOwner()->ForceNetUpdate();
}

void UCMVisionComponent::SetVisionActive(bool bInActive)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()
        || bVisionActive == bInActive)
    {
        return;
    }

    bVisionActive = bInActive;
    GetOwner()->ForceNetUpdate();
}

void UCMVisionComponent::SetAimDirection(const FVector& InAimDirection)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    FVector PlanarDirection = InAimDirection;
    PlanarDirection.Z = 0.0f;
    if (PlanarDirection.Normalize())
    {
        AimDirection = PlanarDirection;
        GetOwner()->ForceNetUpdate();
    }
}

bool UCMVisionComponent::IsVisionActive() const
{
    return bVisionActive;
}

float UCMVisionComponent::GetVisionAngleDegrees() const
{
    return VisionAngleDegrees;
}

float UCMVisionComponent::GetVisionDistance() const
{
    return VisionDistance;
}

FVector UCMVisionComponent::GetAimDirection() const
{
    return AimDirection;
}

FVector UCMVisionComponent::GetVisionOrigin() const
{
    const ACMPartActorBase* PartOwner = Cast<ACMPartActorBase>(GetOwner());
    const UCMPartSlotComponent* PartSlot = PartOwner
        ? PartOwner->GetAttachedPartSlot()
        : nullptr;

    // The Part root is snapped to the slot when equipped. Use that replicated
    // root transform so the rendered Head and its vision always share an
    // origin, even while the physics-driven slot transform is between updates.
    return PartSlot && PartOwner
        ? PartOwner->GetActorLocation()
        : GetComponentLocation();
}

bool UCMVisionComponent::IsLocationVisible(
    const FVector& WorldLocation
) const
{
    if (!bVisionActive || VisionDistance <= 0.0f
        || VisionAngleDegrees <= 0.0f)
    {
        return false;
    }

    return IsPointInsideVisionCone(
        GetVisionOrigin(),
        AimDirection,
        VisionAngleDegrees,
        VisionDistance,
        WorldLocation
    );
}

bool UCMVisionComponent::IsPointInsideVisionCone(
    const FVector& Origin,
    const FVector& Direction,
    float AngleDegrees,
    float Distance,
    const FVector& WorldLocation
)
{
    FVector ToLocation = WorldLocation - Origin;
    ToLocation.Z = 0.0f;
    const float DistanceSquared = ToLocation.SizeSquared();
    if (DistanceSquared > FMath::Square(Distance))
    {
        return false;
    }

    if (DistanceSquared <= UE_KINDA_SMALL_NUMBER
        || AngleDegrees >= 360.0f)
    {
        return true;
    }

    const float MinimumDot = FMath::Cos(
        FMath::DegreesToRadians(AngleDegrees * 0.5f)
    );
    return FVector::DotProduct(
        ToLocation.GetSafeNormal(),
        Direction.GetSafeNormal2D()
    ) >= MinimumDot;
}

void UCMVisionComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UWorld* World = GetWorld())
    {
        if (UCMVisionManagerSubsystem* VisionManager =
            World->GetSubsystem<UCMVisionManagerSubsystem>())
        {
            VisionManager->RegisterVisionSource(this);
        }
    }
}

void UCMVisionComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (UWorld* World = GetWorld())
    {
        if (UCMVisionManagerSubsystem* VisionManager =
            World->GetSubsystem<UCMVisionManagerSubsystem>())
        {
            VisionManager->UnregisterVisionSource(this);
        }
    }

    Super::EndPlay(EndPlayReason);
}
