#include "Stage/Trigger/CMPowerCableActor.h"

#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Arm/CMArmPart.h"
#include "Stage/Trigger/Data/CMPowerCableDefinition.h"
#include "Stage/Trigger/Component/CMPowerSocketComponent.h"
#include "Stage/Trigger/Component/CMPowerSourceComponent.h"
#include "Stage/Trigger/Subsystem/CMPowerSubsystem.h"

ACMPowerCableActor::ACMPowerCableActor()
{
    PrimaryActorTick.bCanEverTick = true;
    CableSpline = CreateDefaultSubobject<USplineComponent>(TEXT("CableSpline"));
    SetRootComponent(CableSpline);
    CableSpline->SetMobility(EComponentMobility::Movable);
    CableSpline->SetClosedLoop(false);

    GrabVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("GrabVolume"));
    GrabVolume->SetupAttachment(CableSpline);
    GrabVolume->SetMobility(EComponentMobility::Movable);
    GrabVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GrabVolume->SetCollisionObjectType(ECC_WorldDynamic);
    GrabVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
    GrabVolume->SetBoxExtent(FVector(50.0f, 15.0f, 15.0f));
    bReplicates = true;
    SetReplicateMovement(true);
}

void ACMPowerCableActor::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CableStartLocation = GetActorLocation();
        bCableStartLocationInitialized = true;

        if (bAutoConnectOnSpawn)
        {
            TArray<UCMPowerSourceComponent*> SourcesA;
            TArray<UCMPowerSourceComponent*> SourcesB;
            TArray<UCMPowerSocketComponent*> SocketsA;
            TArray<UCMPowerSocketComponent*> SocketsB;
            if (InputActor)
            {
                InputActor->GetComponents(SourcesA);
                InputActor->GetComponents(SocketsA);
            }
            if (OutputActor)
            {
                OutputActor->GetComponents(SourcesB);
                OutputActor->GetComponents(SocketsB);
            }

            UCMPowerSourceComponent* Source = nullptr;
            UCMPowerSocketComponent* Socket = nullptr;
            UCMPowerSocketComponent* PoweredSocket = nullptr;
            if (!OutputActor && !SourcesA.IsEmpty())
            {
                Source = SourcesA[0];
            }
            else if (!OutputActor && !SocketsA.IsEmpty())
            {
                Socket = SocketsA[0];
            }
            else if (!SourcesA.IsEmpty() && !SocketsB.IsEmpty())
            {
                Source = SourcesA[0];
                Socket = SocketsB[0];
            }
            else if (!SourcesB.IsEmpty() && !SocketsA.IsEmpty())
            {
                Source = SourcesB[0];
                Socket = SocketsA[0];
            }
            else if (!SocketsA.IsEmpty() && !SocketsB.IsEmpty())
            {
                // This may be an unpowered prewired connection. The source
                // socket becomes powered later when another cable connects
                // a generator to it.
                PoweredSocket = SocketsA[0];
                Socket = SocketsB[0];
            }

            bool bConnected = false;
            if (!OutputActor)
            {
                // A single actor reference intentionally creates a one-sided
                // spawn connection. Prefer a source when the actor provides
                // both component types.
                if (Source)
                {
                    bConnected = TryConnectToSource(Source, true);
                }
                else if (!SocketsA.IsEmpty())
                {
                    bConnected = TryConnectToSocket(SocketsA[0], true);
                }
            }
            else if (Source && Socket)
            {
                bConnected = TryConnectToSource(Source, true)
                    && TryConnectToSocket(Socket, true);
            }
            else if (PoweredSocket && Socket)
            {
                bConnected = TryConnectToSocketSource(PoweredSocket, true)
                    && TryConnectToSocket(Socket, true);
            }

            if (!bConnected)
            {
                UE_LOG(LogChimeraStageLoad, Warning,
                    TEXT("Power cable spawn connection failed or is ambiguous. Cable=%s ActorA=%s ActorB=%s"),
                    *GetName(), *GetNameSafe(InputActor),
                    *GetNameSafe(OutputActor));
            }
        }
    }
    InitializeRope();

    if (UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UCMStageLoadCoordinatorSubsystem* Coordinator =
            GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>())
        {
            Coordinator->OnLoadGroupFinished.AddUniqueDynamic(
                this, &ThisClass::HandleLoadGroupFinished);
        }
    }

    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("Power cable visual init. Cable=%s Definition=%s LoadGroup=%s"),
        *GetName(), *CableDefinition.ToSoftObjectPath().ToString(),
        *LoadGroupId.ToString());
    RefreshCableVisualState();
}

void ACMPowerCableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UCMStageLoadCoordinatorSubsystem* Coordinator =
            GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>())
        {
            Coordinator->OnLoadGroupFinished.RemoveAll(this);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void ACMPowerCableActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bRopeSleeping && !IsGrabbed() && !HasAnyEndpointConnected())
    {
        SetActorTickEnabled(false);
        return;
    }

    if (bCableStartLocationInitialized && !bCableHasBeenMoved
        && FVector::DistSquared(GetActorLocation(), CableStartLocation)
            > FMath::Square(0.1f))
    {
        bCableHasBeenMoved = true;
        bHasCachedVisualEndpoint = false;
    }

    SimulateRope(DeltaSeconds);
    UpdateCableVisual();
    UpdateGrabVolume();
}

FVector ACMPowerCableActor::GetCableEndLocation() const
{
    if (bRopeInitialized && !RopePositions.IsEmpty())
    {
        return RopePositions.Last();
    }

    return (ConnectedSocket && !bSocketAtStart)
        ? ConnectedSocket->GetComponentLocation()
        : (ConnectedSource && !bSourceAtStart)
            ? ConnectedSource->GetComponentLocation()
        : Grabber
            ? Grabber->GetActorLocation()
        : bCableStartLocationInitialized && !bCableHasBeenMoved
            ? CableStartLocation + GetActorForwardVector()
                * GetInitialCableLength()
        : GetActorLocation();
}

FVector ACMPowerCableActor::GetCableStartLocation() const
{
    return GetRopeStartTarget();
}

FVector ACMPowerCableActor::GetClosestFreeEndpointLocation(
    const FVector& Location
) const
{
    const FVector Start = GetRopeStartTarget();
    const FVector End = bRopeInitialized && !RopePositions.IsEmpty()
        ? RopePositions.Last() : GetRopeEndTarget();
    const bool bStartOccupied = ConnectedSource || ConnectedSourceSocket
        || ConnectedSocket && bSocketAtStart;
    const bool bEndOccupied = ConnectedSource && !bSourceAtStart
        || ConnectedSourceSocket && !bSourceAtStart
        || ConnectedSocket && !bSocketAtStart;

    if (bStartOccupied && !bEndOccupied)
    {
        return End;
    }
    if (bEndOccupied && !bStartOccupied)
    {
        return Start;
    }
    return FVector::DistSquared(Location, Start)
        <= FVector::DistSquared(Location, End) ? Start : End;
}

FVector ACMPowerCableActor::GetRopeStartTarget() const
{
    if (Grabber && bGrabAtStart)
    {
        return Grabber->GetActorLocation();
    }
    if (bSourceAtStart)
    {
        if (ConnectedSource)
        {
            return ConnectedSource->GetComponentLocation();
        }
        if (ConnectedSourceSocket)
        {
            return ConnectedSourceSocket->GetComponentLocation();
        }
    }
    if (bSocketAtStart && ConnectedSocket)
    {
        return ConnectedSocket->GetComponentLocation();
    }
    return CableStartLocation;
}

FVector ACMPowerCableActor::GetRopeEndTarget() const
{
    if (Grabber && !bGrabAtStart)
    {
        return Grabber->GetActorLocation();
    }
    if (!bSourceAtStart)
    {
        if (ConnectedSource)
        {
            return ConnectedSource->GetComponentLocation();
        }
        if (ConnectedSourceSocket)
        {
            return ConnectedSourceSocket->GetComponentLocation();
        }
    }
    if (!bSocketAtStart && ConnectedSocket)
    {
        return ConnectedSocket->GetComponentLocation();
    }
    return Grabber ? Grabber->GetActorLocation() :
        CableStartLocation + GetActorForwardVector() * GetInitialCableLength();
}

bool ACMPowerCableActor::IsRopeEndFixed() const
{
    const bool bEndHasConnection =
        (ConnectedSocket && !bSocketAtStart)
        || (ConnectedSource && !bSourceAtStart)
        || (ConnectedSourceSocket && !bSourceAtStart);
    return (Grabber && !bGrabAtStart) || bEndHasConnection;
}

bool ACMPowerCableActor::IsTransmittingPower() const
{
    if (!ConnectedSocket)
    {
        return false;
    }

    if (ConnectedSource && ConnectedSource->IsProvidingPower())
    {
        return true;
    }

    return ConnectedSourceSocket && ConnectedSourceSocket->IsPowered();
}

bool ACMPowerCableActor::IsTransmittingPower(
    TSet<const UCMPowerSocketComponent*>& VisitedSockets
) const
{
    if (!ConnectedSocket)
    {
        return false;
    }

    if (ConnectedSource && ConnectedSource->IsProvidingPower())
    {
        return true;
    }

    return ConnectedSourceSocket
        && ConnectedSourceSocket->IsPowered(VisitedSockets);
}

bool ACMPowerCableActor::CanConnectEndpointWithinLength(
    const FVector& EndpointLocation) const
{
    if (SimulatedRopeLength <= UE_SMALL_NUMBER)
    {
        return true;
    }

    bool bHasExistingEndpoint = false;
    FVector ExistingEndpoint = FVector::ZeroVector;
    if (ConnectedSource || ConnectedSourceSocket)
    {
        ExistingEndpoint = bSourceAtStart
            ? GetRopeStartTarget() : GetRopeEndTarget();
        bHasExistingEndpoint = true;
    }
    else if (ConnectedSocket)
    {
        ExistingEndpoint = bSocketAtStart
            ? GetRopeStartTarget() : GetRopeEndTarget();
        bHasExistingEndpoint = true;
    }

    return !bHasExistingEndpoint
        || FVector::DistSquared(ExistingEndpoint, EndpointLocation)
            <= FMath::Square(SimulatedRopeLength + 1.0f);
}

int32 ACMPowerCableActor::GetVisualSegmentCount() const
{
    return bOverrideDefinitionSettings
        ? FMath::Max(OverrideVisualSegmentCount, 1)
        : CableDefinition.Get()
        ? FMath::Max(CableDefinition->VisualSegmentCount, 1)
        : 1;
}

float ACMPowerCableActor::GetCableSag() const
{
    return CableDefinition.Get() ? CableDefinition->CableSag : 0.0f;
}

float ACMPowerCableActor::GetCableThicknessScale() const
{
    return bOverrideDefinitionSettings
        ? FMath::Max(OverrideCableThicknessScale, 0.01f)
        : CableDefinition.Get()
        ? FMath::Max(CableDefinition->CableThicknessScale, 0.01f)
        : 1.0f;
}

float ACMPowerCableActor::GetInitialCableLength() const
{
    return bOverrideDefinitionSettings
        ? FMath::Max(OverrideInitialCableLength, 0.0f)
        : CableDefinition.Get()
        ? FMath::Max(CableDefinition->InitialCableLength, 0.0f)
        : 100.0f;
}

float ACMPowerCableActor::GetRopeNodeSpacing() const
{
    return bOverrideDefinitionSettings
        ? FMath::Max(OverrideRopeNodeSpacing, 5.0f)
        : CableDefinition.Get()
        ? FMath::Max(CableDefinition->RopeNodeSpacing, 5.0f)
        : 35.0f;
}

float ACMPowerCableActor::GetRopeGravityScale() const
{
    return bOverrideDefinitionSettings
        ? FMath::Max(OverrideRopeGravityScale, 0.0f)
        : CableDefinition.Get()
        ? FMath::Max(CableDefinition->RopeGravityScale, 0.0f)
        : 1.0f;
}

float ACMPowerCableActor::GetRopeDamping() const
{
    return bOverrideDefinitionSettings
        ? FMath::Clamp(OverrideRopeDamping, 0.0f, 1.0f)
        : CableDefinition.Get()
        ? FMath::Clamp(CableDefinition->RopeDamping, 0.0f, 1.0f)
        : 0.85f;
}

int32 ACMPowerCableActor::GetRopeConstraintIterations() const
{
    return bOverrideDefinitionSettings
        ? FMath::Max(OverrideRopeConstraintIterations, 1)
        : CableDefinition.Get()
        ? FMath::Max(CableDefinition->RopeConstraintIterations, 1)
        : 8;
}

float ACMPowerCableActor::GetRopeCollisionRadius() const
{
    return bOverrideDefinitionSettings
        ? FMath::Max(OverrideRopeCollisionRadius, 0.0f)
        : CableDefinition.Get()
        ? FMath::Max(CableDefinition->RopeCollisionRadius, 0.0f)
        : 4.0f;
}

float ACMPowerCableActor::GetRopeSleepMovementThreshold() const
{
    return CableDefinition.Get()
        ? FMath::Max(CableDefinition->RopeSleepMovementThreshold, 0.0f)
        : 0.5f;
}

int32 ACMPowerCableActor::GetRopeSleepFrameCount() const
{
    return CableDefinition.Get()
        ? FMath::Max(CableDefinition->RopeSleepFrameCount, 1)
        : 20;
}

float ACMPowerCableActor::GetInitialCoilRadius() const
{
    return bOverrideDefinitionSettings
        ? FMath::Max(OverrideInitialCoilRadius, 0.0f)
        : CableDefinition.Get()
        ? FMath::Max(CableDefinition->InitialCoilRadius, 0.0f)
        : 25.0f;
}

bool ACMPowerCableActor::ShouldStartCoiled() const
{
    return bOverrideDefinitionSettings
        ? bOverrideStartCoiled
        : CableDefinition.Get()
        ? CableDefinition->bStartCoiled
        : true;
}

void ACMPowerCableActor::InitializeRope()
{
    if (bRopeInitialized || !bCableStartLocationInitialized)
    {
        return;
    }

    const FVector StartLocation = CableStartLocation;
    const float CableLength = FMath::Max(GetInitialCableLength(), 1.0f);
    const FVector Forward = GetActorForwardVector().GetSafeNormal();
    const FVector Right = FVector::CrossProduct(
        FVector::UpVector, Forward).GetSafeNormal();
    const float CoilRadius = GetInitialCoilRadius();
    SimulatedRopeLength = FMath::Max(
        CableLength,
        GetRopeNodeSpacing());
    const int32 MinimumNodeCount = ShouldStartCoiled() ? 8 : 2;
    const int32 NodeCount = FMath::Max(
        MinimumNodeCount,
        FMath::CeilToInt(SimulatedRopeLength / GetRopeNodeSpacing()) + 1);
    RopePositions.SetNum(NodeCount);
    RopePreviousPositions.SetNum(NodeCount);
    for (int32 Index = 0; Index < NodeCount; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / (NodeCount - 1);
        if (ShouldStartCoiled() && CoilRadius > UE_SMALL_NUMBER)
        {
            const FVector CoilCenter = StartLocation + Right * CoilRadius;
            const float Angle = -HALF_PI + CableLength * Alpha
                / CoilRadius;
            RopePositions[Index] = CoilCenter
                + Forward * FMath::Cos(Angle) * CoilRadius
                + Right * FMath::Sin(Angle) * CoilRadius;
        }
        else
        {
            RopePositions[Index] = StartLocation
                + Forward * CableLength * Alpha;
        }
        RopePositions[Index].Z -= GetCableSag()
            * FMath::Sin(Alpha * PI);
        RopePreviousPositions[Index] = RopePositions[Index];
    }
    bRopeInitialized = true;
}

void ACMPowerCableActor::WakeRopeSimulation()
{
    bRopeSleeping = false;
    RopeStableFrameCount = 0;
    SetActorTickEnabled(true);
}

void ACMPowerCableActor::SimulateRope(float DeltaSeconds)
{
    InitializeRope();
    if (!bRopeInitialized || RopePositions.Num() < 2)
    {
        return;
    }

    if (IsGrabbed())
    {
        bRopeSleeping = false;
        RopeStableFrameCount = 0;
    }
    else if (bRopeSleeping && !HasAnyEndpointConnected())
    {
        return;
    }

    const bool bEndIsFixed = IsRopeEndFixed();
    const FVector RequestedEndTarget = GetRopeEndTarget();
    FVector EndTarget = RequestedEndTarget;
    if (IsGrabbed())
    {
        const FVector FromStart = EndTarget - GetRopeStartTarget();
        const float DistanceFromStart = FromStart.Size();
        if (DistanceFromStart > SimulatedRopeLength
            && DistanceFromStart > UE_SMALL_NUMBER)
        {
            EndTarget = GetRopeStartTarget()
                + FromStart / DistanceFromStart * SimulatedRopeLength;
        }
    }

    constexpr int32 SimulationSubsteps = 4;
    const float Step = FMath::Clamp(
        DeltaSeconds / SimulationSubsteps,
        0.001f,
        0.02f);
    RopeConstraintStartPositions.SetNum(RopePositions.Num());
    CollisionLockedNodes.SetNum(RopePositions.Num());
    for (int32 Substep = 0; Substep < SimulationSubsteps; ++Substep)
    {
        FMemory::Memzero(CollisionLockedNodes.GetData(),
            CollisionLockedNodes.Num() * sizeof(uint8));

        for (int32 Index = 1; Index < RopePositions.Num(); ++Index)
        {
            if (bEndIsFixed && Index == RopePositions.Num() - 1)
            {
                continue;
            }

            const FVector CurrentPosition = RopePositions[Index];
            const FVector Velocity = (CurrentPosition
                - RopePreviousPositions[Index]) * GetRopeDamping();
            RopePreviousPositions[Index] = CurrentPosition;
            RopePositions[Index] = CurrentPosition + Velocity
                + FVector(0.0f, 0.0f, GetWorld()->GetGravityZ()
                    * GetRopeGravityScale() * Step * Step);

            if (GetWorld())
            {
                FHitResult Hit;
                FCollisionQueryParams QueryParams(
                    SCENE_QUERY_STAT(CMPowerCableRope), false, this);
                FCollisionObjectQueryParams ObjectQueryParams;
                ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
                ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
                ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
                const bool bHit = GetWorld()->SweepSingleByObjectType(
                    Hit,
                    CurrentPosition,
                    RopePositions[Index],
                    FQuat::Identity,
                    ObjectQueryParams,
                    FCollisionShape::MakeSphere(GetRopeCollisionRadius()),
                    QueryParams);
                if (bHit)
                {
                    RopePositions[Index] = Hit.Location
                        + Hit.Normal * GetRopeCollisionRadius();
                    RopePreviousPositions[Index] = RopePositions[Index];
                    CollisionLockedNodes[Index] = 1;
                }
            }
        }

        const float RestDistance = SimulatedRopeLength
            / (RopePositions.Num() - 1);
        RopeConstraintStartPositions = RopePositions;
        for (int32 Iteration = 0;
            Iteration < GetRopeConstraintIterations();
            ++Iteration)
        {
            RopePositions[0] = GetCableStartLocation();
            if (bEndIsFixed)
            {
                RopePositions.Last() = EndTarget;
            }

            for (int32 Index = 0; Index < RopePositions.Num() - 1; ++Index)
            {
                FVector Delta = RopePositions[Index + 1]
                    - RopePositions[Index];
                const float Distance = Delta.Size();
                if (Distance <= UE_SMALL_NUMBER)
                {
                    continue;
                }

                const FVector Correction = Delta * ((Distance - RestDistance)
                    / Distance);
                const bool bStartPinned = Index == 0;
                const bool bNextPinned = bEndIsFixed
                    && Index + 1 == RopePositions.Num() - 1;
                if (!bStartPinned && !bNextPinned)
                {
                    RopePositions[Index] += Correction * 0.5f;
                    RopePositions[Index + 1] -= Correction * 0.5f;
                }
                else if (bStartPinned && !bNextPinned)
                {
                    RopePositions[Index + 1] -= Correction;
                }
                else if (!bStartPinned && bNextPinned)
                {
                    RopePositions[Index] += Correction;
                }
            }
        }

        // Length constraints can push a node back into the floor after the
        // first collision sweep. Resolve that penetration after constraints
        // as well, otherwise the next Verlet step turns it into a bounce.
        for (int32 Index = 1; Index < RopePositions.Num(); ++Index)
        {
            if (bEndIsFixed && Index == RopePositions.Num() - 1)
            {
                continue;
            }

            FHitResult Hit;
            FCollisionQueryParams QueryParams(
                SCENE_QUERY_STAT(CMPowerCableRopePostConstraint), false, this);
            FCollisionObjectQueryParams ObjectQueryParams;
            ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
            ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
            ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
            const bool bHit = GetWorld()->SweepSingleByObjectType(
                Hit,
                RopeConstraintStartPositions[Index],
                RopePositions[Index],
                FQuat::Identity,
                ObjectQueryParams,
                FCollisionShape::MakeSphere(GetRopeCollisionRadius()),
                QueryParams);
            if (bHit)
            {
                RopePositions[Index] = Hit.Location
                    + Hit.Normal * GetRopeCollisionRadius();
                RopePreviousPositions[Index] = RopePositions[Index];
            }
        }

        RopePositions[0] = GetCableStartLocation();
        if (bEndIsFixed)
        {
            RopePositions.Last() = EndTarget;
        }

        ResolveRopeGroundContact(bEndIsFixed);

        // Prevent constraint corrections from becoming artificial velocity.
        for (int32 Index = 1; Index < RopePreviousPositions.Num(); ++Index)
        {
            if (bEndIsFixed && Index == RopePreviousPositions.Num() - 1)
            {
                RopePreviousPositions[Index] = RopePositions[Index];
            }
            else
            {
                RopePreviousPositions[Index] = FMath::Lerp(
                    RopePreviousPositions[Index],
                    RopePositions[Index],
                    0.35f);
            }
        }

        // Constraint corrections can move a node after it was projected out
        // of a surface. Keep collided nodes at their resolved position so
        // that this correction cannot become artificial bounce velocity on
        // the next substep.
        for (int32 Index = 1; Index < RopePreviousPositions.Num(); ++Index)
        {
            if (CollisionLockedNodes[Index] != 0
                && !(bEndIsFixed
                    && Index == RopePreviousPositions.Num() - 1))
            {
                RopePreviousPositions[Index] = RopePositions[Index];
            }
        }
    }

    RopePositions[0] = GetCableStartLocation();
    if (bEndIsFixed)
    {
        RopePositions.Last() = EndTarget;
    }

    float MaxMovement = 0.0f;
    for (int32 Index = 1; Index < RopePositions.Num(); ++Index)
    {
        MaxMovement = FMath::Max(
            MaxMovement,
            FVector::Distance(
                RopePositions[Index],
                RopePreviousPositions[Index]));
    }

    if (!IsGrabbed() && !HasAnyEndpointConnected()
        && MaxMovement <= GetRopeSleepMovementThreshold())
    {
        ++RopeStableFrameCount;
        if (RopeStableFrameCount >= GetRopeSleepFrameCount())
        {
            bRopeSleeping = true;
            SetActorTickEnabled(false);
            RopeStableFrameCount = 0;
            for (int32 Index = 0; Index < RopePreviousPositions.Num(); ++Index)
            {
                RopePreviousPositions[Index] = RopePositions[Index];
            }
        }
    }
    else
    {
        RopeStableFrameCount = 0;
    }
}

void ACMPowerCableActor::ResolveRopeGroundContact(bool bEndIsFixed)
{
    UWorld* World = GetWorld();
    if (!World || RopePositions.Num() < 2)
    {
        return;
    }

    const float CollisionRadius = GetRopeCollisionRadius();
    if (CollisionRadius <= UE_SMALL_NUMBER)
    {
        return;
    }

    // Only resolve contacts below/near a surface. Starting the sweep above
    // the node prevents a node that is still in the air from being snapped
    // down to a distant floor.
    const float ProbeHeight = FMath::Max(50.0f, CollisionRadius * 4.0f);
    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
            ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    for (int32 Index = 1; Index < RopePositions.Num(); ++Index)
    {
        if (bEndIsFixed && Index == RopePositions.Num() - 1)
        {
            continue;
        }

        const FVector NodePosition = RopePositions[Index];
        FHitResult Hit;
        FCollisionQueryParams QueryParams(
            SCENE_QUERY_STAT(CMPowerCableRopeGroundContact), false, this);
        const bool bHit = World->SweepSingleByObjectType(
            Hit,
            NodePosition + FVector::UpVector * ProbeHeight,
            NodePosition - FVector::UpVector * CollisionRadius,
            FQuat::Identity,
            ObjectQueryParams,
            FCollisionShape::MakeSphere(CollisionRadius),
            QueryParams);

        // A downward-facing normal is a ceiling or underside, not a floor.
        if (bHit && Hit.Normal.Z > 0.2f)
        {
            RopePositions[Index] = Hit.Location
                + Hit.Normal * CollisionRadius;
            // Remove both downward and collision-generated rebound velocity.
            RopePreviousPositions[Index] = RopePositions[Index];
        }
    }
}

bool ACMPowerCableActor::QueryArmHold_Implementation(
    ACMArmPart* ArmPart,
    FCMArmHoldSpec& OutSpec
) const
{
    if (!HasAuthority() || !IsValid(ArmPart)
        || IsGrabbed() || !GrabVolume)
    {
        return false;
    }

    OutSpec.Priority = 0;
    OutSpec.HoldLocation = GetClosestFreeEndpointLocation(
        ArmPart->GetActorLocation());
    OutSpec.HoldNormal = (
        ArmPart->GetActorLocation() - OutSpec.HoldLocation
    ).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    OutSpec.TargetComponent = nullptr;
    OutSpec.bUsePhysicsHandle = false;
    return true;
}

bool ACMPowerCableActor::BeginArmHold_Implementation(ACMArmPart* ArmPart)
{
    return BeginGrab(ArmPart);
}

void ACMPowerCableActor::EndArmHold_Implementation(ACMArmPart* ArmPart)
{
    if (Grabber != ArmPart || !HasAuthority())
    {
        return;
    }

    // Try both endpoint types while Grabber is still set, so their distance
    // checks use the cable endpoint currently held by the arm.
    UCMPowerSocketComponent* ClosestSocket = nullptr;
    UCMPowerSourceComponent* ClosestSource = nullptr;
    float ClosestSocketDistanceSquared = TNumericLimits<float>::Max();
    float ClosestSourceDistanceSquared = TNumericLimits<float>::Max();

    UCMPowerSubsystem* PowerSubsystem = GetWorld()
        ? GetWorld()->GetSubsystem<UCMPowerSubsystem>() : nullptr;
    if (PowerSubsystem)
    {
        for (const TWeakObjectPtr<UCMPowerSocketComponent>& SocketPtr
            : PowerSubsystem->GetSockets())
        {
            UCMPowerSocketComponent* Socket = SocketPtr.Get();
            if (!IsValid(Socket) || Socket->IsPhysicallyConnected()
                || Socket->GetPowerChannel() != PowerChannel
                || PowerChannel.IsNone())
            {
                continue;
            }

            const FVector SocketEndpoint =
                GetClosestFreeEndpointLocation(Socket->GetComponentLocation());
            const float DistanceSquared = FVector::DistSquared(
                Socket->GetComponentLocation(), SocketEndpoint);
            if (DistanceSquared < ClosestSocketDistanceSquared)
            {
                ClosestSocketDistanceSquared = DistanceSquared;
                ClosestSocket = Socket;
            }
        }

        for (const TWeakObjectPtr<UCMPowerSourceComponent>& SourcePtr
            : PowerSubsystem->GetSources())
        {
            UCMPowerSourceComponent* Source = SourcePtr.Get();
            if (!IsValid(Source) || Source->IsPhysicallyConnected()
                || Source->GetPowerChannel() != PowerChannel
                || PowerChannel.IsNone())
            {
                continue;
            }

            const FVector SourceEndpoint =
                GetClosestFreeEndpointLocation(Source->GetComponentLocation());
            const float DistanceSquared = FVector::DistSquared(
                Source->GetComponentLocation(), SourceEndpoint);
            if (DistanceSquared < ClosestSourceDistanceSquared)
            {
                ClosestSourceDistanceSquared = DistanceSquared;
                ClosestSource = Source;
            }
        }
    }

    if (ClosestSource
        && ClosestSourceDistanceSquared <= ClosestSocketDistanceSquared
        && TryConnectToSource(ClosestSource))
    {
        UE_LOG(LogChimeraStageLoad, Display,
            TEXT("Power cable connected on arm release. Cable=%s Source=%s"),
            *GetName(), *GetNameSafe(ClosestSource));
        return;
    }

    if (ClosestSocket && TryConnectToSocket(ClosestSocket))
    {
        UE_LOG(LogChimeraStageLoad, Display,
            TEXT("Power cable connected on arm release. Cable=%s Socket=%s"),
            *GetName(), *GetNameSafe(ClosestSocket));
        return;
    }

    ReleaseGrab();
}

bool ACMPowerCableActor::BeginGrab(AActor* InGrabber)
{
    if (!HasAuthority() || !InGrabber || IsGrabbed())
    {
        return false;
    }

    const FVector GrabLocation = InGrabber->GetActorLocation();
    const FVector StartLocation = bRopeInitialized && !RopePositions.IsEmpty()
        ? RopePositions[0] : GetRopeStartTarget();
    const FVector EndLocation = bRopeInitialized && !RopePositions.IsEmpty()
        ? RopePositions.Last() : GetRopeEndTarget();
    const bool bStartOccupied = ConnectedSource || ConnectedSourceSocket
        || ConnectedSocket && bSocketAtStart;
    const bool bEndOccupied = ConnectedSource && !bSourceAtStart
        || ConnectedSourceSocket && !bSourceAtStart
        || ConnectedSocket && !bSocketAtStart;
    const bool bGrabStart = bStartOccupied && !bEndOccupied
        ? false : bEndOccupied && !bStartOccupied
            ? true : FVector::DistSquared(GrabLocation, StartLocation)
                <= FVector::DistSquared(GrabLocation, EndLocation);

    // Detach only the endpoint that the player is actually grabbing. This
    // prevents a non-detachable endpoint on the opposite side from being
    // pulled across the rope when the cable is picked up.
    const bool bSocketAtGrabbedEnd = ConnectedSocket
        && (bSocketAtStart == bGrabStart);
    const bool bSourceAtGrabbedEnd = (ConnectedSource || ConnectedSourceSocket)
        && (bSourceAtStart == bGrabStart);
    if (bSocketAtGrabbedEnd)
    {
        if (ConnectedSocket && !ConnectedSocket->CanDisconnectCable())
        {
            return false;
        }
        DisconnectFromSocket();
    }
    else if (bSourceAtGrabbedEnd)
    {
        if (ConnectedSource && !ConnectedSource->CanDisconnectCable())
        {
            return false;
        }
        if (ConnectedSourceSocket && !ConnectedSourceSocket->CanDisconnectCable())
        {
            return false;
        }
        DisconnectFromSource();
    }

    WakeRopeSimulation();
    bGrabAtStart = bGrabStart;
    Grabber = InGrabber;
    bRopeSleeping = false;
    RopeStableFrameCount = 0;
    ForceNetUpdate();
    return true;
}

void ACMPowerCableActor::ReleaseGrab()
{
    if (!HasAuthority())
    {
        return;
    }

    WakeRopeSimulation();
    Grabber = nullptr;
    bGrabAtStart = false;
    bRopeSleeping = false;
    RopeStableFrameCount = 0;
    ForceNetUpdate();
}

bool ACMPowerCableActor::TryConnectToSocket(
    UCMPowerSocketComponent* Socket,
    bool bIgnoreConnectionRadius
)
{
    return Socket && Socket->TryConnectCable(this, bIgnoreConnectionRadius);
}

bool ACMPowerCableActor::TryConnectToSource(
    UCMPowerSourceComponent* Source,
    bool bIgnoreConnectionRadius
)
{
    return Source && Source->TryConnectCable(this, bIgnoreConnectionRadius);
}

bool ACMPowerCableActor::TryConnectToPoweredSocket(
    UCMPowerSocketComponent* Socket,
    bool bIgnoreConnectionRadius
)
{
    if (!Socket || HasConnectedSource()
        || HasConnectedSourceSocket())
    {
        return false;
    }

    if (!bIgnoreConnectionRadius && FVector::DistSquared(
        Socket->GetComponentLocation(),
        GetClosestFreeEndpointLocation(Socket->GetComponentLocation()))
        > FMath::Square(Socket->GetConnectionRadius()))
    {
        return false;
    }

    SetConnectedSourceSocket(Socket);
    return true;
}

bool ACMPowerCableActor::TryConnectToSocketSource(
    UCMPowerSocketComponent* Socket,
    bool bIgnoreConnectionRadius
)
{
    if (!Socket || HasConnectedSource() || HasConnectedSourceSocket())
    {
        return false;
    }

    if (!bIgnoreConnectionRadius && FVector::DistSquared(
        Socket->GetComponentLocation(),
        GetClosestFreeEndpointLocation(Socket->GetComponentLocation()))
        > FMath::Square(Socket->GetConnectionRadius()))
    {
        return false;
    }

    SetConnectedSourceSocket(Socket);
    return true;
}

void ACMPowerCableActor::Disconnect()
{
    if (!HasAuthority())
    {
        return;
    }

    if (ConnectedSocket)
    {
        ConnectedSocket->DisconnectCable(this);
    }
    if (ConnectedSource)
    {
        ConnectedSource->DisconnectCable(this);
    }
    if (ConnectedSourceSocket
        && ConnectedSourceSocket->CanDisconnectCable())
    {
        SetConnectedSourceSocket(nullptr);
    }
}

void ACMPowerCableActor::SetConnectedSocket(
    UCMPowerSocketComponent* Socket
)
{
    if (!HasAuthority())
    {
        return;
    }

    WakeRopeSimulation();
    UCMPowerSocketComponent* PreviousSocket = ConnectedSocket;
    const FVector SocketLocation = Socket
        ? Socket->GetComponentLocation() : FVector::ZeroVector;
    const FVector ClosestEndpoint =
        GetClosestFreeEndpointLocation(SocketLocation);
    bSocketAtStart = bRopeInitialized && !RopePositions.IsEmpty()
        ? FVector::DistSquared(ClosestEndpoint, RopePositions[0])
            <= FVector::DistSquared(ClosestEndpoint, RopePositions.Last())
        : FVector::DistSquared(ClosestEndpoint, CableStartLocation)
            <= FVector::DistSquared(
                ClosestEndpoint, GetRopeEndTarget());
    ConnectedSocket = Socket;
    Grabber = nullptr;
    if (Socket)
    {
        // Keep the rope's current simulated shape. The socket connection is
        // logical; moving the actor here would straighten the cable. Keep
        // simulating with both endpoints pinned so later body contacts can
        // deform the cable naturally.
        bRopeSleeping = false;
        RopeStableFrameCount = 0;
    }
    else
    {
        bRopeSleeping = false;
        RopeStableFrameCount = 0;
    }
    if (PreviousSocket && PreviousSocket != ConnectedSocket)
    {
        PreviousSocket->NotifyPowerStateChanged();
    }
    if (ConnectedSocket)
    {
        ConnectedSocket->NotifyPowerStateChanged();
    }
    OnConnectionChanged.Broadcast(IsFullyConnected());
    ForceNetUpdate();
}

void ACMPowerCableActor::DisconnectFromSocket()
{
    if (!HasAuthority() || !ConnectedSocket
        || !ConnectedSocket->CanDisconnectCable())
    {
        return;
    }

    ConnectedSocket->DisconnectCable(this);
}

void ACMPowerCableActor::DisconnectFromSource()
{
    if (!HasAuthority())
    {
        return;
    }

    if (ConnectedSource && ConnectedSource->CanDisconnectCable())
    {
        ConnectedSource->DisconnectCable(this);
    }
    if (ConnectedSourceSocket
        && ConnectedSourceSocket->CanDisconnectCable())
    {
        SetConnectedSourceSocket(nullptr);
    }
}

void ACMPowerCableActor::SetConnectedSource(
    UCMPowerSourceComponent* Source
)
{
    if (!HasAuthority())
    {
        return;
    }

    WakeRopeSimulation();
    const FVector SourceLocation = Source
        ? Source->GetComponentLocation() : FVector::ZeroVector;
    const FVector ClosestEndpoint =
        GetClosestFreeEndpointLocation(SourceLocation);
    bSourceAtStart = bRopeInitialized && !RopePositions.IsEmpty()
        ? FVector::DistSquared(ClosestEndpoint, RopePositions[0])
            <= FVector::DistSquared(ClosestEndpoint, RopePositions.Last())
        : FVector::DistSquared(ClosestEndpoint, CableStartLocation)
            <= FVector::DistSquared(
                ClosestEndpoint, GetRopeEndTarget());
    ConnectedSource = Source;
    Grabber = nullptr;
    bRopeSleeping = false;
    RopeStableFrameCount = 0;
    if (ConnectedSocket)
    {
        ConnectedSocket->NotifyPowerStateChanged();
    }
    OnConnectionChanged.Broadcast(IsFullyConnected());
    ForceNetUpdate();
}

void ACMPowerCableActor::SetConnectedSourceSocket(
    UCMPowerSocketComponent* Socket
)
{
    if (!HasAuthority())
    {
        return;
    }

    WakeRopeSimulation();
    if (ConnectedSourceSocket && ConnectedSourceSocket != Socket)
    {
        ConnectedSourceSocket->RemovePowerOutputCable(this);
    }
    ConnectedSourceSocket = Socket;
    Grabber = nullptr;
    if (Socket)
    {
        const FVector ClosestEndpoint =
            GetClosestFreeEndpointLocation(Socket->GetComponentLocation());
        bSourceAtStart = bRopeInitialized && !RopePositions.IsEmpty()
            ? FVector::DistSquared(ClosestEndpoint, RopePositions[0])
                <= FVector::DistSquared(ClosestEndpoint, RopePositions.Last())
            : FVector::DistSquared(ClosestEndpoint, CableStartLocation)
                <= FVector::DistSquared(
                    ClosestEndpoint, GetRopeEndTarget());
    }
    if (ConnectedSourceSocket)
    {
        ConnectedSourceSocket->AddPowerOutputCable(this);
    }
    bRopeSleeping = false;
    RopeStableFrameCount = 0;
    if (ConnectedSocket)
    {
        ConnectedSocket->NotifyPowerStateChanged();
    }
    OnConnectionChanged.Broadcast(IsFullyConnected());
    ForceNetUpdate();
}

void ACMPowerCableActor::NotifyPowerStateChanged()
{
    TSet<const UCMPowerSocketComponent*> VisitedSockets;
    NotifyPowerStateChanged(VisitedSockets);
}

void ACMPowerCableActor::NotifyPowerStateChanged(
    TSet<const UCMPowerSocketComponent*>& VisitedSockets
)
{
    if (ConnectedSocket)
    {
        ConnectedSocket->NotifyPowerStateChanged(VisitedSockets);
    }
}

void ACMPowerCableActor::OnRep_ConnectedSocket()
{
    if (ConnectedSocket)
    {
        ConnectedSocket->NotifyPowerStateChanged();
    }
    OnConnectionChanged.Broadcast(IsFullyConnected());
}

void ACMPowerCableActor::OnRep_ConnectedSource()
{
    if (ConnectedSocket)
    {
        ConnectedSocket->NotifyPowerStateChanged();
    }
    OnConnectionChanged.Broadcast(IsFullyConnected());
}

void ACMPowerCableActor::OnRep_ConnectedSourceSocket()
{
    if (ConnectedSocket)
    {
        ConnectedSocket->NotifyPowerStateChanged();
    }
    OnConnectionChanged.Broadcast(IsFullyConnected());
}

void ACMPowerCableActor::OnRep_EndpointOrientation()
{
    WakeRopeSimulation();
    UpdateCableVisual();
}

void ACMPowerCableActor::OnRep_CableStartLocation()
{
    bCableStartLocationInitialized = true;
    bHasCachedVisualEndpoint = false;
    if (!bRopeInitialized)
    {
        InitializeRope();
    }
    UpdateCableVisual();
}

void ACMPowerCableActor::HandleLoadGroupFinished(
    FName FinishedLoadGroupId,
    EAsyncLoadResult Result,
    bool bReleasedImmediately
)
{
    if (FinishedLoadGroupId != LoadGroupId
        || bCableVisualReady || bCableVisualFailed)
    {
        return;
    }
    if (Result != EAsyncLoadResult::Succeeded || bReleasedImmediately)
    {
        bCableVisualFailed = true;
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Power cable visual load failed. Cable=%s Definition=%s LoadGroup=%s"),
            *GetName(), *CableDefinition.ToSoftObjectPath().ToString(),
            *LoadGroupId.ToString());
        return;
    }
    TryBuildCableVisual();
}

void ACMPowerCableActor::RefreshCableVisualState()
{
    if (CableDefinition.IsNull() || bCableVisualReady || bCableVisualFailed)
    {
        return;
    }

    if (LoadGroupId.IsNone())
    {
        bCableVisualFailed = true;
        return;
    }

    UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance() : nullptr;
    UCMStageLoadCoordinatorSubsystem* Coordinator = GameInstance
        ? GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>()
        : nullptr;
    if (!Coordinator)
    {
        bCableVisualFailed = true;
        return;
    }

    const ECMStageLoadGroupState State = Coordinator->GetLoadGroupState(
        LoadGroupId);
    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("Power cable load state. Cable=%s Group=%s State=%d Definition=%s"),
        *GetName(), *LoadGroupId.ToString(), static_cast<int32>(State),
        *CableDefinition.ToSoftObjectPath().ToString());
    if (State == ECMStageLoadGroupState::Ready)
    {
        TryBuildCableVisual();
    }
    else if (State == ECMStageLoadGroupState::Failed
        || State == ECMStageLoadGroupState::Released)
    {
        bCableVisualFailed = true;
    }
}

bool ACMPowerCableActor::TryBuildCableVisual()
{
    UCMPowerCableDefinition* LoadedDefinition = CableDefinition.Get();
    UStaticMesh* LoadedMesh = LoadedDefinition
        ? LoadedDefinition->CableMesh.Get() : nullptr;
    if (!LoadedMesh || !CableSpline)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Power cable visual could not resolve loaded Definition. Cable=%s Definition=%s LoadGroup=%s"),
            *GetName(), *CableDefinition.ToSoftObjectPath().ToString(),
            *LoadGroupId.ToString());
        return false;
    }

    bCableVisualReady = true;
    if (!IsGrabbed() && !HasAnyEndpointConnected())
    {
        bRopeInitialized = false;
        RopePositions.Reset();
        RopePreviousPositions.Reset();
        InitializeRope();
    }

    CableMeshes.Reserve(GetVisualSegmentCount());
    for (int32 Index = 0; Index < GetVisualSegmentCount(); ++Index)
    {
        USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(
            this);
        if (!SplineMesh)
        {
            continue;
        }
        SplineMesh->SetMobility(EComponentMobility::Movable);
        SplineMesh->SetStaticMesh(LoadedMesh);
        // SM_Wires01 is authored vertically, so its length axis is Z.
        // The spline mesh must use the same axis or each segment appears standing upright.
        SplineMesh->SetForwardAxis(ESplineMeshAxis::Z, false);
        SplineMesh->SetupAttachment(CableSpline);
        AddInstanceComponent(SplineMesh);
        SplineMesh->RegisterComponent();
        CableMeshes.Add(SplineMesh);
    }
    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("Power cable visual created. Cable=%s Mesh=%s Segments=%d"),
        *GetName(), *GetNameSafe(LoadedMesh), CableMeshes.Num());
    UpdateCableVisual();
    return !CableMeshes.IsEmpty();
}

void ACMPowerCableActor::UpdateCableVisual()
{
    if (!bCableVisualReady || !CableSpline || CableMeshes.IsEmpty())
    {
        return;
    }

    if (RopePositions.Num() < 2)
    {
        return;
    }

    CableSpline->ClearSplinePoints(false);
    for (int32 Index = 0; Index < RopePositions.Num(); ++Index)
    {
        CableSpline->AddSplinePoint(
            RopePositions[Index], ESplineCoordinateSpace::World, false);
        CableSpline->SetSplinePointType(
            Index, ESplinePointType::Curve, false);
    }
    CableSpline->UpdateSpline();

    const float TotalLength = CableSpline->GetSplineLength();
    UStaticMesh* CableMesh = CableMeshes[0]->GetStaticMesh();
    const float MeshLength = CableMesh
        ? CableMesh->GetBounds().BoxExtent.Z * 2.0f
        : 0.0f;
    const int32 DesiredMeshCount = MeshLength > UE_SMALL_NUMBER
        ? FMath::Max(1, FMath::CeilToInt(TotalLength / MeshLength))
        : GetVisualSegmentCount();
    EnsureCableMeshCount(DesiredMeshCount, CableMesh);

    for (int32 Index = 0; Index < CableMeshes.Num(); ++Index)
    {
        if (Index >= DesiredMeshCount)
        {
            CableMeshes[Index]->SetVisibility(false);
            continue;
        }

        CableMeshes[Index]->SetVisibility(true);
        const float StartDistance = MeshLength > UE_SMALL_NUMBER
            ? FMath::Min(Index * MeshLength, TotalLength)
            : TotalLength * Index / DesiredMeshCount;
        const float EndDistance = MeshLength > UE_SMALL_NUMBER
            ? FMath::Min((Index + 1) * MeshLength, TotalLength)
            : TotalLength * (Index + 1) / DesiredMeshCount;
        const FVector Start = CableSpline->GetLocationAtDistanceAlongSpline(
            StartDistance, ESplineCoordinateSpace::Local);
        const FVector End = CableSpline->GetLocationAtDistanceAlongSpline(
            EndDistance, ESplineCoordinateSpace::Local);
        const FVector StartTangent =
            CableSpline->GetTangentAtDistanceAlongSpline(
                StartDistance, ESplineCoordinateSpace::Local);
        const FVector EndTangent =
            CableSpline->GetTangentAtDistanceAlongSpline(
                EndDistance, ESplineCoordinateSpace::Local);
        CableMeshes[Index]->SetStartAndEnd(
            Start, StartTangent, End, EndTangent, true);
        const FVector2D ThicknessScale(
            GetCableThicknessScale(),
            GetCableThicknessScale());
        CableMeshes[Index]->SetStartScale(ThicknessScale, true);
        CableMeshes[Index]->SetEndScale(ThicknessScale, true);
    }
}

void ACMPowerCableActor::EnsureCableMeshCount(
    int32 DesiredCount,
    UStaticMesh* Mesh
)
{
    if (!Mesh || !CableSpline)
    {
        return;
    }

    while (CableMeshes.Num() < DesiredCount)
    {
        USplineMeshComponent* SplineMesh =
            NewObject<USplineMeshComponent>(this);
        if (!SplineMesh)
        {
            break;
        }

        SplineMesh->SetMobility(EComponentMobility::Movable);
        SplineMesh->SetStaticMesh(Mesh);
        SplineMesh->SetForwardAxis(ESplineMeshAxis::Z, false);
        SplineMesh->SetupAttachment(CableSpline);
        AddInstanceComponent(SplineMesh);
        SplineMesh->RegisterComponent();
        CableMeshes.Add(SplineMesh);
    }
}

void ACMPowerCableActor::UpdateGrabVolume()
{
    if (!GrabVolume)
    {
        return;
    }

    if (RopePositions.IsEmpty())
    {
        return;
    }

    FBox Bounds(ForceInit);
    for (const FVector& Position : RopePositions)
    {
        Bounds += Position;
    }
    Bounds = Bounds.ExpandBy(15.0f);
    GrabVolume->SetWorldLocation(Bounds.GetCenter());
    GrabVolume->SetWorldRotation(FRotator::ZeroRotator);
    GrabVolume->SetBoxExtent(Bounds.GetExtent());
}

void ACMPowerCableActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMPowerCableActor, Grabber);
    DOREPLIFETIME(ACMPowerCableActor, bGrabAtStart);
    DOREPLIFETIME(ACMPowerCableActor, ConnectedSocket);
    DOREPLIFETIME(ACMPowerCableActor, ConnectedSource);
    DOREPLIFETIME(ACMPowerCableActor, ConnectedSourceSocket);
    DOREPLIFETIME(ACMPowerCableActor, bSocketAtStart);
    DOREPLIFETIME(ACMPowerCableActor, bSourceAtStart);
    DOREPLIFETIME(ACMPowerCableActor, CableStartLocation);
}
