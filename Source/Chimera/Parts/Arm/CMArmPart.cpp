#include "Parts/Arm/CMArmPart.h"

#include "Ability/CMArmGameplayAbility.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/Part/CMPartLegArmTableRow.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Gore/CMDismemberableTarget.h"
#include "Net/UnrealNetwork.h"
#include "Player/CMPartSlotComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraArm, Log, All);

namespace
{
    constexpr float SwingDetectionIntervalSeconds = 0.05f;
}

ACMArmPart::ACMArmPart()
{
    PartType = ECMPartSlotType::Arm;
    GrantedAbilityClass = UCMArmGameplayAbility::StaticClass();
    PartRowName = TEXT("DefaultArm");
}

void ACMArmPart::BeginPlay()
{
    Super::BeginPlay();
    OnPartDied.AddDynamic(this, &ACMArmPart::HandlePartDied);
}

void ACMArmPart::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMArmPart, bSwinging);
    DOREPLIFETIME(ACMArmPart, HoldType);
    DOREPLIFETIME(ACMArmPart, HeldComponent);
    DOREPLIFETIME(ACMArmPart, GroundAnchorLocation);
    DOREPLIFETIME(ACMArmPart, GroundAnchorNormal);
    DOREPLIFETIME(ACMArmPart, SwingStartTime);
}

void ACMArmPart::OnDetachedFromPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
    EndGroundAnchor();
    EndSwing();
    Super::OnDetachedFromPartSlot_Implementation(PartSlot);
}

float ACMArmPart::GetStaminaCost() const
{
    return StaminaCost;
}

float ACMArmPart::GetAnchorStaminaCostPerSecond() const
{
    return AnchorStaminaCostPerSecond;
}

float ACMArmPart::GetSwingDuration() const
{
    return SwingDuration;
}

float ACMArmPart::GetAttackRange() const
{
    return AttackRange;
}

float ACMArmPart::GetAttackRadius() const
{
    return AttackRadius;
}

bool ACMArmPart::IsSwinging() const
{
    return bSwinging;
}

FGuid ACMArmPart::GetCurrentSwingAttackId() const
{
    return CurrentSwingAttackId;
}

float ACMArmPart::GetSwingPhase() const
{
    const UWorld* World = GetWorld();
    return bSwinging && World
        ? FMath::Clamp(
            (World->GetTimeSeconds() - SwingStartTime)
                / FMath::Max(SwingDuration, 0.01f),
            0.0f,
            1.0f)
        : 0.0f;
}

FVector ACMArmPart::GetGroundAnchorLocation() const
{
    // 상호작 대상이 움직일 수 있으므로 저장된 로컬 지점을
    // 매번 현재 컴포넌트 기준 월드 좌표로 복원한다.
    const FVector StoredLocation = GroundAnchorLocation;
    return HoldType == ECMArmHoldType::Interactable
        && IsValid(HeldComponent)
        ? HeldComponent->GetComponentTransform().TransformPosition(
            StoredLocation)
        : StoredLocation;
}

FVector ACMArmPart::GetGroundAnchorNormal() const
{
    const FVector StoredNormal = GroundAnchorNormal;
    return HoldType == ECMArmHoldType::Interactable
        && IsValid(HeldComponent)
        ? HeldComponent->GetComponentTransform().TransformVectorNoScale(
            StoredNormal).GetSafeNormal(SMALL_NUMBER, FVector::UpVector)
        : StoredNormal;
}

void ACMArmPart::BeginGroundAnchor(
    const FVector Location,
    const FVector Normal
)
{
    if (!HasAuthority())
    {
        return;
    }
    HoldType = ECMArmHoldType::Ground;
    HeldComponent = nullptr;
    GroundAnchorLocation = Location;
    GroundAnchorNormal = Normal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    OnGroundAnchorStateChanged.Broadcast(
        true,
        GroundAnchorLocation,
        GroundAnchorNormal);
    ForceNetUpdate();
}

void ACMArmPart::BeginInteractableHold(
    UPrimitiveComponent* TargetComponent,
    const FVector Location,
    const FVector Normal
)
{
    if (!HasAuthority())
    {
        return;
    }

    HoldType = ECMArmHoldType::Interactable;
    HeldComponent = TargetComponent;
    if (HeldComponent)
    {
        // 손 위치와 방향을 대상 로컬 공간에 저장해
        // 물체가 움직이거나 회전해도 IK가 같은 표면을 따라간다.
        const FTransform ComponentTransform = HeldComponent->GetComponentTransform();
        GroundAnchorLocation = ComponentTransform.InverseTransformPosition(Location);
        GroundAnchorNormal = ComponentTransform.InverseTransformVectorNoScale(
            Normal).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    }
    else
    {
        GroundAnchorLocation = Location;
        GroundAnchorNormal = Normal.GetSafeNormal(
            SMALL_NUMBER,
            FVector::UpVector);
    }
    OnGroundAnchorStateChanged.Broadcast(
        true,
        GetGroundAnchorLocation(),
        GetGroundAnchorNormal());
    ForceNetUpdate();
}

void ACMArmPart::EndGroundAnchor()
{
    if (!HasAuthority() || HoldType == ECMArmHoldType::None)
    {
        return;
    }
    // 컴포넌트 참조를 비우기 전에 마지막 월드 손 좌표를 보존한다.
    const FVector LastHoldLocation = GetGroundAnchorLocation();
    const FVector LastHoldNormal = GetGroundAnchorNormal();
    HoldType = ECMArmHoldType::None;
    HeldComponent = nullptr;
    OnGroundAnchorStateChanged.Broadcast(
        false,
        LastHoldLocation,
        LastHoldNormal);
    ForceNetUpdate();
}

bool ACMArmPart::BeginSwing()
{
    if (!HasAuthority() || !IsOperational() || bSwinging
        || !BattleComponent
        || !BattleComponent->BeginParryWindow(SwingDuration))
    {
        return false;
    }

    CurrentSwingAttackId = FGuid::NewGuid();
    SwingStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    bSwinging = true;
    OnSwingStateChanged.Broadcast(true);
    ForceNetUpdate();

    DetectSwingTargets();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            SwingDetectionTimerHandle,
            this,
            &ThisClass::DetectSwingTargets,
            SwingDetectionIntervalSeconds,
            true);
    }

    UE_LOG(LogChimeraArm, Log,
        TEXT("[Arm Swing Started] Part=%s Duration=%.3f AttackId=%s"),
        *GetName(),
        SwingDuration,
        *CurrentSwingAttackId.ToString());
    return true;
}

void ACMArmPart::EndSwing()
{
    if (!HasAuthority())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SwingDetectionTimerHandle);
    }

    if (!bSwinging)
    {
        return;
    }

    if (BattleComponent)
    {
        BattleComponent->EndParryWindow();
    }
    bSwinging = false;
    CurrentSwingAttackId.Invalidate();
    OnSwingStateChanged.Broadcast(false);
    ForceNetUpdate();

    UE_LOG(LogChimeraArm, Log,
        TEXT("[Arm Swing Ended] Part=%s"), *GetName());
}

ECMPartHitResult ACMArmPart::ResolveSwingHit(
    UCMBattleComponent* TargetBattleComponent,
    FVector ImpactPoint,
    FVector ImpactNormal
)
{
    if (!HasAuthority() || !bSwinging || !IsOperational()
        || !TargetBattleComponent
        || TargetBattleComponent == BattleComponent
        || !CurrentSwingAttackId.IsValid())
    {
        return ECMPartHitResult::Invalid;
    }

    FCMPartHitPayload HitPayload;
    HitPayload.Attacker = GetOwner();
    HitPayload.SourceActor = this;
    HitPayload.Damage = GetStrength();
    HitPayload.ImpactPoint = ImpactPoint;
    HitPayload.ImpactNormal = ImpactNormal;
    HitPayload.AttackId = CurrentSwingAttackId;

    const ECMPartHitResult Result =
        TargetBattleComponent->ResolveHit(HitPayload);
    OnSwingHit.Broadcast(Result, TargetBattleComponent->GetOwner());
    return Result;
}

void ACMArmPart::DetectSwingTargets()
{
    UWorld* World = GetWorld();
    if (!HasAuthority() || !bSwinging || !World || !PartMesh
        || AttackRange <= 0.0f || AttackRadius <= 0.0f)
    {
        return;
    }

    const FVector Origin = PartMesh->GetComponentLocation();
    FVector ForwardDirection = PartMesh->GetForwardVector();
    if (const UCMPartSlotComponent* PartSlot = GetAttachedPartSlot())
    {
        if (const USceneComponent* SegmentBody = PartSlot->GetAttachParent())
        {
            const FVector OutwardDirection = FVector::VectorPlaneProject(
                PartSlot->GetComponentLocation()
                    - SegmentBody->GetComponentLocation(),
                SegmentBody->GetUpVector()
            ).GetSafeNormal();
            if (!OutwardDirection.IsNearlyZero())
            {
                ForwardDirection = OutwardDirection;
            }
        }
    }
#if ENABLE_DRAW_DEBUG
    if (bDrawSwingDebug
        && !World->GetTimerManager().IsTimerActive(
            SwingDetectionTimerHandle))
    {
        const FVector SafeForward = ForwardDirection.GetSafeNormal();
        const float HalfAngle = FMath::Atan2(AttackRadius, AttackRange);
        DrawDebugCone(
            World,
            Origin,
            SafeForward,
            AttackRange,
            HalfAngle,
            HalfAngle,
            24,
            FColor::Cyan,
            false,
            SwingDebugDuration,
            0,
            1.5f
        );
        DrawDebugDirectionalArrow(
            World,
            Origin,
            Origin + SafeForward * AttackRange,
            20.0f,
            FColor::Yellow,
            false,
            SwingDebugDuration,
            0,
            2.5f
        );
    }
#endif
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(CMArmSwingSector),
        false,
        this
    );
    if (AActor* OwnerActor = GetOwner())
    {
        QueryParams.AddIgnoredActor(OwnerActor);
    }
    const FCollisionObjectQueryParams ObjectQueryParams(
        FCollisionObjectQueryParams::AllObjects
    );

    World->OverlapMultiByObjectType(
        Overlaps,
        Origin,
        FQuat::Identity,
        ObjectQueryParams,
        FCollisionShape::MakeSphere(AttackRange),
        QueryParams
    );

    TSet<AActor*> DetectedActors;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* TargetActor = Overlap.GetActor();
        UPrimitiveComponent* TargetComponent = Overlap.GetComponent();
        if (!TargetActor || !TargetComponent
            || TargetActor == this
            || TargetActor == GetOwner()
            || TargetActor->GetOwner() == GetOwner()
            || DetectedActors.Contains(TargetActor))
        {
            continue;
        }

        FVector TargetLocation = TargetComponent->Bounds.Origin;
        const float ClosestPointDistance =
            TargetComponent->GetClosestPointOnCollision(
                Origin,
                TargetLocation);
        const bool bOriginInsideTarget = ClosestPointDistance == 0.0f
            && TargetComponent->Bounds.GetBox().IsInsideOrOn(Origin);
        if (!bOriginInsideTarget && !IsInsideSwingSector(
                Origin,
                ForwardDirection,
                TargetLocation,
                AttackRange,
                AttackRadius))
        {
            continue;
        }

        DetectedActors.Add(TargetActor);
        int32 SeveredPartCount = 0;
        if (TargetActor->Implements<UCMDismemberableTarget>())
        {
            FCMDismembermentHitRequest Request;
            Request.Attacker = GetOwner();
            Request.SourcePart = this;
            Request.AttackId = CurrentSwingAttackId;
            Request.ImpactPoint = TargetLocation;
            Request.ImpactDirection = ForwardDirection;
            SeveredPartCount =
                ICMDismemberableTarget::Execute_ReceiveDismembermentHit(
                TargetActor,
                Request);
        }
        OnSwingTargetDetected.Broadcast(TargetActor, TargetLocation);

        UE_LOG(LogChimeraArm, Log,
            TEXT("[Arm Swing Target] Part=%s Target=%s Dismemberable=%s Severed=%d Impact=%s"),
            *GetName(),
            *GetNameSafe(TargetActor),
            TargetActor->Implements<UCMDismemberableTarget>()
                ? TEXT("true")
                : TEXT("false"),
            SeveredPartCount,
            *TargetLocation.ToCompactString());
    }

    UE_LOG(LogChimeraArm, Log,
        TEXT("[Arm Swing Sector] Part=%s Direction=%s Range=%.1f Radius=%.1f Overlaps=%d Detected=%d"),
        *GetName(),
        *ForwardDirection.ToCompactString(),
        AttackRange,
        AttackRadius,
        Overlaps.Num(),
        DetectedActors.Num());
}

bool ACMArmPart::IsInsideSwingSector(
    const FVector& Origin,
    const FVector& ForwardDirection,
    const FVector& TargetLocation,
    float Range,
    float Radius
)
{
    const FVector ToTarget = TargetLocation - Origin;
    const float DistanceSquared = ToTarget.SizeSquared();
    if (DistanceSquared <= UE_SMALL_NUMBER
        || DistanceSquared > FMath::Square(Range))
    {
        return false;
    }

    const FVector SafeForward = ForwardDirection.GetSafeNormal();
    if (SafeForward.IsNearlyZero())
    {
        return false;
    }

    const float CosHalfAngle = Range / FMath::Sqrt(
        FMath::Square(Range) + FMath::Square(Radius)
    );
    return FVector::DotProduct(ToTarget.GetSafeNormal(), SafeForward)
        >= CosHalfAngle;
}

void ACMArmPart::OnRep_Swinging()
{
    OnSwingStateChanged.Broadcast(bSwinging);
}

void ACMArmPart::OnRep_GroundAnchor()
{
    OnGroundAnchorStateChanged.Broadcast(
        IsHolding(),
        GetGroundAnchorLocation(),
        GetGroundAnchorNormal());
}

void ACMArmPart::HandlePartDied()
{
    EndSwing();
}

void ACMArmPart::ApplyPartData(const FCMPartLegArmTableRow& PartRow)
{
    StaminaCost = FMath::Max(PartRow.StaminaCost, 0.0f);
    AnchorStaminaCostPerSecond = FMath::Max(
        PartRow.StaminaPerSecond,
        0.0f
    );
    SwingDuration = FMath::Max(PartRow.ActionDuration, 0.01f);
    AttackRange = FMath::Max(PartRow.AttackRange, 0.0f);
    AttackRadius = FMath::Max(PartRow.AttackRadius, 0.0f);
}
