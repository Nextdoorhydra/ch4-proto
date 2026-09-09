#include "Stage/Trigger/CMLeverBase.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Parts/Arm/CMArmPart.h"
#include "Net/UnrealNetwork.h"
#include "Sound/CMSoundPlayback.h"
#include "Sound/CMSoundTags.h"
#include "Stage/Trigger/Component/CMActivationTriggerComponent.h"
#include "TimerManager.h"

namespace
{
    constexpr float LeverMoveSoundIdleDelay = 0.12f;
}

ACMLeverBase::ACMLeverBase()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    ActivationTrigger->bOneShot = false;
    LeverPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LeverPivot"));
    LeverPivot->SetupAttachment(SceneRoot);
    ArmHoldVolume = CreateDefaultSubobject<USphereComponent>(TEXT("ArmHoldVolume"));
    ArmHoldVolume->SetupAttachment(LeverPivot);
    ArmHoldVolume->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
    ArmHoldVolume->SetSphereRadius(25.0f);
    ArmHoldVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ArmHoldVolume->SetCollisionObjectType(ECC_WorldDynamic);
    ArmHoldVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void ACMLeverBase::BeginPlay()
{
    InitialPivotRotation = LeverPivot->GetRelativeRotation().Quaternion();
    bLeverPoseInitialized = true;
    Super::BeginPlay();
    ActivationTrigger->OnActivated.AddUniqueDynamic(this, &ThisClass::HandleLeverTriggerChanged);
    ActivationTrigger->OnDeactivated.AddUniqueDynamic(this, &ThisClass::HandleLeverTriggerChanged);
    VisualLeverAlpha = LeverAlpha;
    NotifyLeverTargetChanged();
    UpdateVisualRotation(0.0f);
}

void ACMLeverBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, LeverAlpha);
}

bool ACMLeverBase::QueryArmHold_Implementation(ACMArmPart* ArmPart, FCMArmHoldSpec& OutSpec) const
{
    FVector PlanarPullAxis = GetActorTransform()
        .TransformVectorNoScale(LocalPullAxis);
    PlanarPullAxis.Z = 0.0f;
    if (!HasAuthority() || !IsValid(ArmPart) || !ArmPart->IsOperational()
        || !IsElementActive() || HoldingArm.IsValid()
        || (InteractionMode == ECMLeverInteractionMode::LinearPull
            && PlanarPullAxis.IsNearlyZero())
        || (InteractionMode == ECMLeverInteractionMode::WheelRotation
            && LocalRotationAxis.IsNearlyZero())
        || (!ActivationTrigger->IsTriggered() && !ActivationTrigger->CanActivate()))
    {
        return false;
    }
    OutSpec.Priority = 100;
    OutSpec.HoldLocation = ArmHoldVolume->GetComponentLocation();
    OutSpec.HoldNormal = -PlanarPullAxis.GetSafeNormal(
        SMALL_NUMBER, -ArmHoldVolume->GetForwardVector());
    OutSpec.TargetComponent = ArmHoldVolume;
    OutSpec.bUsePhysicsHandle = false;
    return true;
}

bool ACMLeverBase::BeginArmHold_Implementation(ACMArmPart* ArmPart)
{
    FCMArmHoldSpec Spec;
    if (!QueryArmHold_Implementation(ArmPart, Spec))
    {
        return false;
    }
    HoldingArm = ArmPart;
    GrabStartArmLocation = ArmPart->GetActorLocation();
    GrabStartAlpha = LeverAlpha;
    const FVector WorldRotationAxis = GetActorTransform()
        .TransformVectorNoScale(LocalRotationAxis).GetSafeNormal();
    GrabStartWheelDirection = FVector::VectorPlaneProject(
        GrabStartArmLocation - LeverPivot->GetComponentLocation(),
        WorldRotationAxis).GetSafeNormal();
    bTrackingArmHold = true;
    SetActorTickEnabled(true);
    return true;
}

void ACMLeverBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority() && bTrackingArmHold)
    {
        UpdateArmHold();
    }
    UpdateVisualRotation(DeltaSeconds);
}

void ACMLeverBase::UpdateArmHold()
{
    ACMArmPart* Arm = HoldingArm.Get();
    if (!IsValid(Arm) || !Arm->IsOperational() || !Arm->IsHolding() || !IsElementActive())
    {
        StopArmHold();
        return;
    }
    if (IsHoldDistanceExceeded(*Arm))
    {
        StopArmHold();
        return;
    }
    LeverAlpha = CalculateLeverAlphaFromArmLocation(Arm->GetActorLocation());
    const float Threshold = FMath::Clamp(SwitchThreshold, 0.01f, 1.0f);
    if (bRequiresHoldToStayActivated)
    {
        TGuardValue<bool> HoldUpdateGuard(bUpdatingFromHold, true);
        SetLeverPressed(LeverAlpha >= Threshold, Arm);
    }
    else if (LeverAlpha >= Threshold)
    {
        TGuardValue<bool> HoldUpdateGuard(bUpdatingFromHold, true);
        SetLeverPressed(true, Arm);
    }
    else if (LeverAlpha <= -Threshold)
    {
        TGuardValue<bool> HoldUpdateGuard(bUpdatingFromHold, true);
        SetLeverPressed(false, Arm);
    }
    NotifyLeverTargetChanged();
}

bool ACMLeverBase::SetLeverPressed(bool bPressed, AActor* InstigatorActor)
{
    if (!HasAuthority() || !IsElementActive() || ActivationTrigger->IsTriggered() == bPressed)
    {
        return false;
    }
    const bool bChanged = bPressed ? PressButton(InstigatorActor) : ReleaseButton(InstigatorActor);
    if (bChanged && bPressed)
    {
        OnLeverPulled(InstigatorActor);
    }
    return bChanged;
}

void ACMLeverBase::EndArmHold_Implementation(ACMArmPart* ArmPart)
{
    if (HasAuthority() && HoldingArm.Get() == ArmPart)
    {
        StopArmHold();
    }
}

void ACMLeverBase::StopArmHold()
{
    ACMArmPart* Arm = HoldingArm.Get();
    HoldingArm.Reset();
    bTrackingArmHold = false;
    if (IsValid(Arm))
    {
        Arm->EndGroundAnchor();
    }
    if (bRequiresHoldToStayActivated && ActivationTrigger->IsTriggered())
    {
        TGuardValue<bool> HoldUpdateGuard(bUpdatingFromHold, true);
        ReleaseButton(Arm);
    }
    LeverAlpha = ActivationTrigger->IsTriggered() ? 1.0f : -1.0f;
    NotifyLeverTargetChanged();
    ForceNetUpdate();
}

bool ACMLeverBase::IsHoldDistanceExceeded(const ACMArmPart& ArmPart) const
{
    return MaximumHoldDistance > 0.0f
        && FVector::DistSquared2D(
            ArmPart.GetActorLocation(), ArmHoldVolume->GetComponentLocation())
            > FMath::Square(MaximumHoldDistance);
}

float ACMLeverBase::CalculateLeverAlphaFromArmLocation(
    const FVector& ArmLocation) const
{
    if (InteractionMode == ECMLeverInteractionMode::WheelRotation)
    {
        const FVector WorldAxis = GetActorTransform()
            .TransformVectorNoScale(LocalRotationAxis).GetSafeNormal();
        const FVector CurrentDirection = FVector::VectorPlaneProject(
            ArmLocation - LeverPivot->GetComponentLocation(),
            WorldAxis).GetSafeNormal();
        if (WorldAxis.IsNearlyZero() || GrabStartWheelDirection.IsNearlyZero()
            || CurrentDirection.IsNearlyZero())
        {
            return LeverAlpha;
        }

        const float SignedAngle = FMath::RadiansToDegrees(FMath::Atan2(
            FVector::DotProduct(
                WorldAxis,
                FVector::CrossProduct(GrabStartWheelDirection, CurrentDirection)),
            FVector::DotProduct(GrabStartWheelDirection, CurrentDirection)));
        return FMath::Clamp(
            GrabStartAlpha + SignedAngle / FMath::Max(RotationHalfAngle, 1.0f),
            -1.0f,
            1.0f);
    }

    FVector Axis = GetActorTransform().TransformVectorNoScale(LocalPullAxis);
    Axis.Z = 0.0f;
    Axis.Normalize();
    FVector PlanarDelta = ArmLocation - GrabStartArmLocation;
    PlanarDelta.Z = 0.0f;
    const float Distance = FVector::DotProduct(PlanarDelta, Axis);
    return FMath::Clamp(
        GrabStartAlpha + 2.0f * Distance / FMath::Max(FullTravelDistance, 1.0f),
        -1.0f,
        1.0f);
}

void ACMLeverBase::HandleLeverTriggerChanged(AActor* TriggeringActor)
{
    // External button calls synchronize the pose; hold updates retain their intermediate angle.
    if (HasAuthority() && !bUpdatingFromHold)
    {
        StopArmHold();
    }
}

void ACMLeverBase::NotifyLeverTargetChanged()
{
    if (!bLeverPoseInitialized)
    {
        return;
    }
    SetActorTickEnabled(true);
    OnLeverAlphaChanged(LeverAlpha);
}

void ACMLeverBase::UpdateVisualRotation(float DeltaSeconds)
{
    if (!bLeverPoseInitialized)
    {
        return;
    }
    const float PreviousVisualAlpha = VisualLeverAlpha;
    VisualLeverAlpha = RotationTransitionDuration > SMALL_NUMBER
        ? FMath::FInterpConstantTo(VisualLeverAlpha, LeverAlpha, DeltaSeconds, 2.0f / RotationTransitionDuration)
        : LeverAlpha;
    const bool bAtTarget = FMath::IsNearlyEqual(VisualLeverAlpha, LeverAlpha);
    const bool bVisualMoved = !FMath::IsNearlyEqual(
        PreviousVisualAlpha,
        VisualLeverAlpha);
    if (bAtTarget)
    {
        VisualLeverAlpha = LeverAlpha;
    }
    const FVector Axis = LocalRotationAxis.GetSafeNormal(SMALL_NUMBER, FVector::RightVector);
    LeverPivot->SetRelativeRotation(InitialPivotRotation * FQuat(Axis, FMath::DegreesToRadians(VisualLeverAlpha * RotationHalfAngle)));
    if (bVisualMoved)
    {
        StartLeverMoveSound();
        OnLeverVisualAlphaChanged(VisualLeverAlpha);
    }
    if (bLeverMovementSoundActive && bAtTarget
        && FMath::IsNearlyEqual(FMath::Abs(VisualLeverAlpha), 1.0f))
    {
        StopLeverMoveSound();
        PlayLeverSettleSound();
    }
    SetActorTickEnabled((HasAuthority() && bTrackingArmHold) || !bAtTarget);
}

void ACMLeverBase::StartLeverMoveSound()
{
    GetWorldTimerManager().SetTimer(
        LeverMoveSoundStopTimerHandle,
        this,
        &ThisClass::HandleLeverMoveSoundIdle,
        LeverMoveSoundIdleDelay,
        false);

    if (bLeverMovementSoundActive)
    {
        return;
    }

    bLeverMovementSoundActive = true;
    LeverMoveLoopComponent = FCMSoundPlayback::PlayAttachedSFX(
        LeverPivot,
        CMSoundTags::Stage_Lever_MoveLoop);
}

void ACMLeverBase::HandleLeverMoveSoundIdle()
{
    StopLeverMoveSound();
}

void ACMLeverBase::StopLeverMoveSound()
{
    GetWorldTimerManager().ClearTimer(LeverMoveSoundStopTimerHandle);
    bLeverMovementSoundActive = false;
    if (IsValid(LeverMoveLoopComponent))
    {
        LeverMoveLoopComponent->Stop();
        LeverMoveLoopComponent = nullptr;
    }
}

void ACMLeverBase::PlayLeverSettleSound()
{
    FCMSoundPlayback::PlaySFXAtActor(
        this,
        CMSoundTags::Stage_Lever_Settle);
}

void ACMLeverBase::OnRep_LeverAlpha()
{
    NotifyLeverTargetChanged();
}

void ACMLeverBase::HandleElementActiveChanged_Implementation(bool bIsActive)
{
    Super::HandleElementActiveChanged_Implementation(bIsActive);
    if (HasAuthority() && !bIsActive)
    {
        StopArmHold();
    }
}

void ACMLeverBase::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    StopLeverMoveSound();
    StopArmHold();
    VisualLeverAlpha = LeverAlpha;
    UpdateVisualRotation(0.0f);
}

void ACMLeverBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ActivationTrigger->OnActivated.RemoveDynamic(this, &ThisClass::HandleLeverTriggerChanged);
    ActivationTrigger->OnDeactivated.RemoveDynamic(this, &ThisClass::HandleLeverTriggerChanged);
    if (HasAuthority())
    {
        StopArmHold();
    }
    StopLeverMoveSound();
    SetActorTickEnabled(false);
    Super::EndPlay(EndPlayReason);
}

// 레버 명중은 항상 소비한다. 전환 불가도 몸통 당김으로 fallback하지 않는다.
ECMGrabPullResult ACMLeverBase::HandlePullWithResult_Implementation(
    AActor* PullingActor, FVector PullOrigin, float PullStrength)
{
    // Execute the legacy event so existing Blueprint overrides still run.
    LastPullResult = ECMGrabPullResult::Applied;
    return ICMGrabPullTarget::Execute_TryHandlePull(this, PullingActor, PullOrigin, PullStrength)
        ? LastPullResult : ECMGrabPullResult::Unhandled;
}

bool ACMLeverBase::TryHandlePull_Implementation(
    AActor* PullingActor,
    FVector PullOrigin,
    float PullStrength)
{
    LastPullResult = ECMGrabPullResult::Unhandled;
    if (!HasAuthority() || !IsValid(PullingActor))
    {
        return false;
    }

    LastPullResult = ECMGrabPullResult::HandledNoChange;
    if (!IsElementActive() || HoldingArm.IsValid()
        || bRequiresHoldToStayActivated
        || InteractionMode == ECMLeverInteractionMode::WheelRotation
        || !FMath::IsFinite(PullStrength) || PullStrength < RequiredPullStrength
        || PullOrigin.ContainsNaN())
    {
        return true;
    }

    FVector PullDirection = PullOrigin - LeverPivot->GetComponentLocation();
    PullDirection.Z = 0.0f;
    PullDirection.Normalize();
    FVector PullAxis = GetActorTransform().TransformVectorNoScale(LocalPullAxis);
    PullAxis.Z = 0.0f;
    PullAxis.Normalize();
    const float Alignment = FVector::DotProduct(PullDirection, PullAxis);
    if (PullDirection.IsNearlyZero() || PullAxis.IsNearlyZero()
        || FMath::IsNearlyZero(Alignment)
        || FMath::Abs(Alignment) < FMath::Clamp(MinimumPullAlignment, 0.0f, 1.0f))
    {
        return true;
    }

    // 팔이 있는 쪽으로 전환한다. 이미 해당 상태거나 OneShot으로 거절되어도 소비한다.
    const bool bPressed = Alignment > 0.0f;
    if (!SetLeverPressed(bPressed, PullingActor))
    {
        return true;
    }

    // 명령 콜백에서 Reset될 수도 있으므로 최종 트리거 상태를 사용한다.
    LeverAlpha = ActivationTrigger->IsTriggered() ? 1.0f : -1.0f;
    NotifyLeverTargetChanged();
    ForceNetUpdate();
    LastPullResult = ECMGrabPullResult::Applied;
    return true;
}
