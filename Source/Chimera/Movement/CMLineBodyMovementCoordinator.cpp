#include "Movement/CMLineBodyMovementCoordinator.h"

#include "Ability/CMChimeraAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Player/CMControlTypes.h"
#include "DrawDebugHelpers.h"
#include "Parts/Arm/CMArmHoldTarget.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Leg/CMLegPart.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"
#include "Player/CMPlayerState.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraMovement, Log, All);

UCMLineBodyMovementCoordinator::UCMLineBodyMovementCoordinator()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UCMLineBodyMovementCoordinator::TryActivateLeg(
    ACMChimera& Chimera,
    ACMLegPart& LegPart,
    ACMPlayerState* ContributingPlayerState,
    float MovementImpulse,
    bool bReverseMovement
)
{
    UCMPartSlotComponent* PartSlot = LegPart.GetAttachedPartSlot();
    const FCMPartSlotAddress SlotAddress = LegPart.GetAttachedSlotAddress();
    const int32 SegmentIndex = SlotAddress.SegmentIndex;
    const bool bVisualReplantInProgress =
        LegPart.GetPlantTrigger() == ECMLegPlantTrigger::ReachRecovery
        && LegPart.GetStepDirection() == ECMLegStepDirection::None
        && (LegPart.GetPlantState() == ECMLegPlantState::Swing
            || LegPart.GetPlantState() == ECMLegPlantState::Landing);
    if (!Chimera.HasAuthority()
        || !LegPart.IsOperational()
        || ((!bVisualReplantInProgress)
            && (LegPart.GetPlantState() == ECMLegPlantState::Swing
                || LegPart.GetPlantState() == ECMLegPlantState::Landing
                || LegPart.GetPlantState() == ECMLegPlantState::Recover))
        || !PartSlot
        || PartSlot->GetOwner() != &Chimera
        || MovementImpulse <= 0.0f
        || SegmentIndex < 0
        || SegmentIndex >= Chimera.ActiveSegmentCount
        || !Chimera.BodySegments.IsValidIndex(SegmentIndex))
    {
        UE_LOG(LogChimeraMovement, Warning,
            TEXT("[Leg Step Rejected] Preconditions Part=%s Authority=%s Operational=%s Slot=%s SlotOwner=%s Impulse=%.1f Segment=%d ActiveSegments=%d Alive=%s"),
            *GetNameSafe(&LegPart),
            Chimera.HasAuthority() ? TEXT("true") : TEXT("false"),
            LegPart.IsOperational() ? TEXT("true") : TEXT("false"),
            *GetNameSafe(PartSlot),
            *GetNameSafe(PartSlot ? PartSlot->GetOwner() : nullptr),
            MovementImpulse,
            SegmentIndex,
            Chimera.ActiveSegmentCount,
            Chimera.IsSegmentAlive(SegmentIndex)
                ? TEXT("true")
                : TEXT("false"));
        return false;
    }

    const bool bAlreadyActive = ActiveLegSteps.ContainsByPredicate(
        [&LegPart](const FActiveLegStep& Step)
        {
            return Step.LegPart.Get() == &LegPart;
        }
    );
    UBoxComponent* SegmentBody = Chimera.BodySegments[SegmentIndex];
    if (bAlreadyActive || !SegmentBody
        || !SegmentBody->IsSimulatingPhysics())
    {
        UE_LOG(LogChimeraMovement, Warning,
            TEXT("[Leg Step Rejected] Physics Part=%s Segment=%d AlreadyActive=%s Body=%s Simulating=%s"),
            *GetNameSafe(&LegPart),
            SegmentIndex,
            bAlreadyActive ? TEXT("true") : TEXT("false"),
            *GetNameSafe(SegmentBody),
            SegmentBody && SegmentBody->IsSimulatingPhysics()
                ? TEXT("true")
                : TEXT("false"));
        return false;
    }

    FVector ForwardDirection = PartSlot->GetForwardVector();
    ForwardDirection.Z = 0.0f;
    if (!ForwardDirection.Normalize())
    {
        UE_LOG(LogChimeraMovement, Warning,
            TEXT("[Leg Step Rejected] Invalid slot Forward Part=%s Slot=%s"),
            *GetNameSafe(&LegPart),
            *GetNameSafe(PartSlot));
        return false;
    }
    const float DirectionSign = bReverseMovement ? -1.0f : 1.0f;
    ForwardDirection *= DirectionSign;

    const FVector VirtualFootPoint =
        PartSlot->GetComponentLocation()
        + ForwardDirection * Chimera.LegStepLength;
    FHitResult GroundHit;
    if (!TraceGroundAtPoint(
        Chimera,
        VirtualFootPoint,
        &LegPart,
        GroundHit))
    {
        UE_LOG(LogChimeraMovement, Warning,
            TEXT("[Leg Step Rejected] No ground Part=%s VirtualFoot=%s TraceHeight=%.1f TraceDepth=%.1f Radius=%.1f Channel=%d MinimumNormalZ=%.2f"),
            *GetNameSafe(&LegPart),
            *VirtualFootPoint.ToCompactString(),
            Chimera.LegStepTraceHeight,
            Chimera.LegStepTraceDepth,
            Chimera.GroundCheckRadius,
            static_cast<int32>(Chimera.GroundTraceChannel.GetValue()),
            Chimera.MinimumGroundNormalZ);
        return false;
    }

    // GroundNormal에 투영한 접선 방향은 경사면에서 큰 위쪽 Force 성분을
    // 만들어 좌우 다리를 함께 누를 때 몸 전체를 띄웠다. 추진력은 수평으로
    // 유지하고, 실제 높이 변화는 몸통과 경사면의 충돌 반응에 맡긴다.
    const FVector PushDirection = ForwardDirection;
    const float PushDuration = FMath::Max(
        LegPart.GetActionDuration(),
        0.01f
    );
    const float PushForceMagnitude =
        MovementImpulse
        * GetPerControlImpulseMultiplier(Chimera)
        * FMath::Max(Chimera.LegStepForceScale, 0.0f)
        / PushDuration;
    if (PushDirection.IsNearlyZero()
        || PushForceMagnitude <= UE_SMALL_NUMBER)
    {
        UE_LOG(LogChimeraMovement, Warning,
            TEXT("[Leg Step Rejected] Invalid force Part=%s Direction=%s Impulse=%.1f PerControl=%.3f StepScale=%.3f Duration=%.3f Final=%.1f"),
            *GetNameSafe(&LegPart),
            *PushDirection.ToCompactString(),
            MovementImpulse,
            GetPerControlImpulseMultiplier(Chimera),
            Chimera.LegStepForceScale,
            PushDuration,
            PushForceMagnitude);
        return false;
    }

    FActiveLegStep& Step = ActiveLegSteps.AddDefaulted_GetRef();
    Step.LegPart = &LegPart;
    Step.SegmentBody = SegmentBody;
    Step.SegmentIndex = SegmentIndex;
    Step.VirtualFootPoint = VirtualFootPoint;
    Step.GroundPoint = GroundHit.ImpactPoint;
    Step.GroundNormal = GroundHit.ImpactNormal;
    Step.PushForce = PushDirection * PushForceMagnitude;
    Step.EndTime = Chimera.GetWorld()->GetTimeSeconds() + PushDuration;
    Step.bOwnsVisualStep = !bVisualReplantInProgress;
    if (Step.bOwnsVisualStep)
    {
        LegPart.BeginProceduralStep(
            bReverseMovement,
            GroundHit.ImpactPoint,
            GroundHit.ImpactNormal,
            PushDuration);
    }

    // 개별 Step Force는 해당 마디의 회전과 접지 이동을 담당한다.
    // 같은 시간창에 좌우 다리가 함께 눌렸을 때만 기존 Rolling Match가
    // 전진 보정을 만들도록, 이번 Step의 총 평면 충격량을 등록한다.
    FVector EquivalentPlanarImpulse = Step.PushForce * PushDuration;
    EquivalentPlanarImpulse.Z = 0.0f;
    RegisterCooperativeInput(
        Chimera,
        SlotAddress,
        ContributingPlayerState,
        EquivalentPlanarImpulse,
        DirectionSign
    );

#if ENABLE_DRAW_DEBUG
    if (Chimera.bDrawGroundContactDebug)
    {
        DrawDebugSphere(
            Chimera.GetWorld(),
            VirtualFootPoint,
            Chimera.GroundCheckRadius,
            12,
            FColor::Cyan,
            false,
            PushDuration,
            0,
            1.5f
        );
        DrawDebugSphere(
            Chimera.GetWorld(),
            GroundHit.ImpactPoint,
            Chimera.GroundCheckRadius,
            12,
            FColor::Green,
            false,
            PushDuration,
            0,
            2.0f
        );
        DrawDebugDirectionalArrow(
            Chimera.GetWorld(),
            GroundHit.ImpactPoint,
            GroundHit.ImpactPoint + PushDirection * 100.0f,
            20.0f,
            FColor::Red,
            false,
            PushDuration,
            0,
            2.0f
        );
    }
#endif

    UE_LOG(LogChimeraMovement, Log,
        TEXT("[Leg Step Started] PlayerState=%s Part=%s Segment=%d Slot=(%d,%d) Direction=%s Duration=%.3f Force=%.1f Ground=%s"),
        *GetNameSafe(ContributingPlayerState),
        *GetNameSafe(&LegPart),
        SegmentIndex,
        SlotAddress.SegmentIndex,
        SlotAddress.PartSlotIndex,
        bReverseMovement ? TEXT("Reverse") : TEXT("Forward"),
        PushDuration,
        PushForceMagnitude,
        *GroundHit.ImpactPoint.ToCompactString());
    return true;
}

void UCMLineBodyMovementCoordinator::CancelLegStep(
    ACMLegPart* LegPart
)
{
    if (!LegPart)
    {
        return;
    }

    const int32 RemovedCount = ActiveLegSteps.RemoveAll(
        [LegPart](const FActiveLegStep& Step)
        {
            return Step.LegPart.Get() == LegPart;
        }
    );
    if (RemovedCount > 0)
    {
        LegPart->CancelProceduralStep();
    }
    if (RemovedCount > 0)
    {
        UE_LOG(LogChimeraMovement, Log,
            TEXT("[Leg Step Cancelled] Part=%s"),
            *GetNameSafe(LegPart));
    }
}

bool UCMLineBodyMovementCoordinator::TryActivateArm(
    ACMChimera& Chimera,
    int32 SegmentIndex,
    USceneComponent* ImpulsePoint,
    ACMPlayerState* ContributingPlayerState,
    float MovementImpulse,
    float MovementImpulseMultiplier
)
{
    if (!Chimera.HasAuthority()
        || SegmentIndex < 0
        || SegmentIndex >= Chimera.ActiveSegmentCount
        || !Chimera.BodySegments.IsValidIndex(SegmentIndex))
    {
        return false;
    }

    return ApplyArmImpulse(
        Chimera,
        Chimera.BodySegments[SegmentIndex],
        ImpulsePoint,
        ContributingPlayerState,
        MovementImpulse,
        MovementImpulseMultiplier
    );
}

bool UCMLineBodyMovementCoordinator::ApplyAnchorPull(
    ACMChimera& Chimera,
    int32 SegmentIndex,
    const FVector& AnchorLocation,
    float PullImpulse,
    float StopDistance
)
{
    if (!Chimera.HasAuthority()
        || PullImpulse <= 0.0f
        || SegmentIndex < 0
        || SegmentIndex >= Chimera.ActiveSegmentCount
        || !Chimera.BodySegments.IsValidIndex(SegmentIndex))
    {
        return false;
    }

    float TotalMass = 0.0f;
    TArray<UBoxComponent*> SimulatedSegments;
    for (int32 Index = 0; Index < Chimera.ActiveSegmentCount; ++Index)
    {
        UBoxComponent* BodySegment =
            Chimera.BodySegments.IsValidIndex(Index)
            ? Chimera.BodySegments[Index]
            : nullptr;
        if (!BodySegment || !BodySegment->IsSimulatingPhysics())
        {
            continue;
        }

        const float SegmentMass = FMath::Max(BodySegment->GetMass(), 0.01f);
        TotalMass += SegmentMass;
        SimulatedSegments.Add(BodySegment);
    }
    if (SimulatedSegments.IsEmpty() || TotalMass <= UE_SMALL_NUMBER)
    {
        return false;
    }

    // The hook only needs to bring its attached segment to the anchor. Using
    // the whole chain's center makes long bodies pull forever at a wall.
    UBoxComponent* SourceSegment = Chimera.BodySegments[SegmentIndex];
    const FVector AnchorOffset =
        AnchorLocation - SourceSegment->GetCenterOfMass();
    if (AnchorOffset.Size() <= FMath::Max(StopDistance, 0.0f))
    {
        return false;
    }

    const FVector PullDirection = AnchorOffset.GetSafeNormal();
    if (PullDirection.IsNearlyZero())
    {
        return false;
    }

    // Distribute one total impulse by mass. Every segment receives the same
    // velocity change, so the constraint chain translates without an
    // artificial yaw torque from pulling only one segment.
    for (UBoxComponent* BodySegment : SimulatedSegments)
    {
        const float MassFraction = BodySegment->GetMass() / TotalMass;
        BodySegment->AddImpulse(
            PullDirection * PullImpulse * MassFraction
        );
    }

    UE_LOG(LogChimeraMovement, Verbose,
        TEXT("[SpringArm Body Pull] SourceSegment=%d SegmentCount=%d Anchor=%s TotalImpulse=%.1f"),
        SegmentIndex,
        SimulatedSegments.Num(),
        *AnchorLocation.ToCompactString(),
        PullImpulse);
    return true;
}

bool UCMLineBodyMovementCoordinator::ApplyArmImpulse(
    ACMChimera& Chimera,
    UBoxComponent* SegmentBody,
    USceneComponent* ImpulsePoint,
    ACMPlayerState* ContributingPlayerState,
    float MovementImpulse,
    float MovementImpulseMultiplier
)
{
    if (!SegmentBody || !ImpulsePoint
        || MovementImpulse <= 0.0f
        || MovementImpulseMultiplier <= 0.0f)
    {
        return false;
    }

    FVector ForwardDirection = SegmentBody->GetForwardVector();
    ForwardDirection.Z = 0.0f;
    if (!ForwardDirection.Normalize())
    {
        return false;
    }

    const FVector Impulse = ForwardDirection
        * MovementImpulse
        * GetPerControlImpulseMultiplier(Chimera);
    const float TranslationFraction = FMath::Clamp(
        Chimera.IndividualPlanarTranslationFraction,
        0.0f,
        1.0f
    );
    const float RotationFraction = FMath::Clamp(
        Chimera.IndividualYawRotationFraction,
        0.0f,
        1.0f
    );
    const FVector ImpulseLocation = ImpulsePoint->GetComponentLocation();
    SegmentBody->AddImpulseAtLocation(
        Impulse * RotationFraction,
        ImpulseLocation
    );
    SegmentBody->AddImpulse(
        Impulse * (TranslationFraction - RotationFraction)
    );

    if (const UCMPartSlotComponent* PartSlot =
            Cast<UCMPartSlotComponent>(ImpulsePoint))
    {
        ApplyWholeBodyYawAssist(
            Chimera,
            PartSlot->GetSlotAddress(),
            MovementImpulseMultiplier
        );
        RegisterCooperativeInput(
            Chimera,
            PartSlot->GetSlotAddress(),
            ContributingPlayerState,
            Impulse
        );
    }

    UE_LOG(LogChimeraMovement, Log,
        TEXT("[Arm Impulse] Segment=%d Scale=%.2f PartImpulse=%.1f Final=%.1f"),
        Chimera.BodySegments.IndexOfByKey(SegmentBody),
        MovementImpulseMultiplier,
        MovementImpulse,
        Impulse.Size());
    return true;
}

void UCMLineBodyMovementCoordinator::RegisterCooperativeInput(
    ACMChimera& Chimera,
    const FCMPartSlotAddress& PartSlotAddress,
    ACMPlayerState* ContributingPlayerState,
    const FVector& PlanarImpulse,
    float DirectionSign
)
{
    if (!Chimera.HasAuthority()
        || !IsValid(ContributingPlayerState)
        || !CMControl::IsValidPartSlot(
            PartSlotAddress,
            Chimera.ActiveSegmentCount)
        || PlanarImpulse.IsNearlyZero())
    {
        return;
    }

    const bool bIsLeft = CMControl::IsLeftPartSlot(PartSlotAddress);
    const bool bIsRight = CMControl::IsRightPartSlot(PartSlotAddress);
    if (!bIsLeft && !bIsRight)
    {
        return;
    }

    const double CurrentTime = Chimera.GetWorld()->GetTimeSeconds();
    PurgeExpiredCooperativeInputs(CurrentTime);

    const int32 FlatSlotIndex =
        CMControl::ToFlatPartSlotIndex(PartSlotAddress);
    const auto IsSamePendingSlot = [FlatSlotIndex](
        const FPendingCooperativeImpulse& PendingInput)
    {
        return PendingInput.FlatSlotIndex == FlatSlotIndex;
    };
    if (PendingLeftInputs.ContainsByPredicate(IsSamePendingSlot)
        || PendingRightInputs.ContainsByPredicate(IsSamePendingSlot))
    {
        return;
    }

    FPendingCooperativeImpulse PendingInput;
    PendingInput.FlatSlotIndex = FlatSlotIndex;
    PendingInput.RemainingImpulse = PlanarImpulse.Size2D();
    PendingInput.DirectionSign = DirectionSign < 0.0f ? -1.0f : 1.0f;
    PendingInput.ExpireTime = CurrentTime
        + FMath::Max(Chimera.CooperationInputWindow, 0.01f);

    (bIsLeft ? PendingLeftInputs : PendingRightInputs).Add(PendingInput);
    MatchCooperativeInputs(Chimera);
    ScheduleNextCooperativeExpiry(Chimera);
}

void UCMLineBodyMovementCoordinator::MatchCooperativeInputs(
    ACMChimera& Chimera
)
{
    while (!PendingLeftInputs.IsEmpty() && !PendingRightInputs.IsEmpty())
    {
        int32 LeftIndex = INDEX_NONE;
        int32 RightIndex = INDEX_NONE;
        for (int32 CandidateLeft = 0;
            CandidateLeft < PendingLeftInputs.Num();
            ++CandidateLeft)
        {
            const float LeftDirection =
                PendingLeftInputs[CandidateLeft].DirectionSign;
            RightIndex = PendingRightInputs.IndexOfByPredicate(
                [LeftDirection](
                    const FPendingCooperativeImpulse& RightInput)
                {
                    return FMath::IsNearlyEqual(
                        RightInput.DirectionSign,
                        LeftDirection
                    );
                }
            );
            if (RightIndex != INDEX_NONE)
            {
                LeftIndex = CandidateLeft;
                break;
            }
        }

        // Forward and reverse inputs intentionally remain separate. They may
        // still create their individual turning forces, but opposite movement
        // directions never produce a cooperative translation bonus together.
        if (LeftIndex == INDEX_NONE || RightIndex == INDEX_NONE)
        {
            break;
        }

        FPendingCooperativeImpulse& LeftInput =
            PendingLeftInputs[LeftIndex];
        FPendingCooperativeImpulse& RightInput =
            PendingRightInputs[RightIndex];
        const float MatchedImpulse = FMath::Min(
            LeftInput.RemainingImpulse,
            RightInput.RemainingImpulse
        );
        if (MatchedImpulse <= UE_SMALL_NUMBER)
        {
            break;
        }

        const float SignedForwardImpulse =
            MatchedImpulse * 1.0f * LeftInput.DirectionSign;
        ApplyCooperativeForwardImpulse(
            Chimera,
            SignedForwardImpulse
        );
        UE_LOG(LogChimeraMovement, Log,
            TEXT("[Cooperative Rolling Match] Direction=%s LeftSlot=%d RightSlot=%d Matched=%.1f Translation=%.1f LeftRemaining=%.1f RightRemaining=%.1f"),
            LeftInput.DirectionSign < 0.0f
                ? TEXT("Reverse")
                : TEXT("Forward"),
            LeftInput.FlatSlotIndex + 1,
            RightInput.FlatSlotIndex + 1,
            MatchedImpulse,
            SignedForwardImpulse,
            LeftInput.RemainingImpulse - MatchedImpulse,
            RightInput.RemainingImpulse - MatchedImpulse);

        LeftInput.RemainingImpulse -= MatchedImpulse;
        RightInput.RemainingImpulse -= MatchedImpulse;
        if (LeftInput.RemainingImpulse <= UE_SMALL_NUMBER)
        {
            PendingLeftInputs.RemoveAt(
                LeftIndex,
                EAllowShrinking::No
            );
        }
        if (RightInput.RemainingImpulse <= UE_SMALL_NUMBER)
        {
            PendingRightInputs.RemoveAt(
                RightIndex,
                EAllowShrinking::No
            );
        }
    }
}

bool UCMLineBodyMovementCoordinator::TryBeginArmAnchor(
    ACMChimera& Chimera,
    ACMArmPart& ArmPart
)
{
    UCMPartSlotComponent* PartSlot = ArmPart.GetAttachedPartSlot();
    const FCMPartSlotAddress PartSlotAddress =
        ArmPart.GetAttachedSlotAddress();
    const int32 SegmentIndex = PartSlotAddress.SegmentIndex;
    if (!Chimera.HasAuthority()
        || !ArmPart.IsOperational()
        || ArmPart.IsSwinging()
        || !PartSlot
        || PartSlot->GetOwner() != &Chimera
        || SegmentIndex < 0
        || SegmentIndex >= Chimera.ActiveSegmentCount
        || !Chimera.BodySegments.IsValidIndex(SegmentIndex))
    {
        return false;
    }

    const bool bAlreadyAnchored = ActiveArmAnchors.ContainsByPredicate(
        [&PartSlotAddress](const FActiveArmAnchor& Anchor)
        {
            return Anchor.PartSlotAddress == PartSlotAddress;
        }
    );
    UBoxComponent* SegmentBody = Chimera.BodySegments[SegmentIndex];
    if (bAlreadyAnchored || !SegmentBody
        || !SegmentBody->IsSimulatingPhysics())
    {
        return bAlreadyAnchored;
    }

    // 지면 짚기는 키메라 몸을 월드에 고정하는 것이 목적이므로
    // 상호작 물체와 달리 기존 하드 Constraint를 유지한다.
    const auto CreateGroundConstraint = [&Chimera, SegmentBody](
        const FVector& HoldLocation)
    {
        UPhysicsConstraintComponent* Constraint =
            NewObject<UPhysicsConstraintComponent>(&Chimera);
        if (!Constraint)
        {
            return static_cast<UPhysicsConstraintComponent*>(nullptr);
        }

        Chimera.AddInstanceComponent(Constraint);
        Constraint->RegisterComponent();
        Constraint->SetWorldLocation(HoldLocation);
        Constraint->SetDisableCollision(true);
        Constraint->SetLinearXLimit(
            ELinearConstraintMotion::LCM_Locked,
            0.0f);
        Constraint->SetLinearYLimit(
            ELinearConstraintMotion::LCM_Locked,
            0.0f);
        Constraint->SetLinearZLimit(
            ELinearConstraintMotion::LCM_Locked,
            0.0f);
        Constraint->SetAngularSwing1Limit(
            EAngularConstraintMotion::ACM_Free,
            0.0f);
        Constraint->SetAngularSwing2Limit(
            EAngularConstraintMotion::ACM_Free,
            0.0f);
        Constraint->SetAngularTwistLimit(
            EAngularConstraintMotion::ACM_Free,
            0.0f);
        Constraint->SetConstrainedComponents(
            SegmentBody,
            NAME_None,
            nullptr,
            NAME_None);
        return Constraint;
    };

    struct FArmHoldCandidate
    {
        TObjectPtr<AActor> TargetActor;
        FCMArmHoldSpec Spec;
        float DistanceSquared = 0.0f;
    };

    TArray<FArmHoldCandidate> Candidates;
    if (UWorld* World = Chimera.GetWorld())
    {
        const FVector Start = ArmPart.GetPartMesh()
            ? ArmPart.GetPartMesh()->GetComponentLocation()
            : PartSlot->GetComponentLocation();
        FVector HoldDirection = ArmPart.GetPartMesh()
            ? ArmPart.GetPartMesh()->GetForwardVector()
            : PartSlot->GetForwardVector();
        // 파트 메시의 전방보다 몸통 중심에서 슬롯 밖으로 향하는
        // 방향이 실제 왼손/오른손 탐색 방향에 더 안정적이다.
        if (const USceneComponent* SegmentComponent =
                PartSlot->GetAttachParent())
        {
            const FVector OutwardDirection = FVector::VectorPlaneProject(
                PartSlot->GetComponentLocation()
                    - SegmentComponent->GetComponentLocation(),
                SegmentComponent->GetUpVector()).GetSafeNormal();
            if (!OutwardDirection.IsNearlyZero())
            {
                HoldDirection = OutwardDirection;
            }
        }
        HoldDirection = HoldDirection.GetSafeNormal();

        TArray<FHitResult> InteractionHits;
        FCollisionQueryParams QueryParams(
            SCENE_QUERY_STAT(CMArmHoldTargets),
            false,
            &Chimera);
        QueryParams.AddIgnoredActor(&ArmPart);
        World->SweepMultiByObjectType(
            InteractionHits,
            Start,
            Start + HoldDirection * ArmPart.GetHoldRange(),
            FQuat::Identity,
            FCollisionObjectQueryParams(
                FCollisionObjectQueryParams::AllObjects),
            FCollisionShape::MakeSphere(ArmPart.GetHoldRadius()),
            QueryParams);

        TSet<AActor*> QueriedActors;
        for (const FHitResult& Hit : InteractionHits)
        {
            AActor* TargetActor = Hit.GetActor();
            if (!TargetActor || QueriedActors.Contains(TargetActor)
                || !TargetActor->Implements<UCMArmHoldTarget>())
            {
                continue;
            }
            QueriedActors.Add(TargetActor);

            FCMArmHoldSpec Spec;
            if (!ICMArmHoldTarget::Execute_QueryArmHold(
                    TargetActor,
                    &ArmPart,
                    Spec)
                || (Spec.bUsePhysicsHandle && !Spec.TargetComponent))
            {
                continue;
            }

            FArmHoldCandidate& Candidate = Candidates.AddDefaulted_GetRef();
            Candidate.TargetActor = TargetActor;
            Candidate.Spec = Spec;
            Candidate.DistanceSquared = FVector::DistSquared(
                Start,
                Spec.HoldLocation);
        }
    }

    // 대상이 제공한 우선순위를 먼저 적용하고,
    // 같은 우선순위라면 팔에 더 가까운 대상을 선택한다.
    Candidates.Sort([](
        const FArmHoldCandidate& Left,
        const FArmHoldCandidate& Right)
    {
        return Left.Spec.Priority != Right.Spec.Priority
            ? Left.Spec.Priority > Right.Spec.Priority
            : Left.DistanceSquared < Right.DistanceSquared;
    });

    for (const FArmHoldCandidate& Candidate : Candidates)
    {
        AActor* TargetActor = Candidate.TargetActor.Get();
        if (!IsValid(TargetActor)
            || !ICMArmHoldTarget::Execute_BeginArmHold(
                TargetActor,
                &ArmPart))
        {
            continue;
        }

        // 물체 이동은 몸통과 대상을 Constraint로 묶지 않는다.
        // 키메라는 목표점만 제공하고 Physics Handle이 물체만 따라오게 한다.
        UPhysicsHandleComponent* PhysicsHandle = nullptr;
        if (Candidate.Spec.bUsePhysicsHandle)
        {
            PhysicsHandle = NewObject<UPhysicsHandleComponent>(&Chimera);
            if (PhysicsHandle)
            {
                Chimera.AddInstanceComponent(PhysicsHandle);
                PhysicsHandle->RegisterComponent();
                PhysicsHandle->GrabComponentAtLocation(
                    Candidate.Spec.TargetComponent,
                    NAME_None,
                    Candidate.Spec.HoldLocation);
            }
            if (!PhysicsHandle
                || PhysicsHandle->GetGrabbedComponent()
                    != Candidate.Spec.TargetComponent)
            {
                if (PhysicsHandle)
                {
                    PhysicsHandle->DestroyComponent();
                }
                ICMArmHoldTarget::Execute_EndArmHold(
                    TargetActor,
                    &ArmPart);
                continue;
            }
        }

        FActiveArmAnchor& Anchor = ActiveArmAnchors.AddDefaulted_GetRef();
        Anchor.ArmPart = &ArmPart;
        Anchor.InteractionTarget = TargetActor;
        Anchor.TargetComponent = Candidate.Spec.TargetComponent;
        Anchor.PhysicsHandle = PhysicsHandle;
        Anchor.PartSlotAddress = PartSlotAddress;
        // 잡은 순간의 몸통 대비 손 위치를 저장해 마디가
        // 이동·회전해도 물체가 같은 상대 위치를 따라오게 한다.
        Anchor.PhysicsHandleTargetInSegmentSpace =
            SegmentBody->GetComponentTransform().InverseTransformPosition(
                Candidate.Spec.HoldLocation);
        Anchor.SegmentIndex = SegmentIndex;
        Anchor.bInteractable = true;
        Anchor.bHasTargetComponent = Candidate.Spec.TargetComponent != nullptr;
        Anchor.bRequiresPhysicsHandle = Candidate.Spec.bUsePhysicsHandle;
        ArmPart.BeginInteractableHold(
            Candidate.Spec.TargetComponent,
            Candidate.Spec.HoldLocation,
            Candidate.Spec.HoldNormal);
        if (ArmPart.GetAnchorStaminaCostPerSecond() > UE_SMALL_NUMBER)
        {
            Chimera.PauseStaminaRegeneration();
        }

        UE_LOG(LogChimeraMovement, Log,
            TEXT("[Arm Interaction Hold Started] Part=%s Target=%s Slot=(%d,%d) Priority=%d PhysicsHandle=%s"),
            *GetNameSafe(&ArmPart),
            *GetNameSafe(TargetActor),
            PartSlotAddress.SegmentIndex,
            PartSlotAddress.PartSlotIndex,
            Candidate.Spec.Priority,
            PhysicsHandle ? TEXT("true") : TEXT("false"));
        return true;
    }

    // 승인된 상호작 대상이 없을 때만 지면 짚기를 시도한다. 슬롯
    // 중심이 아니라 현재 손 본의 XY에서 내리쏴야 홀드 입력 순간 손이
    // 옆으로 순간 이동하지 않는다. 전용 서버처럼 포즈가 없는 경우에는
    // 기존 슬롯 위치가 안전한 폴백이다.
    FVector GroundTracePoint = PartSlot->GetComponentLocation();
    if (const USkeletalMeshComponent* ArmMesh = ArmPart.GetPartMesh())
    {
        static const FName HandBone(TEXT("hand_l"));
        if (ArmMesh->GetBoneIndex(HandBone) != INDEX_NONE)
        {
            GroundTracePoint = ArmMesh->GetBoneLocation(
                HandBone,
                EBoneSpaces::WorldSpace);
        }
    }
    FHitResult GroundHit;
    if (!TraceGroundAtPoint(
        Chimera,
        GroundTracePoint,
        &ArmPart,
        GroundHit))
    {
        UE_LOG(LogChimeraMovement, Warning,
            TEXT("[Arm Anchor Rejected] Part=%s Slot=(%d,%d) NoGround=true"),
            *GetNameSafe(&ArmPart),
            PartSlotAddress.SegmentIndex,
            PartSlotAddress.PartSlotIndex);
        return false;
    }

    UPhysicsConstraintComponent* Constraint = CreateGroundConstraint(
        GroundHit.ImpactPoint);
    if (!Constraint)
    {
        return false;
    }

    FActiveArmAnchor& Anchor = ActiveArmAnchors.AddDefaulted_GetRef();
    Anchor.ArmPart = &ArmPart;
    Anchor.Constraint = Constraint;
    Anchor.PartSlotAddress = PartSlotAddress;
    Anchor.SegmentIndex = SegmentIndex;
    ArmPart.BeginGroundAnchor(
        GroundHit.ImpactPoint,
        GroundHit.ImpactNormal);
    if (ArmPart.GetAnchorStaminaCostPerSecond() > UE_SMALL_NUMBER)
    {
        Chimera.PauseStaminaRegeneration();
    }

    UE_LOG(LogChimeraMovement, Log,
        TEXT("[Arm Anchor Started] Part=%s Slot=(%d,%d) Segment=%d Ground=%s DrainPerSecond=%.1f"),
        *GetNameSafe(&ArmPart),
        PartSlotAddress.SegmentIndex,
        PartSlotAddress.PartSlotIndex,
        SegmentIndex,
        *GroundHit.ImpactPoint.ToCompactString(),
        ArmPart.GetAnchorStaminaCostPerSecond());
    return true;
}

bool UCMLineBodyMovementCoordinator::IsArmHoldingInteractable(
    const FCMPartSlotAddress& PartSlotAddress
) const
{
    return ActiveArmAnchors.ContainsByPredicate(
        [&PartSlotAddress](const FActiveArmAnchor& Anchor)
        {
            return Anchor.PartSlotAddress == PartSlotAddress
                && Anchor.bInteractable;
        });
}

void UCMLineBodyMovementCoordinator::EndArmAnchor(
    const FCMPartSlotAddress& PartSlotAddress
)
{
    for (int32 AnchorIndex = ActiveArmAnchors.Num() - 1;
        AnchorIndex >= 0;
        --AnchorIndex)
    {
        if (ActiveArmAnchors[AnchorIndex].PartSlotAddress
            == PartSlotAddress)
        {
            DestroyArmAnchor(AnchorIndex);
        }
    }
}

void UCMLineBodyMovementCoordinator::ApplyCooperativeForwardImpulse(
    ACMChimera& Chimera,
    float SignedForwardImpulse
) const
{
    if (!Chimera.HasAuthority()
        || !Chimera.BodyMesh
        || FMath::IsNearlyZero(SignedForwardImpulse))
    {
        return;
    }

    const float DirectionSign = FMath::Sign(SignedForwardImpulse);
    float ForwardImpulseMagnitude = FMath::Abs(SignedForwardImpulse);

    float TotalMass = 0.0f;
    TArray<UBoxComponent*> SimulatedSegments;
    for (int32 Index = 0; Index < Chimera.ActiveSegmentCount; ++Index)
    {
        UBoxComponent* BodySegment =
            Chimera.BodySegments.IsValidIndex(Index)
            ? Chimera.BodySegments[Index]
            : nullptr;
        if (BodySegment && BodySegment->IsSimulatingPhysics())
        {
            TotalMass += FMath::Max(BodySegment->GetMass(), 0.01f);
            SimulatedSegments.Add(BodySegment);
        }
    }
    if (TotalMass <= UE_SMALL_NUMBER)
    {
        return;
    }

    // MaximumCooperativePlanarImpulse는 마디 하나가 받을 수 있는 협동 전진
    // 임펄스의 기준 상한이다. 몸통이 길어져 총질량이 늘어날 때 가속력이
    // 지나치게 약해지지 않도록 실제 전체 상한은 활성 물리 마디 수에 비례한다.
    const float TotalImpulseLimit =
        FMath::Max(Chimera.MaximumCooperativePlanarImpulse, 0.0f)
        * SimulatedSegments.Num();
    ForwardImpulseMagnitude = FMath::Min(
        ForwardImpulseMagnitude,
        TotalImpulseLimit
    );
    if (ForwardImpulseMagnitude <= UE_SMALL_NUMBER)
    {
        return;
    }

    for (UBoxComponent* BodySegment : SimulatedSegments)
    {
        const float MassFraction =
            FMath::Max(BodySegment->GetMass(), 0.01f) / TotalMass;

        // 각 마디가 자신의 접선 방향으로 밀어 지네처럼 움직이게 한다.
        // 몸이 굽어 있으면 각 전방 벡터의 합력이 전체 이동 방향을 만들고,
        // 서로 다른 힘의 방향은 Constraint를 통해 몸통 형태도 변화시킨다.
        FVector SegmentForwardDirection = BodySegment->GetForwardVector();
        SegmentForwardDirection.Z = 0.0f;
        if (!SegmentForwardDirection.Normalize())
        {
            continue;
        }

        BodySegment->AddImpulse(
            SegmentForwardDirection
            * ForwardImpulseMagnitude
            * DirectionSign
            * MassFraction
        );
    }
}

void UCMLineBodyMovementCoordinator::ApplyWholeBodyYawAssist(
    ACMChimera& Chimera,
    const FCMPartSlotAddress& PartSlotAddress,
    float MovementImpulseMultiplier
) const
{
    if (!Chimera.HasAuthority()
        || MovementImpulseMultiplier <= 0.0f
        || !CMControl::IsValidPartSlot(
            PartSlotAddress,
            Chimera.ActiveSegmentCount))
    {
        return;
    }

    const bool bIsLeft = CMControl::IsLeftPartSlot(PartSlotAddress);
    const bool bIsRight = CMControl::IsRightPartSlot(PartSlotAddress);
    if (!bIsLeft && !bIsRight)
    {
        return;
    }

    const float SideSign = bIsLeft ? 1.0f : -1.0f;
    const float PowerScale = FMath::Sqrt(MovementImpulseMultiplier);
    const float YawDeltaRadians = FMath::DegreesToRadians(
        Chimera.YawAssistDegreesPerInput * PowerScale
    ) * SideSign;
    const float MaximumYawSpeedRadians = FMath::DegreesToRadians(
        FMath::Max(Chimera.MaximumYawAngularSpeedDegrees, 0.0f)
    );
    if (FMath::IsNearlyZero(YawDeltaRadians)
        || MaximumYawSpeedRadians <= 0.0f)
    {
        return;
    }

    int32 AssistedSegmentCount = 0;
    for (int32 SegmentIndex = 0;
        SegmentIndex < Chimera.ActiveSegmentCount;
        ++SegmentIndex)
    {
        UBoxComponent* BodySegment =
            Chimera.BodySegments.IsValidIndex(SegmentIndex)
                ? Chimera.BodySegments[SegmentIndex]
                : nullptr;
        if (!BodySegment || !BodySegment->IsSimulatingPhysics())
        {
            continue;
        }

        FVector AngularVelocity =
            BodySegment->GetPhysicsAngularVelocityInRadians();
        AngularVelocity.Z = FMath::Clamp(
            AngularVelocity.Z + YawDeltaRadians,
            -MaximumYawSpeedRadians,
            MaximumYawSpeedRadians
        );
        BodySegment->SetPhysicsAngularVelocityInRadians(
            AngularVelocity,
            false
        );
        ++AssistedSegmentCount;
    }

    UE_LOG(LogChimeraMovement, Log,
        TEXT("[Whole Body Yaw Assist] Side=%s Slot=(%d,%d) Scale=%.2f DeltaDegrees=%.2f MaxDegreesPerSecond=%.1f Segments=%d"),
        bIsLeft ? TEXT("Left") : TEXT("Right"),
        PartSlotAddress.SegmentIndex,
        PartSlotAddress.PartSlotIndex,
        MovementImpulseMultiplier,
        FMath::RadiansToDegrees(YawDeltaRadians),
        Chimera.MaximumYawAngularSpeedDegrees,
        AssistedSegmentCount);
}

void UCMLineBodyMovementCoordinator::PurgeExpiredCooperativeInputs(
    double CurrentTime
)
{
    const auto IsExpired = [CurrentTime](
        const FPendingCooperativeImpulse& PendingInput)
    {
        return PendingInput.ExpireTime <= CurrentTime;
    };
    PendingLeftInputs.RemoveAll(IsExpired);
    PendingRightInputs.RemoveAll(IsExpired);
}

void UCMLineBodyMovementCoordinator::ScheduleNextCooperativeExpiry(
    ACMChimera& Chimera
)
{
    Chimera.GetWorldTimerManager().ClearTimer(CooperationExpiryTimerHandle);
    if (PendingLeftInputs.IsEmpty() && PendingRightInputs.IsEmpty())
    {
        return;
    }

    double NextExpireTime = TNumericLimits<double>::Max();
    for (const FPendingCooperativeImpulse& Input : PendingLeftInputs)
    {
        NextExpireTime = FMath::Min(NextExpireTime, Input.ExpireTime);
    }
    for (const FPendingCooperativeImpulse& Input : PendingRightInputs)
    {
        NextExpireTime = FMath::Min(NextExpireTime, Input.ExpireTime);
    }

    const double CurrentTime = Chimera.GetWorld()->GetTimeSeconds();
    Chimera.GetWorldTimerManager().SetTimer(
        CooperationExpiryTimerHandle,
        this,
        &UCMLineBodyMovementCoordinator::HandleCooperativeInputExpiry,
        static_cast<float>(FMath::Max(
            NextExpireTime - CurrentTime,
            0.001
        )),
        false
    );
}

void UCMLineBodyMovementCoordinator::HandleCooperativeInputExpiry()
{
    ACMChimera* Chimera = Cast<ACMChimera>(GetOwner());
    if (!Chimera || !Chimera->HasAuthority() || !Chimera->GetWorld())
    {
        PendingLeftInputs.Reset();
        PendingRightInputs.Reset();
        return;
    }

    PurgeExpiredCooperativeInputs(Chimera->GetWorld()->GetTimeSeconds());
    ScheduleNextCooperativeExpiry(*Chimera);
}

void UCMLineBodyMovementCoordinator::UpdateServerMovement(
    ACMChimera& Chimera
)
{
    if (!Chimera.HasAuthority() || !Chimera.BodyMesh)
    {
        return;
    }

    RemoveInvalidArmAnchors(Chimera);
    ApplyArmAnchorStaminaDrain(Chimera);
    UpdatePhysicsHandles(Chimera);

    UpdateLegPlantStates(Chimera);

    // A Step is a sustained ground reaction, so its Force is supplied every
    // server physics frame before the existing whole-body speed cap runs.
    ApplyActiveLegSteps(Chimera);

    TArray<UBoxComponent*> SimulatedSegments;
    float TotalMass = 0.0f;
    FVector MassWeightedHorizontalVelocity = FVector::ZeroVector;
    for (int32 SegmentIndex = 0;
        SegmentIndex < Chimera.ActiveSegmentCount;
        ++SegmentIndex)
    {
        UBoxComponent* BodySegment =
            Chimera.BodySegments.IsValidIndex(SegmentIndex)
                ? Chimera.BodySegments[SegmentIndex]
                : nullptr;
        if (!BodySegment || !BodySegment->IsSimulatingPhysics())
        {
            continue;
        }

        const float SegmentMass = FMath::Max(BodySegment->GetMass(), 0.01f);
        const FVector SegmentVelocity = BodySegment->GetPhysicsLinearVelocity();
        MassWeightedHorizontalVelocity += FVector(
            SegmentVelocity.X,
            SegmentVelocity.Y,
            0.0f
        ) * SegmentMass;
        TotalMass += SegmentMass;
        SimulatedSegments.Add(BodySegment);
    }

    if (TotalMass <= UE_SMALL_NUMBER || SimulatedSegments.IsEmpty())
    {
        return;
    }

    const FVector CenterOfMassHorizontalVelocity =
        MassWeightedHorizontalVelocity / TotalMass;
    const float EffectiveMaxSpeed =
        Chimera.IsSpringArmPulling()
            ? Chimera.SpringArmMaxSpeed
            : Chimera.MaxSpeed;

    if (EffectiveMaxSpeed <= 0.0f
        || CenterOfMassHorizontalVelocity.Size() <= EffectiveMaxSpeed)
    {
        return;
    }

    // 질량중심의 초과 속도만 모든 마디에서 동일하게 제거한다. 각 마디를
    // 개별 Clamp하지 않으므로 굽힘과 흔들림에 필요한 상대 속도는 유지된다.
    const FVector LimitedCenterOfMassVelocity =
        CenterOfMassHorizontalVelocity.GetSafeNormal() * EffectiveMaxSpeed;
    const FVector HorizontalVelocityCorrection =
        LimitedCenterOfMassVelocity - CenterOfMassHorizontalVelocity;

    for (UBoxComponent* BodySegment : SimulatedSegments)
    {
        FVector SegmentVelocity = BodySegment->GetPhysicsLinearVelocity();
        SegmentVelocity.X += HorizontalVelocityCorrection.X;
        SegmentVelocity.Y += HorizontalVelocityCorrection.Y;
        BodySegment->SetPhysicsLinearVelocity(SegmentVelocity);
    }
}

void UCMLineBodyMovementCoordinator::UpdateLegPlantStates(
    ACMChimera& Chimera
)
{
    const UWorld* World = Chimera.GetWorld();
    if (!World)
    {
        return;
    }

    const double CurrentTime = World->GetTimeSeconds();
    const int32 ActiveSlotCount = Chimera.ActiveSegmentCount
        * CMControl::PartSlotsPerSegment;
    for (int32 FlatSlotIndex = 0;
        FlatSlotIndex < ActiveSlotCount;
        ++FlatSlotIndex)
    {
        if (!Chimera.PartSlotPoints.IsValidIndex(FlatSlotIndex))
        {
            continue;
        }
        UCMPartSlotComponent* PartSlot =
            Chimera.PartSlotPoints[FlatSlotIndex];
        ACMLegPart* LegPart = PartSlot
            ? Cast<ACMLegPart>(PartSlot->GetAttachedPart())
            : nullptr;
        if (LegPart)
        {
            LegPart->AdvancePlantState();
        }
    }

    if (CurrentTime < NextReachRecoveryCheckTime)
    {
        return;
    }
    NextReachRecoveryCheckTime = CurrentTime
        + FMath::Max(static_cast<double>(Chimera.LegReplantCheckInterval),
            0.01);

    // Acquire each leg's first contact from the same production sweep used by
    // gameplay steps. This runs at the low-frequency contact cadence, never in
    // AnimGraph/Control Rig evaluation, and does not create a force window.
    for (int32 FlatSlotIndex = 0;
        FlatSlotIndex < ActiveSlotCount;
        ++FlatSlotIndex)
    {
        if (!Chimera.PartSlotPoints.IsValidIndex(FlatSlotIndex))
        {
            continue;
        }

        UCMPartSlotComponent* PartSlot =
            Chimera.PartSlotPoints[FlatSlotIndex];
        ACMLegPart* LegPart = PartSlot
            ? Cast<ACMLegPart>(PartSlot->GetAttachedPart())
            : nullptr;
        if (!PartSlot || !LegPart
            || !LegPart->IsOperational()
            || LegPart->GetPlantState() != ECMLegPlantState::Free
            || ActiveLegSteps.ContainsByPredicate(
                [LegPart](const FActiveLegStep& Step)
                {
                    return Step.LegPart.Get() == LegPart;
                }))
        {
            continue;
        }

        FVector ForwardDirection = PartSlot->GetForwardVector();
        ForwardDirection.Z = 0.0f;
        if (!ForwardDirection.Normalize())
        {
            continue;
        }

        const FVector DesiredFootPoint = PartSlot->GetComponentLocation()
            + ForwardDirection * Chimera.LegStepLength;
        FHitResult GroundHit;
        if (TraceGroundAtPoint(
                Chimera,
                DesiredFootPoint,
                LegPart,
                GroundHit))
        {
            LegPart->InitializePlantedContact(
                GroundHit.ImpactPoint,
                GroundHit.ImpactNormal);
        }
    }
    UpdateReachRecovery(Chimera, CurrentTime);
}

void UCMLineBodyMovementCoordinator::UpdateReachRecovery(
    ACMChimera& Chimera,
    const double CurrentTime
)
{
    const int32 ActiveSlotCount = Chimera.ActiveSegmentCount
        * CMControl::PartSlotsPerSegment;
    const float ReleaseDistance = FMath::Max(
        Chimera.LegReplantReleaseDistance,
        Chimera.LegStepLength);
    const float SettleDistance = FMath::Clamp(
        Chimera.LegReplantSettleDistance,
        0.0f,
        ReleaseDistance * 0.9f);
    for (int32 FlatSlotIndex = 0;
        FlatSlotIndex < ActiveSlotCount;
        ++FlatSlotIndex)
    {
        if (!Chimera.PartSlotPoints.IsValidIndex(FlatSlotIndex))
        {
            continue;
        }

        UCMPartSlotComponent* PartSlot =
            Chimera.PartSlotPoints[FlatSlotIndex];
        ACMLegPart* LegPart = PartSlot
            ? Cast<ACMLegPart>(PartSlot->GetAttachedPart())
            : nullptr;
        if (!PartSlot || !LegPart
            || LegPart->GetPlantState() != ECMLegPlantState::Planted
            || !LegPart->IsOperational()
            || ActiveLegSteps.ContainsByPredicate(
                [LegPart](const FActiveLegStep& Step)
                {
                    return Step.LegPart.Get() == LegPart
                        && Step.bOwnsVisualStep;
                }))
        {
            continue;
        }

        FVector ForwardDirection = PartSlot->GetForwardVector();
        ForwardDirection.Z = 0.0f;
        if (!ForwardDirection.Normalize())
        {
            continue;
        }
        const FVector DesiredFootPoint = PartSlot->GetComponentLocation()
            + ForwardDirection * Chimera.LegStepLength;
        const FVector GroundPoint = LegPart->GetStepGroundLocation();
        const FVector GroundNormal = LegPart->GetStepGroundNormal()
            .GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
        const FVector PlanarError = FVector::VectorPlaneProject(
            DesiredFootPoint - GroundPoint,
            GroundNormal);
        // The smaller settle threshold is kept explicit so tuning cannot
        // accidentally remove the release/settle hysteresis contract.
        if (PlanarError.SizeSquared() <= FMath::Square(SettleDistance))
        {
            continue;
        }
        if (PlanarError.SizeSquared() < FMath::Square(ReleaseDistance))
        {
            continue;
        }

        FHitResult GroundHit;
        if (!TraceGroundAtPoint(
                Chimera,
                DesiredFootPoint,
                LegPart,
                GroundHit))
        {
            UE_LOG(LogChimeraMovement, Verbose,
                TEXT("[Leg Reach Recovery Deferred] Part=%s Slot=(%d,%d) Error=%.1f Reason=NoGround"),
                *GetNameSafe(LegPart),
                PartSlot->GetSlotAddress().SegmentIndex,
                PartSlot->GetSlotAddress().PartSlotIndex,
                PlanarError.Size());
            continue;
        }

        // This transition changes only the replicated visual target. It is
        // deliberately not inserted into ActiveLegSteps and contributes no
        // force, impulse, or cooperative movement input.
        LegPart->BeginVisualReplant(
            GroundHit.ImpactPoint,
            GroundHit.ImpactNormal,
            Chimera.LegReplantDuration);
        UE_LOG(LogChimeraMovement, Log,
            TEXT("[Leg Reach Recovery Started] Part=%s Slot=(%d,%d) Error=%.1f Ground=%s Time=%.2f"),
            *GetNameSafe(LegPart),
            PartSlot->GetSlotAddress().SegmentIndex,
            PartSlot->GetSlotAddress().PartSlotIndex,
            PlanarError.Size(),
            *GroundHit.ImpactPoint.ToCompactString(),
            CurrentTime);
    }
}

void UCMLineBodyMovementCoordinator::UpdatePhysicsHandles(
    ACMChimera& Chimera)
{
    for (FActiveArmAnchor& Anchor : ActiveArmAnchors)
    {
        UPhysicsHandleComponent* PhysicsHandle = Anchor.PhysicsHandle.Get();
        UBoxComponent* SegmentBody =
            Chimera.BodySegments.IsValidIndex(Anchor.SegmentIndex)
                ? Chimera.BodySegments[Anchor.SegmentIndex]
                : nullptr;
        if (!PhysicsHandle || !SegmentBody)
        {
            continue;
        }

        // Handle 목표만 옮기므로 물체의 반력이 키메라 몸통에
        // Constraint 힘으로 역전달되지 않는다.
        PhysicsHandle->SetTargetLocation(
            SegmentBody->GetComponentTransform().TransformPosition(
                Anchor.PhysicsHandleTargetInSegmentSpace));
    }
}

void UCMLineBodyMovementCoordinator::ApplyArmAnchorStaminaDrain(
    ACMChimera& Chimera
)
{
    float StaminaPerSecond = 0.0f;
    for (const FActiveArmAnchor& Anchor : ActiveArmAnchors)
    {
        const ACMArmPart* ArmPart = Anchor.ArmPart.Get();
        if (IsValid(ArmPart) && ArmPart->IsOperational())
        {
            StaminaPerSecond += FMath::Max(
                ArmPart->GetAnchorStaminaCostPerSecond(),
                0.0f
            );
        }
    }

    const float DeltaSeconds = Chimera.GetWorld()
        ? Chimera.GetWorld()->GetDeltaSeconds()
        : 0.0f;
    if (StaminaPerSecond <= UE_SMALL_NUMBER
        || DeltaSeconds <= 0.0f
        || !Chimera.AbilitySystemComponent
        || !Chimera.AttributeSet)
    {
        return;
    }

    Chimera.AbilitySystemComponent->ApplyModToAttribute(
        UCMChimeraAttributeSet::GetStaminaAttribute(),
        EGameplayModOp::Additive,
        -StaminaPerSecond * DeltaSeconds
    );
    if (Chimera.AttributeSet->GetStamina() > UE_SMALL_NUMBER)
    {
        return;
    }

    UE_LOG(LogChimeraMovement, Log,
        TEXT("[Arm Anchor Stamina Depleted] Rate=%.1f Anchors=%d"),
        StaminaPerSecond,
        ActiveArmAnchors.Num());
    for (int32 AnchorIndex = ActiveArmAnchors.Num() - 1;
        AnchorIndex >= 0;
        --AnchorIndex)
    {
        DestroyArmAnchor(AnchorIndex);
    }
}

void UCMLineBodyMovementCoordinator::RemoveInvalidArmAnchors(
    ACMChimera& Chimera
)
{
    for (int32 AnchorIndex = ActiveArmAnchors.Num() - 1;
        AnchorIndex >= 0;
        --AnchorIndex)
    {
        const FActiveArmAnchor& Anchor = ActiveArmAnchors[AnchorIndex];
        const ACMArmPart* ArmPart = Anchor.ArmPart.Get();
        if (!IsValid(ArmPart)
            || !ArmPart->IsOperational()
            || !ArmPart->IsHolding()
            || (!Anchor.bInteractable && !Anchor.Constraint.IsValid())
            || (Anchor.bRequiresPhysicsHandle
                && (!Anchor.PhysicsHandle.IsValid()
                    || Anchor.PhysicsHandle->GetGrabbedComponent()
                        != Anchor.TargetComponent.Get()))
            || (Anchor.bInteractable
                && !Anchor.InteractionTarget.IsValid())
            || (Anchor.bHasTargetComponent
                && !Anchor.TargetComponent.IsValid())
            || !Chimera.IsSegmentAlive(Anchor.SegmentIndex))
        {
            DestroyArmAnchor(AnchorIndex);
        }
    }
}

void UCMLineBodyMovementCoordinator::DestroyArmAnchor(int32 AnchorIndex)
{
    if (!ActiveArmAnchors.IsValidIndex(AnchorIndex))
    {
        return;
    }

    // 대상 콜백, 팔 상태, 물리 컴포넌트를 한 경로에서 끝내
    // 입력 해제·파트 파괴·스태미나 고갈이 모두 같은 정리를 거치게 한다.
    const FActiveArmAnchor Anchor = ActiveArmAnchors[AnchorIndex];
    if (Anchor.bInteractable)
    {
        if (AActor* InteractionTarget = Anchor.InteractionTarget.Get())
        {
            ICMArmHoldTarget::Execute_EndArmHold(
                InteractionTarget,
                Anchor.ArmPart.Get());
        }
    }
    if (ACMArmPart* ArmPart = Anchor.ArmPart.Get())
    {
        ArmPart->EndGroundAnchor();
    }
    if (UPhysicsConstraintComponent* Constraint = Anchor.Constraint.Get())
    {
        Constraint->BreakConstraint();
        Constraint->DestroyComponent();
    }
    if (UPhysicsHandleComponent* PhysicsHandle = Anchor.PhysicsHandle.Get())
    {
        PhysicsHandle->ReleaseComponent();
        PhysicsHandle->DestroyComponent();
    }
    UE_LOG(LogChimeraMovement, Log,
        TEXT("[Arm Anchor Ended] Part=%s Slot=(%d,%d)"),
        *GetNameSafe(Anchor.ArmPart.Get()),
        Anchor.PartSlotAddress.SegmentIndex,
        Anchor.PartSlotAddress.PartSlotIndex);
    ActiveArmAnchors.RemoveAtSwap(
        AnchorIndex,
        EAllowShrinking::No
    );

    const bool bStillDraining = ActiveArmAnchors.ContainsByPredicate(
        [](const FActiveArmAnchor& ActiveAnchor)
        {
            const ACMArmPart* ArmPart = ActiveAnchor.ArmPart.Get();
            return IsValid(ArmPart)
                && ArmPart->GetAnchorStaminaCostPerSecond()
                    > UE_SMALL_NUMBER;
        }
    );
    if (!bStillDraining)
    {
        if (ACMChimera* Chimera = Cast<ACMChimera>(GetOwner()))
        {
            Chimera->StartStaminaRegeneration();
        }
    }
}

float UCMLineBodyMovementCoordinator::GetPlayerCountSpeedMultiplier(
    const ACMChimera& Chimera
) const
{
    const int32 PlayerCount = FMath::Clamp(
        Chimera.ActiveSegmentCount / CMControl::SegmentsPerPlayer,
        1,
        CMControl::MaxPlayers
    );

    float TargetSeconds = Chimera.OnePlayerTurnTargetSeconds;
    if (PlayerCount == 2)
    {
        TargetSeconds = Chimera.TwoPlayerTurnTargetSeconds;
    }
    else if (PlayerCount == 3)
    {
        TargetSeconds = Chimera.ThreePlayerTurnTargetSeconds;
    }
    else if (PlayerCount >= 4)
    {
        TargetSeconds = Chimera.FourPlayerTurnTargetSeconds;
    }

    return FMath::Max(Chimera.OnePlayerTurnTargetSeconds, 0.1f)
        / FMath::Max(TargetSeconds, 0.1f);
}

float UCMLineBodyMovementCoordinator::GetPerControlImpulseMultiplier(
    const ACMChimera& Chimera
) const
{
    constexpr float SinglePlayerControlCount = 4.0f;
    const int32 TotalControlCount = Chimera.ActiveSegmentCount
        * CMControl::PartSlotsPerSegment;
    return GetPlayerCountSpeedMultiplier(Chimera)
        * SinglePlayerControlCount
        / FMath::Max(static_cast<float>(TotalControlCount), 1.0f);
}

void UCMLineBodyMovementCoordinator::ApplyActiveLegSteps(
    ACMChimera& Chimera
)
{
    const double CurrentTime = Chimera.GetWorld()->GetTimeSeconds();
    for (int32 StepIndex = ActiveLegSteps.Num() - 1;
        StepIndex >= 0;
        --StepIndex)
    {
        FActiveLegStep& Step = ActiveLegSteps[StepIndex];
        ACMLegPart* LegPart = Step.LegPart.Get();
        UBoxComponent* SegmentBody = Step.SegmentBody.Get();
        const bool bCanContinue = CurrentTime < Step.EndTime
            && IsValid(LegPart)
            && LegPart->IsOperational()
            && IsValid(SegmentBody)
            && SegmentBody->IsSimulatingPhysics();
        if (!bCanContinue)
        {
            UE_LOG(LogChimeraMovement, Verbose,
                TEXT("[Leg Step Stopped] Part=%s Segment=%d Operational=%s"),
                *GetNameSafe(LegPart),
                Step.SegmentIndex,
                LegPart && LegPart->IsOperational()
                    ? TEXT("true")
                    : TEXT("false"));
            if (LegPart)
            {
                const bool bCanFinalizeContact = Step.bOwnsVisualStep
                    &&
                    LegPart->IsOperational()
                    && IsValid(SegmentBody)
                    && SegmentBody->IsSimulatingPhysics()
                    && (LegPart->GetPlantState() == ECMLegPlantState::Swing
                        || LegPart->GetPlantState()
                            == ECMLegPlantState::Landing);
                if (bCanFinalizeContact)
                {
                    FHitResult FinalGroundHit;
                    const bool bFinalContactValid = TraceGroundAtPoint(
                        Chimera,
                        Step.VirtualFootPoint,
                        LegPart,
                        FinalGroundHit);
                    if (bFinalContactValid)
                    {
                        LegPart->EndProceduralStep();
                    }
                    else
                    {
                        // A single missed sweep at the exact landing frame
                        // must not release a valid planted foot. Keep the
                        // state in Landing for the short configured grace
                        // window, without applying force after EndTime.
                        const double LandingGrace = FMath::Max(
                            static_cast<double>(Chimera.LegLandingContactGrace),
                            0.0);
                        if (CurrentTime < Step.EndTime + LandingGrace)
                        {
                            continue;
                        }
                        // Do not promote a stale target to Planted after a
                        // failed final contact check. Recover keeps the last
                        // rendered pose briefly before releasing it.
                        LegPart->CancelProceduralStep();
                    }
                }
                else if (Step.bOwnsVisualStep
                    && LegPart->IsOperational()
                    && IsValid(SegmentBody)
                    && !SegmentBody->IsSimulatingPhysics())
                {
                    LegPart->CancelProceduralStep();
                }
                else if (Step.bOwnsVisualStep)
                {
                    LegPart->CancelProceduralStep();
                }
            }
            ActiveLegSteps.RemoveAtSwap(
                StepIndex,
                EAllowShrinking::No
            );
            continue;
        }

        // 현재 슬롯의 XY 오프셋으로 좌우 다리의 자연스러운 Yaw를 유지한다.
        // 시작 시점의 월드 접지점을 계속 사용하면 몸통 이동에 따라 레버 암이
        // 무한히 커지므로, 적용점은 슬롯을 따라가고 높이만 COM에 맞춘다.
        const UCMPartSlotComponent* PartSlot = LegPart->GetAttachedPartSlot();
        FVector ForceApplicationPoint = PartSlot
            ? PartSlot->GetComponentLocation()
            : Step.GroundPoint;
        ForceApplicationPoint.Z = SegmentBody->GetCenterOfMass().Z;
        SegmentBody->AddForceAtLocation(
            Step.PushForce,
            ForceApplicationPoint
        );
    }
}

bool UCMLineBodyMovementCoordinator::TraceGroundAtPoint(
    const ACMChimera& Chimera,
    const FVector& DesiredFootPoint,
    const AActor* IgnoredPart,
    FHitResult& OutHit
) const
{
    if (!Chimera.GetWorld())
    {
        return false;
    }

    const FVector Start = DesiredFootPoint
        + FVector::UpVector * Chimera.LegStepTraceHeight;
    const FVector End = DesiredFootPoint
        - FVector::UpVector * Chimera.LegStepTraceDepth;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(&Chimera);
    if (IgnoredPart)
    {
        QueryParams.AddIgnoredActor(IgnoredPart);
    }

    // 몸이 U자나 원형으로 말리면 다른 장착 파츠가 가상 발의 수직
    // Sweep을 먼저 막을 수 있다. Actor Attachment 목록 대신 키메라가
    // 실제 권위 상태로 관리하는 PartSlot들을 순회해 확실히 제외한다.
    const int32 ActivePartSlotCount = Chimera.ActiveSegmentCount
        * CMControl::PartSlotsPerSegment;
    for (int32 FlatSlotIndex = 0;
        FlatSlotIndex < ActivePartSlotCount;
        ++FlatSlotIndex)
    {
        if (!Chimera.PartSlotPoints.IsValidIndex(FlatSlotIndex))
        {
            continue;
        }

        const UCMPartSlotComponent* AttachedPartSlot =
            Chimera.PartSlotPoints[FlatSlotIndex];
        if (AttachedPartSlot && AttachedPartSlot->GetAttachedPart())
        {
            QueryParams.AddIgnoredActor(
                AttachedPartSlot->GetAttachedPart()
            );
        }
    }

    TArray<FHitResult> Hits;
    Chimera.GetWorld()->SweepMultiByChannel(
        Hits,
        Start,
        End,
        FQuat::Identity,
        Chimera.GroundTraceChannel,
        FCollisionShape::MakeSphere(Chimera.GroundCheckRadius),
        QueryParams
    );
    const TArray<FHitResult> ChannelHits = Hits;

    const auto SelectWalkableGround = [&Chimera, &OutHit](
        const TArray<FHitResult>& CandidateHits)
    {
        bool bFound = false;
        float BestMetric = MAX_FLT;
        for (const FHitResult& Hit : CandidateHits)
        {
            const AActor* HitActor = Hit.GetActor();
            if (HitActor
                && HitActor->GetAttachParentActor() == &Chimera)
            {
                continue;
            }
            if (Hit.bStartPenetrating
                || Hit.ImpactPoint.ContainsNaN()
                || Hit.ImpactNormal.ContainsNaN()
                || Hit.ImpactNormal.SizeSquared() <= SMALL_NUMBER)
            {
                continue;
            }

            const FVector SafeNormal = Hit.ImpactNormal.GetSafeNormal();
            if (SafeNormal.IsNearlyZero()
                || SafeNormal.Z < Chimera.MinimumGroundNormalZ)
            {
                continue;
            }

            // SweepMulti does not promise array order. Time is the distance
            // along this sweep; Distance is a safe fallback for providers
            // that leave Time unset.
            float Metric = Hit.Time;
            if (!FMath::IsFinite(Metric) || Metric < 0.0f)
            {
                Metric = Hit.Distance;
            }
            if (!FMath::IsFinite(Metric))
            {
                Metric = 0.0f;
            }
            if (!bFound || Metric < BestMetric)
            {
                FHitResult ValidHit = Hit;
                ValidHit.ImpactNormal = SafeNormal;
                OutHit = ValidHit;
                BestMetric = Metric;
                bFound = true;
            }
        }
        return bFound;
    };

    bool bHasGroundContact = SelectWalkableGround(Hits);
    const TCHAR* SelectedTraceSource = TEXT("Channel");
    TArray<FHitResult> WorldStaticHits;

    // 레벨 바닥이 Visibility를 막지 않거나, 부착 파츠가 채널 Sweep의
    // 첫 Blocking Hit가 되면 실제 바닥까지 결과가 이어지지 않을 수 있다.
    // 설정 채널에서 유효한 지면을 못 찾은 경우 물리 지면인 WorldStatic만
    // 다시 조회하여 파츠의 Collision 설정에 접지 판정이 좌우되지 않게 한다.
    if (!bHasGroundContact)
    {
        FCollisionObjectQueryParams GroundObjectQuery;
        GroundObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
        Chimera.GetWorld()->SweepMultiByObjectType(
            WorldStaticHits,
            Start,
            End,
            FQuat::Identity,
            GroundObjectQuery,
            FCollisionShape::MakeSphere(Chimera.GroundCheckRadius),
            QueryParams
        );
        bHasGroundContact = SelectWalkableGround(WorldStaticHits);
        SelectedTraceSource = TEXT("WorldStatic");
    }

    if (bHasGroundContact)
    {
        const UPrimitiveComponent* HitComponent = OutHit.GetComponent();
        UE_LOG(LogChimeraMovement, Log,
            TEXT("[Ground Trace Selected] Source=%s Actor=%s Component=%s ObjectType=%d Blocking=%s StartPenetrating=%s Point=%s Normal=%s"),
            SelectedTraceSource,
            *GetNameSafe(OutHit.GetActor()),
            *GetNameSafe(HitComponent),
            HitComponent
                ? static_cast<int32>(HitComponent->GetCollisionObjectType())
                : INDEX_NONE,
            OutHit.bBlockingHit ? TEXT("true") : TEXT("false"),
            OutHit.bStartPenetrating ? TEXT("true") : TEXT("false"),
            *OutHit.ImpactPoint.ToCompactString(),
            *OutHit.ImpactNormal.ToCompactString());
    }
    else
    {
        const auto LogRejectedCandidates = [](const TCHAR* Source,
            const TArray<FHitResult>& CandidateHits)
        {
            UE_LOG(LogChimeraMovement, Warning,
                TEXT("[Ground Trace Candidates] Source=%s Count=%d"),
                Source,
                CandidateHits.Num());
            for (int32 HitIndex = 0;
                HitIndex < CandidateHits.Num();
                ++HitIndex)
            {
                const FHitResult& Hit = CandidateHits[HitIndex];
                const UPrimitiveComponent* HitComponent = Hit.GetComponent();
                UE_LOG(LogChimeraMovement, Warning,
                    TEXT("[Ground Trace Hit] Source=%s Index=%d Actor=%s Component=%s ObjectType=%d Blocking=%s StartPenetrating=%s Distance=%.2f Point=%s Normal=%s AttachParent=%s"),
                    Source,
                    HitIndex,
                    *GetNameSafe(Hit.GetActor()),
                    *GetNameSafe(HitComponent),
                    HitComponent
                        ? static_cast<int32>(HitComponent->GetCollisionObjectType())
                        : INDEX_NONE,
                    Hit.bBlockingHit ? TEXT("true") : TEXT("false"),
                    Hit.bStartPenetrating ? TEXT("true") : TEXT("false"),
                    Hit.Distance,
                    *Hit.ImpactPoint.ToCompactString(),
                    *Hit.ImpactNormal.ToCompactString(),
                    *GetNameSafe(Hit.GetActor()
                        ? Hit.GetActor()->GetAttachParentActor()
                        : nullptr));
            }
        };

        LogRejectedCandidates(TEXT("Channel"), ChannelHits);
        LogRejectedCandidates(TEXT("WorldStatic"), WorldStaticHits);
    }

    if (Chimera.bDrawGroundContactDebug)
    {
        DrawDebugLine(
            Chimera.GetWorld(),
            Start,
            End,
            bHasGroundContact ? FColor::Green : FColor::Red,
            false,
            0.5f,
            0,
            1.0f
        );
        DrawDebugSphere(
            Chimera.GetWorld(),
            bHasGroundContact ? OutHit.ImpactPoint : End,
            Chimera.GroundCheckRadius,
            12,
            bHasGroundContact ? FColor::Green : FColor::Red,
            false,
            0.5f,
            0,
            1.5f
        );
    }

    return bHasGroundContact;
}

void UCMLineBodyMovementCoordinator::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            CooperationExpiryTimerHandle
        );
    }
    PendingLeftInputs.Reset();
    PendingRightInputs.Reset();
    for (const FActiveLegStep& Step : ActiveLegSteps)
    {
        if (ACMLegPart* LegPart = Step.LegPart.Get())
        {
            LegPart->CancelProceduralStep();
        }
    }
    ActiveLegSteps.Reset();
    for (int32 AnchorIndex = ActiveArmAnchors.Num() - 1;
        AnchorIndex >= 0;
        --AnchorIndex)
    {
        DestroyArmAnchor(AnchorIndex);
    }

    Super::EndPlay(EndPlayReason);
}
