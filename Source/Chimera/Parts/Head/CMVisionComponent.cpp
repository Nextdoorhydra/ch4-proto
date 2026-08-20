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
    DOREPLIFETIME(UCMVisionComponent, VisionContribution);
    DOREPLIFETIME(UCMVisionComponent, VisionAngleDegrees);
    DOREPLIFETIME(UCMVisionComponent, VisionDistance);
    DOREPLIFETIME(UCMVisionComponent, NearVisionRadius);
    DOREPLIFETIME(UCMVisionComponent, VisionEyeHeightOffset);
    DOREPLIFETIME(UCMVisionComponent, VisionTint);
    DOREPLIFETIME(UCMVisionComponent, AimDirection);
    DOREPLIFETIME(UCMVisionComponent, AimRotationDegrees);
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

    if (RemoteAimInterpolationSpeedDegrees <= 0.0f)
    {
        RenderedAimRotationDegrees = AimRotationDegrees;
        RenderedAimDirection = FVector(AimDirection).GetSafeNormal2D();
        return;
    }

    RenderedAimRotationDegrees = FMath::FInterpConstantTo(
        RenderedAimRotationDegrees,
        AimRotationDegrees,
        DeltaTime,
        FMath::Max(
            RemoteAimInterpolationSpeedDegrees,
            RemoteAimCatchUpSpeedDegrees
        )
    );
    const float RenderedAngleRadians = FMath::DegreesToRadians(
        RenderedAimRotationDegrees
    );
    RenderedAimDirection = FVector(
        FMath::Cos(RenderedAngleRadians),
        FMath::Sin(RenderedAngleRadians),
        0.0f
    );
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

void UCMVisionComponent::SetVisionContribution(
    ECMVisionContribution InContribution
)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()
        || VisionContribution == InContribution)
    {
        return;
    }

    VisionContribution = InContribution;
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
    if (!PlanarDirection.Normalize())
    {
        return;
    }

    const float WrappedAngleDegrees = FMath::RadiansToDegrees(
        FMath::Atan2(PlanarDirection.Y, PlanarDirection.X)
    );
    AimDirection = PlanarDirection;
    AimRotationDegrees = ResolveUnwrappedAimRotation(
        WrappedAngleDegrees,
        AimRotationDegrees
    );
    RefreshRemoteAimCatchUpSpeed();
}

void UCMVisionComponent::SetNetworkAimDirection(
    const FVector& InAimDirection,
    float InAimRotationDegrees
)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()
        || !FMath::IsFinite(InAimRotationDegrees))
    {
        return;
    }

    FVector PlanarDirection = InAimDirection;
    PlanarDirection.Z = 0.0f;
    if (!PlanarDirection.Normalize())
    {
        return;
    }

    const float WrappedAngleDegrees = FMath::RadiansToDegrees(
        FMath::Atan2(PlanarDirection.Y, PlanarDirection.X)
    );
    AimDirection = PlanarDirection;
    AimRotationDegrees = ResolveUnwrappedAimRotation(
        WrappedAngleDegrees,
        InAimRotationDegrees
    );
    RefreshRemoteAimCatchUpSpeed();
    MulticastNetworkAimDirection(AimDirection, AimRotationDegrees);
}

void UCMVisionComponent::MulticastNetworkAimDirection_Implementation(
    FVector_NetQuantizeNormal InAimDirection,
    float InAimRotationDegrees
)
{
    if ((GetOwner() && GetOwner()->HasAuthority())
        || FVector(InAimDirection).ContainsNaN()
        || !FMath::IsFinite(InAimRotationDegrees))
    {
        return;
    }

    AimDirection = InAimDirection;
    AimRotationDegrees = InAimRotationDegrees;
    RefreshRemoteAimCatchUpSpeed();
}

void UCMVisionComponent::OnRep_AimRotationDegrees()
{
    RefreshRemoteAimCatchUpSpeed();
}

void UCMVisionComponent::RefreshRemoteAimCatchUpSpeed()
{
    RemoteAimCatchUpSpeedDegrees = FMath::Abs(
        AimRotationDegrees - RenderedAimRotationDegrees
    ) / FMath::Max(RemoteAimMaximumCatchUpTime, 0.01f);
}

bool UCMVisionComponent::IsVisionActive() const
{
    return bVisionActive;
}

ECMVisionContribution UCMVisionComponent::GetVisionContribution() const
{
    return VisionContribution;
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

void UCMVisionComponent::SetVisionEyeHeightOffset(float InHeightOffset)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    const float ClampedHeightOffset = FMath::Max(InHeightOffset, 0.0f);
    if (FMath::IsNearlyEqual(VisionEyeHeightOffset, ClampedHeightOffset))
    {
        return;
    }

    VisionEyeHeightOffset = ClampedHeightOffset;
    GetOwner()->ForceNetUpdate();
}

float UCMVisionComponent::GetVisionEyeHeightOffset() const
{
    return VisionEyeHeightOffset;
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
    const float WrappedAngleDegrees = FMath::RadiansToDegrees(
        FMath::Atan2(PlanarDirection.Y, PlanarDirection.X)
    );
    RenderedAimRotationDegrees = ResolveUnwrappedAimRotation(
        WrappedAngleDegrees,
        RenderedAimRotationDegrees
    );
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
    // root transform as the stable base, then raise the logical origin to eye
    // height because this character's rendered Head origin is embedded in the floor.
    FVector Origin = PartSlot && PartOwner
        ? PartOwner->GetActorLocation()
        : GetComponentLocation();
    Origin.Z += VisionEyeHeightOffset;
    return Origin;
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

float UCMVisionComponent::ResolveUnwrappedAimRotation(
    float WrappedAngleDegrees,
    float ReferenceRotationDegrees
)
{
    return WrappedAngleDegrees
        + 360.0f * FMath::RoundToFloat(
            (ReferenceRotationDegrees - WrappedAngleDegrees) / 360.0f
        );
}

void UCMVisionComponent::BeginPlay()
{
    Super::BeginPlay();

    RenderedAimRotationDegrees = AimRotationDegrees;
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
