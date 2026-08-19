#include "Parts/Arm/CMArmPart.h"

#include "Ability/CMArmGameplayAbility.h"
#include "Data/Part/CMPartLegArmTableRow.h"
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
    SwingDuration = FMath::Max(PartRow.ActionDuration, 0.01f);
    AttackRange = FMath::Max(PartRow.AttackRange, 0.0f);
    AttackRadius = FMath::Max(PartRow.AttackRadius, 0.0f);
}
