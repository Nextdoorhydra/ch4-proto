#include "Parts/Arm/CMSpringArmPart.h"

#include "Data/Part/CMPartLegArmTableRow.h"
#include "Parts/Arm/CMSpringArmHookProjectile.h"
#include "Parts/Combat/CMBattleComponent.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"
#include "Stage/Trigger/CMGrabPullTarget.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraSpringArm, Log, All);

ACMSpringArmPart::ACMSpringArmPart()
{
    PartRowName = TEXT("SpringArm");
    AttackRange = 300.0f;
    AttackRadius = 25.0f;
    HookProjectileClass = ACMSpringArmHookProjectile::StaticClass();
    
    SweepDirectionArrow =
       CreateDefaultSubobject<UArrowComponent>(TEXT("SweepDirectionArrow"));

    SweepDirectionArrow->SetupAttachment(RootComponent);
    SweepDirectionArrow->ArrowLength = 300.0f;
    SweepDirectionArrow->ArrowSize = 2.0f;
    
    PrimaryActorTick.bCanEverTick = true;
}

void ACMSpringArmPart::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMSpringArmPart, ActiveHook);
}

bool ACMSpringArmPart::BeginSwing()
{
    if (!Super::BeginSwing())
    {
        return false;
    }

    if (!LaunchHook())
    {
        Super::EndSwing();
        return false;
    }
    return true;
}

void ACMSpringArmPart::EndSwing()
{
    StopBodyPull();
    if (HasAuthority() && IsValid(ActiveHook))
    {
        ActiveHook->Destroy();
    }
    ActiveHook = nullptr;
    Super::EndSwing();
}

float ACMSpringArmPart::GetSwingDuration() const
{
    const float MaximumFlightTime = ExtensionSpeed > UE_SMALL_NUMBER
        ? AttackRange / ExtensionSpeed
        : 0.0f;
    return FMath::Max(Super::GetSwingDuration(), MaximumFlightTime);
}

void ACMSpringArmPart::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!SweepDirectionArrow)
    {
        return;
    }

    const FVector AimDirection = GetCurrentAimDirection();

    if (!AimDirection.IsNearlyZero())
    {
        SweepDirectionArrow->SetWorldRotation(AimDirection.Rotation());
    }
}

float ACMSpringArmPart::GetExtensionSpeed() const
{
    return ExtensionSpeed;
}

float ACMSpringArmPart::GetPullImpulse() const
{
    return PullImpulse;
}

float ACMSpringArmPart::GetCurrentExtensionLength() const
{
    return IsValid(ActiveHook)
        ? FVector::Distance(GetActorLocation(), ActiveHook->GetActorLocation())
        : 0.0f;
}

float ACMSpringArmPart::GetExtensionRatio() const
{
    return AttackRange > UE_SMALL_NUMBER
        ? FMath::Clamp(GetCurrentExtensionLength() / AttackRange, 0.0f, 1.0f)
        : 0.0f;
}

float ACMSpringArmPart::GetCurrentSweepAngle() const
{
    if (SweepHalfAngle <= 0.0f || SweepSpeed <= 0.0f || !GetWorld())
    {
        return 0.0f;
    }

    const AGameStateBase* GameState = GetWorld()->GetGameState();
    const double SynchronizedTime = GameState
        ? GameState->GetServerWorldTimeSeconds()
        : GetWorld()->GetTimeSeconds();
    return FMath::Sin(
        static_cast<float>(SynchronizedTime) * SweepSpeed
        + GetAutomaticSweepPhase()
    ) * SweepHalfAngle;
}

FVector ACMSpringArmPart::GetCurrentAimDirection() const
{
    UCMPartSlotComponent* PartSlot = GetAttachedPartSlot();

    FVector BaseDirection = GetActorForwardVector();

    if (PartSlot)
    {
        if (USceneComponent* Body = PartSlot->GetAttachParent())
        {
            const FVector ToSlot =
                PartSlot->GetComponentLocation() - Body->GetComponentLocation();

            const FVector BodyRight = Body->GetRightVector();

            const float Side =
                FVector::DotProduct(ToSlot, BodyRight);

            BaseDirection = Side >= 0.0f
                ? BodyRight
                : -BodyRight;
        }
    }

    BaseDirection.Z = 0.0f;

    if (!BaseDirection.Normalize())
    {
        return FVector::ZeroVector;
    }

    return BaseDirection.RotateAngleAxis(
        GetCurrentSweepAngle(),
        FVector::UpVector
    ).GetSafeNormal();
}

float ACMSpringArmPart::GetAutomaticSweepPhase() const
{
    const FCMPartSlotAddress SlotAddress = GetAttachedSlotAddress();
    if (SlotAddress.SegmentIndex < 0 || SlotAddress.PartSlotIndex < 0)
    {
        return 0.0f;
    }

    const int32 FlatSlotIndex =
        SlotAddress.SegmentIndex * CMControl::PartSlotsPerSegment
        + SlotAddress.PartSlotIndex;
    // Golden-angle spacing prevents every SpringArm from pointing in the same
    // direction while remaining deterministic on server and clients.
    return FMath::Fmod(
        static_cast<float>(FlatSlotIndex) * 2.39996323f,
        2.0f * UE_PI
    );
}

bool ACMSpringArmPart::LaunchHook()
{
    UCMPartSlotComponent* PartSlot = GetAttachedPartSlot();
    ACMChimera* Chimera = PartSlot
        ? Cast<ACMChimera>(PartSlot->GetOwner())
        : nullptr;
    if (!HasAuthority() || !Chimera || !GetWorld()
        || !HookProjectileClass || ExtensionSpeed <= 0.0f
        || AttackRange <= 0.0f)
    {
        return false;
    }

    const FVector Direction = GetCurrentAimDirection();
    if (Direction.IsNearlyZero())
    {
        return false;
    }
    
    const float HookRadius = FMath::Max(AttackRadius, 1.0f);

    const FVector SpawnLocation =
        PartSlot->GetComponentLocation()
        + Direction * HookRadius
        + FVector::UpVector * (HookRadius + 30.0f);

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = Chimera;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ActiveHook = GetWorld()->SpawnActor<ACMSpringArmHookProjectile>(
        HookProjectileClass,
        SpawnLocation,
        Direction.Rotation(),
        SpawnParameters
    );
    if (!ActiveHook)
    {
        return false;
    }

    ActiveHook->OnDestroyed.AddDynamic(
        this,
        &ACMSpringArmPart::HandleHookDestroyed
    );

    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(Chimera);
    Chimera->GetAttachedActors(IgnoredActors, false, true);

    // Part actors are attached through replicated slot components. Depending
    // on attachment/replication timing, they are not guaranteed to appear in
    // AActor::GetAttachedActors, so explicitly ignore every Part belonging to
    // this same Chimera. Otherwise the hook damages a sibling Part and treats
    // its nearby impact point as a wall anchor before reaching the real wall.
    for (TActorIterator<ACMPartActorBase> It(GetWorld()); It; ++It)
    {
        ACMPartActorBase* OtherPart = *It;
        UCMPartSlotComponent* OtherSlot = OtherPart
            ? OtherPart->GetAttachedPartSlot()
            : nullptr;
        if (OtherPart && (OtherPart->GetOwner() == Chimera
            || (OtherSlot && OtherSlot->GetOwner() == Chimera)))
        {
            IgnoredActors.AddUnique(OtherPart);
        }
    }

    ActiveHook->InitializeHook(
        this,
        Direction,
        ExtensionSpeed,
        AttackRadius,
        AttackRange,
        IgnoredActors
    );

    UE_LOG(LogChimeraSpringArm, Log,
        TEXT("[SpringArm Launched] Part=%s Range=%.1f Speed=%.1f Radius=%.1f Pull=%.1f SweepAngle=%.1f Direction=%s Spawn=%s"),
        *GetName(),
        AttackRange,
        ExtensionSpeed,
        AttackRadius,
        PullImpulse,
        GetCurrentSweepAngle(),
        *Direction.ToCompactString(),
        *SpawnLocation.ToCompactString());
    return true;
}

void ACMSpringArmPart::ResolveHookHit(const FHitResult& Hit)
{
    if (!HasAuthority() || !IsSwinging())
    {
        return;
    }

    AActor* HitActor = Hit.GetActor();
    UPrimitiveComponent* HitComponent = Hit.GetComponent();
    bool bPulledTarget = false;
    bool bHandledPullTarget = false;
    ECMGrabPullResult PullResult = ECMGrabPullResult::Unhandled;

    if (HitActor)
    {
        if (UCMBattleComponent* TargetBattle =
                HitActor->FindComponentByClass<UCMBattleComponent>())
        {
            ResolveSwingHit(
                TargetBattle,
                Hit.ImpactPoint,
                Hit.ImpactNormal
            );
        }

        if (HitActor->Implements<UCMGrabPullTarget>())
        {
            PullResult = ICMGrabPullTarget::Execute_HandlePullWithResult(
                HitActor,
                this,
                GetActorLocation(),
                PullImpulse);
            bHandledPullTarget = PullResult != ECMGrabPullResult::Unhandled;
            bPulledTarget = PullResult == ECMGrabPullResult::Applied;
        }
    }

    if (!bHandledPullTarget && HitComponent && HitComponent->IsSimulatingPhysics())
    {
        const FVector PullDirection =
            (GetActorLocation() - Hit.ImpactPoint).GetSafeNormal();
        if (!PullDirection.IsNearlyZero() && PullImpulse > 0.0f)
        {
            HitComponent->AddImpulseAtLocation(
                PullDirection * PullImpulse,
                Hit.ImpactPoint
            );
            bPulledTarget = true;
            PullResult = ECMGrabPullResult::Applied;
        }
    }
    else if (!bHandledPullTarget)
    {
        if (UCMPartSlotComponent* PartSlot = GetAttachedPartSlot())
        {
            if (ACMChimera* Chimera = Cast<ACMChimera>(PartSlot->GetOwner()))
            {
                StartBodyPull(
                    Chimera,
                    PartSlot->GetSlotAddress(),
                    Hit.ImpactPoint
                );
            }
        }
    }

    OnHookResolved.Broadcast(HitActor, bPulledTarget);
    OnPullTargetResolved.Broadcast(HitActor, PullResult);
    UE_LOG(LogChimeraSpringArm, Log,
        TEXT("[SpringArm Resolved] Part=%s Hit=%s Mode=%s Point=%s"),
        *GetName(),
        *GetNameSafe(HitActor),
        bPulledTarget ? TEXT("TargetPull")
            : bHandledPullTarget ? TEXT("HandledNoChange") : TEXT("BodyPull"),
        *Hit.ImpactPoint.ToCompactString());
}

void ACMSpringArmPart::CancelBodyPullFromOverride()
{
    if (!HasAuthority())
    {
        return;
    }

    StopBodyPull();

    UE_LOG(
        LogChimeraSpringArm,
        Log,
        TEXT("[SpringArm Pull Overridden] Part=%s"),
        *GetName()
    );

    OnSpringArmFinished.Broadcast();
}

void ACMSpringArmPart::StartBodyPull(
    ACMChimera* Chimera,
    const FCMPartSlotAddress& SlotAddress,
    const FVector& AnchorLocation
)
{
    StopBodyPull();
    if (!HasAuthority() || !Chimera || !GetWorld()
        || PullImpulse <= 0.0f)
    {
        return;
    }
    
    if (!Chimera || !Chimera->RequestSpringArmPull(this))
    {
        return;
    }

    PullTargetChimera = Chimera;
    PullTargetSlot = SlotAddress;
    PullAnchorLocation = AnchorLocation;

    // Apply once immediately so a close wall does not wait for the first
    // timer interval, then continue only while the pull remains active.
    UpdateBodyPull();
    if (PullTargetChimera.IsValid())
    {
        GetWorldTimerManager().SetTimer(
            PullTimerHandle,
            this,
            &ACMSpringArmPart::UpdateBodyPull,
            FMath::Max(PullUpdateInterval, 0.01f),
            true
        );

        UE_LOG(LogChimeraSpringArm, Log,
            TEXT("[SpringArm Tether Started] Part=%s Segment=%d Anchor=%s Rate=%.1f Interval=%.3f StopDistance=%.1f"),
            *GetName(),
            SlotAddress.SegmentIndex,
            *AnchorLocation.ToCompactString(),
            PullImpulse,
            PullUpdateInterval,
            PullStopDistance);
    }
}

void ACMSpringArmPart::UpdateBodyPull()
{
    ACMChimera* Chimera = PullTargetChimera.Get();
    if (!HasAuthority() || !Chimera || !IsSwinging())
    {
        StopBodyPull();
        return;
    }

    const float Interval = FMath::Max(PullUpdateInterval, 0.01f);
    const bool bStillPulling = Chimera->ApplySpringArmPull(
        PullTargetSlot,
        PullAnchorLocation,
        PullImpulse * Interval,
        PullStopDistance
    );
    if (!bStillPulling)
    {
        StopBodyPull();
        OnSpringArmFinished.Broadcast();
    }
}

void ACMSpringArmPart::StopBodyPull()
{
    const bool bWasPulling = PullTargetChimera.IsValid();
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(PullTimerHandle);
    }
    
    if (ACMChimera* Chimera = PullTargetChimera.Get())
    {
        Chimera->ReleaseSpringArmPull(this);
    }
    
    PullTargetChimera.Reset();
    PullTargetSlot = FCMPartSlotAddress();
    PullAnchorLocation = FVector::ZeroVector;

    if (bWasPulling)
    {
        UE_LOG(LogChimeraSpringArm, Log,
            TEXT("[SpringArm Tether Ended] Part=%s"), *GetName());
    }
}

void ACMSpringArmPart::HandleHookDestroyed(AActor* DestroyedActor)
{
    if (DestroyedActor != ActiveHook)
    {
        return;
    }

    ActiveHook = nullptr;

    // 벽에 걸려 Body Pull이 진행 중이라면
    // Projectile이 사라져도 Ability는 계속 유지한다.
    if (PullTargetChimera.IsValid())
    {
        return;
    }

    UE_LOG(LogChimeraSpringArm, Log,
    TEXT("[SpringArm Miss] Part=%s RangeExpired"),
    *GetName());
    
    // 아무것도 못 맞췄거나,
    // 즉시 해결되는 대상에 맞았다면 SpringArm 행동 종료.
    OnSpringArmFinished.Broadcast();

}

void ACMSpringArmPart::ApplyPartData(
    const FCMPartLegArmTableRow& PartRow
)
{
    Super::ApplyPartData(PartRow);
    ExtensionSpeed = FMath::Max(PartRow.ExtensionSpeed, 0.0f);
    PullImpulse = FMath::Max(PartRow.PullImpulse, 0.0f);
    SweepHalfAngle = FMath::Clamp(
        PartRow.SweepHalfAngle,
        0.0f,
        180.0f
    );
    SweepSpeed = FMath::Max(PartRow.SweepSpeed, 0.0f);
}
