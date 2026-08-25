#include "Parts/Arm/CMArmPart.h"

#include "Ability/CMArmGameplayAbility.h"
#include "Components/StaticMeshComponent.h"
#include "Data/Part/CMPartLegArmTableRow.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/CMPartSlotComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraArm, Log, All);

ACMArmPart::ACMArmPart()
{
    PartType = ECMPartSlotType::Arm;
    GrantedAbilityClass = UCMArmGameplayAbility::StaticClass();
    MovementImpulseMultiplier = 0.1f;
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
}

void ACMArmPart::OnDetachedFromPartSlot_Implementation(
    UCMPartSlotComponent* PartSlot
)
{
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

bool ACMArmPart::BeginSwing()
{
    if (!HasAuthority() || !IsOperational() || bSwinging
        || !BattleComponent
        || !BattleComponent->BeginParryWindow(SwingDuration))
    {
        return false;
    }

    CurrentSwingAttackId = FGuid::NewGuid();
    bSwinging = true;
    OnSwingStateChanged.Broadcast(true);
    ForceNetUpdate();

    DetectSwingTargets();

    UE_LOG(LogChimeraArm, Log,
        TEXT("[Arm Swing Started] Part=%s Duration=%.3f AttackId=%s"),
        *GetName(),
        SwingDuration,
        *CurrentSwingAttackId.ToString());
    return true;
}

void ACMArmPart::EndSwing()
{
    if (!HasAuthority() || !bSwinging)
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
    if (!HasAuthority() || !World || !PartMesh
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
    if (bDrawSwingDebug)
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

        const FVector TargetLocation = TargetComponent->Bounds.Origin;
        if (!IsInsideSwingSector(
                Origin,
                ForwardDirection,
                TargetLocation,
                AttackRange,
                AttackRadius))
        {
            continue;
        }

        DetectedActors.Add(TargetActor);
        OnSwingTargetDetected.Broadcast(TargetActor, TargetLocation);
    }

    UE_LOG(LogChimeraArm, Verbose,
        TEXT("[Arm Swing Sector] Part=%s Direction=%s Range=%.1f Radius=%.1f Detected=%d"),
        *GetName(),
        *ForwardDirection.ToCompactString(),
        AttackRange,
        AttackRadius,
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
