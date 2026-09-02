#include "Parts/Head/CMVisionComponent.h"

#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Player/CMPartSlotComponent.h"
#include "Vision/CMVisionManagerSubsystem.h"
#include "TimerManager.h"

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
    DOREPLIFETIME(UCMVisionComponent, bBlinded);
    DOREPLIFETIME(UCMVisionComponent, BlindnessRecoveryDuration);
    DOREPLIFETIME(UCMVisionComponent, VisionAngleStatusMultiplier);
    DOREPLIFETIME(UCMVisionComponent, VisionDistanceStatusMultiplier);
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

    RenderedVisionAngleMultiplier = FMath::FInterpTo(
        RenderedVisionAngleMultiplier,
        VisionAngleStatusMultiplier,
        DeltaTime,
        StatusInterpolationSpeed);
    RenderedVisionDistanceMultiplier = FMath::FInterpTo(
        RenderedVisionDistanceMultiplier,
        VisionDistanceStatusMultiplier,
        DeltaTime,
        StatusInterpolationSpeed);
    const float BlindnessTarget = bBlinded ? 0.0f : 1.0f;
    RenderedBlindnessMultiplier = bBlinded
        ? 0.0f
        : FMath::FInterpConstantTo(
            RenderedBlindnessMultiplier,
            BlindnessTarget,
            DeltaTime,
            1.0f / FMath::Max(BlindnessRecoveryDuration, 0.01f));

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

void UCMVisionComponent::ApplyBlindness(
    float Duration,
    float Delay,
    float RecoveryDuration)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
    {
        return;
    }

    const float ClampedDuration = FMath::Max(Duration, 0.01f);
    BlindnessRecoveryDuration = FMath::Max(RecoveryDuration, 0.01f);
    if (bBlinded)
    {
        const float RemainingDuration = GetWorld()->GetTimerManager()
            .GetTimerRemaining(BlindnessTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(
            BlindnessTimerHandle,
            this,
            &ThisClass::ClearBlindness,
            FMath::Max(ClampedDuration, RemainingDuration),
            false);
        GetOwner()->ForceNetUpdate();
        return;
    }

    PendingBlindnessDuration = FMath::Max(
        PendingBlindnessDuration, ClampedDuration);
    if (bBlindnessPending)
    {
        return;
    }

    const float ClampedDelay = FMath::Max(Delay, 0.0f);
    if (ClampedDelay <= UE_KINDA_SMALL_NUMBER)
    {
        BeginBlindness();
        return;
    }

    bBlindnessPending = true;
    GetWorld()->GetTimerManager().SetTimer(
        BlindnessDelayTimerHandle,
        this,
        &ThisClass::BeginBlindness,
        ClampedDelay,
        false);
}

void UCMVisionComponent::BeginBlindness()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
    {
        return;
    }

    bBlindnessPending = false;
    const float Duration = FMath::Max(PendingBlindnessDuration, 0.01f);
    PendingBlindnessDuration = 0.0f;
    const float RemainingDuration = GetWorld()->GetTimerManager()
        .GetTimerRemaining(BlindnessTimerHandle);
    bBlinded = true;
    GetWorld()->GetTimerManager().SetTimer(
        BlindnessTimerHandle,
        this,
        &ThisClass::ClearBlindness,
        FMath::Max(Duration, RemainingDuration),
        false);
    GetOwner()->ForceNetUpdate();
}

void UCMVisionComponent::ClearBlindness()
{
    bBlinded = false;
    if (GetOwner())
    {
        GetOwner()->ForceNetUpdate();
    }
}

void UCMVisionComponent::ApplyVisionReduction(
    float Duration,
    float AngleMultiplier,
    float DistanceMultiplier,
    UObject* Source)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || !Source)
    {
        return;
    }

    FActiveVisionReduction* Status = ActiveVisionReductions.FindByPredicate(
        [Source](const FActiveVisionReduction& Candidate)
        {
            return Candidate.Source.Get() == Source;
        });
    if (!Status)
    {
        Status = &ActiveVisionReductions.AddDefaulted_GetRef();
        Status->Handle = NextVisionReductionHandle++;
        Status->Source = Source;
    }
    Status->AngleMultiplier = FMath::Clamp(AngleMultiplier, 0.0f, 1.0f);
    Status->DistanceMultiplier = FMath::Clamp(
        DistanceMultiplier, 0.0f, 1.0f);

    GetWorld()->GetTimerManager().ClearTimer(Status->ExpirationTimer);
    if (Duration > 0.0f)
    {
        FTimerDelegate ExpirationDelegate;
        ExpirationDelegate.BindUObject(
            this,
            &ThisClass::HandleVisionReductionExpired,
            Status->Handle);
        GetWorld()->GetTimerManager().SetTimer(
            Status->ExpirationTimer,
            ExpirationDelegate,
            Duration,
            false);
    }
    RecalculateVisionReduction();
}

void UCMVisionComponent::RemoveVisionReduction(UObject* Source)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
    {
        return;
    }

    FTimerManager& TimerManager = GetWorld()->GetTimerManager();
    const int32 RemovedCount = ActiveVisionReductions.RemoveAll(
        [&TimerManager, Source](FActiveVisionReduction& Status)
        {
            if (Status.Source.Get() != Source)
            {
                return false;
            }
            TimerManager.ClearTimer(Status.ExpirationTimer);
            return true;
        });
    if (RemovedCount > 0)
    {
        RecalculateVisionReduction();
    }
}

void UCMVisionComponent::HandleVisionReductionExpired(int32 Handle)
{
    if (ActiveVisionReductions.RemoveAll(
        [Handle](const FActiveVisionReduction& Status)
        {
            return Status.Handle == Handle;
        }) > 0)
    {
        RecalculateVisionReduction();
    }
}

void UCMVisionComponent::RecalculateVisionReduction()
{
    VisionAngleStatusMultiplier = 1.0f;
    VisionDistanceStatusMultiplier = 1.0f;
    for (const FActiveVisionReduction& Status : ActiveVisionReductions)
    {
        VisionAngleStatusMultiplier = FMath::Min(
            VisionAngleStatusMultiplier, Status.AngleMultiplier);
        VisionDistanceStatusMultiplier = FMath::Min(
            VisionDistanceStatusMultiplier, Status.DistanceMultiplier);
    }
    if (GetOwner())
    {
        GetOwner()->ForceNetUpdate();
    }
}

ECMVisionContribution UCMVisionComponent::GetVisionContribution() const
{
    return VisionContribution;
}

float UCMVisionComponent::GetVisionAngleDegrees() const
{
    return VisionAngleDegrees * RenderedVisionAngleMultiplier
        * RenderedBlindnessMultiplier;
}

float UCMVisionComponent::GetVisionDistance() const
{
    return VisionDistance * RenderedVisionDistanceMultiplier
        * RenderedBlindnessMultiplier;
}

float UCMVisionComponent::GetNearVisionRadius() const
{
    return NearVisionRadius * RenderedBlindnessMultiplier;
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
    if (!IsVisionActive())
    {
        return false;
    }

    if (FVector::DistSquared2D(GetVisionOrigin(), WorldLocation)
        <= FMath::Square(NearVisionRadius))
    {
        return true;
    }

    const float EffectiveDistance = GetVisionDistance();
    const float EffectiveAngle = GetVisionAngleDegrees();
    if (EffectiveDistance <= 0.0f || EffectiveAngle <= 0.0f)
    {
        return false;
    }

    return IsPointInsideVisionCone(
        GetVisionOrigin(),
        AimDirection,
        EffectiveAngle,
        EffectiveDistance,
        WorldLocation
    );
}

bool UCMVisionComponent::IsLocationInsideVisionCone(
    const FVector& WorldLocation) const
{
    return IsVisionActive() && IsPointInsideVisionCone(
        GetVisionOrigin(),
        AimDirection,
        GetVisionAngleDegrees(),
        GetVisionDistance(),
        WorldLocation);
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
        World->GetTimerManager().ClearTimer(BlindnessTimerHandle);
        for (FActiveVisionReduction& Status : ActiveVisionReductions)
        {
            World->GetTimerManager().ClearTimer(Status.ExpirationTimer);
        }
    }
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
