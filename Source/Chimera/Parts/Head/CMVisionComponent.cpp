#include "Parts/Head/CMVisionComponent.h"

#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Player/CMPartSlotComponent.h"
#include "Vision/CMVisionManagerSubsystem.h"

UCMVisionComponent::UCMVisionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
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
    DOREPLIFETIME(UCMVisionComponent, NearVisionRadius);
    DOREPLIFETIME(UCMVisionComponent, VisionTint);
    DOREPLIFETIME(UCMVisionComponent, AimDirection);
}

void UCMVisionComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const UWorld* World = GetWorld();
    const bool bPredictionIsCurrent = bHasLocalAimPrediction
        && World
        && World->GetTimeSeconds() - LastLocalPredictionTime
            <= LocalPredictionTimeout;

    if (bHasLocalAimPrediction && !bPredictionIsCurrent)
    {
        bHasLocalAimPrediction = false;
    }

    if (bPredictionIsCurrent)
    {
        // The owning player must see cursor motion without waiting for an RPC.
        RenderedAimDirection = LocalPredictedAimDirection;
        return;
    }

    const FVector TargetDirection = FVector(AimDirection).GetSafeNormal2D();
    if (RemoteAimInterpolationSpeedDegrees <= 0.0f)
    {
        RenderedAimDirection = TargetDirection;
        return;
    }

    RenderedAimDirection = FMath::VInterpNormalRotationTo(
        RenderedAimDirection.GetSafeNormal2D(),
        TargetDirection,
        DeltaTime,
        RemoteAimInterpolationSpeedDegrees
    ).GetSafeNormal2D();
}

void UCMVisionComponent::ConfigureVision(
    float InAngleDegrees,
    float InDistance,
    float InNearVisionRadius
)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    VisionAngleDegrees = FMath::Clamp(InAngleDegrees, 0.0f, 360.0f);
    VisionDistance = FMath::Max(InDistance, 0.0f);
    NearVisionRadius = FMath::Max(InNearVisionRadius, 0.0f);
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
    if (PlanarDirection.Normalize()
        && !PlanarDirection.Equals(FVector(AimDirection), 0.001f))
    {
        AimDirection = PlanarDirection;
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

float UCMVisionComponent::GetNearVisionRadius() const
{
    return NearVisionRadius;
}

void UCMVisionComponent::SetVisionTint(
    const FLinearColor& InColor,
    float InStrength
)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    VisionTint = FLinearColor(
        FMath::Clamp(InColor.R, 0.0f, 1.0f),
        FMath::Clamp(InColor.G, 0.0f, 1.0f),
        FMath::Clamp(InColor.B, 0.0f, 1.0f),
        FMath::Clamp(InStrength, 0.0f, 1.0f)
    );
    GetOwner()->ForceNetUpdate();
}

FLinearColor UCMVisionComponent::GetVisionTint() const
{
    return VisionTint;
}

FVector UCMVisionComponent::GetAimDirection() const
{
    return AimDirection;
}

FVector UCMVisionComponent::GetRenderedAimDirection() const
{
    return RenderedAimDirection;
}

void UCMVisionComponent::SetLocalPredictedAimDirection(
    const FVector& InAimDirection
)
{
    FVector PlanarDirection = InAimDirection;
    PlanarDirection.Z = 0.0f;
    if (!PlanarDirection.Normalize())
    {
        return;
    }

    LocalPredictedAimDirection = PlanarDirection;
    RenderedAimDirection = PlanarDirection;
    bHasLocalAimPrediction = true;
    if (const UWorld* World = GetWorld())
    {
        LastLocalPredictionTime = World->GetTimeSeconds();
    }
}

void UCMVisionComponent::ClearLocalAimPrediction()
{
    bHasLocalAimPrediction = false;
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
    if (!bVisionActive)
    {
        return false;
    }

    if (FVector::DistSquared2D(GetVisionOrigin(), WorldLocation)
        <= FMath::Square(NearVisionRadius))
    {
        return true;
    }

    if (VisionDistance <= 0.0f || VisionAngleDegrees <= 0.0f)
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

    RenderedAimDirection = FVector(AimDirection).GetSafeNormal2D();

    if (UWorld* World = GetWorld())
    {
        if (World->GetNetMode() == NM_DedicatedServer)
        {
            SetComponentTickEnabled(false);
        }

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
