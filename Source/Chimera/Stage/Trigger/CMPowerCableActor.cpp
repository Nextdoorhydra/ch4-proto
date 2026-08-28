#include "Stage/Trigger/CMPowerCableActor.h"

#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Stage/Trigger/Data/CMPowerCableDefinition.h"
#include "Stage/Trigger/Component/CMPowerSocketComponent.h"

ACMPowerCableActor::ACMPowerCableActor()
{
    PrimaryActorTick.bCanEverTick = true;
    CableSpline = CreateDefaultSubobject<USplineComponent>(TEXT("CableSpline"));
    SetRootComponent(CableSpline);
    CableSpline->SetMobility(EComponentMobility::Movable);
    CableSpline->SetClosedLoop(false);
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
    }

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

    if (bCableStartLocationInitialized && !bCableHasBeenMoved
        && FVector::DistSquared(GetActorLocation(), CableStartLocation)
            > FMath::Square(0.1f))
    {
        bCableHasBeenMoved = true;
        bHasCachedVisualEndpoint = false;
    }

    UpdateCableVisual();
}

FVector ACMPowerCableActor::GetCableEndLocation() const
{
    return ConnectedSocket
        ? ConnectedSocket->GetComponentLocation()
        : bCableStartLocationInitialized && !bCableHasBeenMoved
            ? CableStartLocation + GetActorForwardVector() * InitialCableLength
            : GetActorLocation();
}

bool ACMPowerCableActor::BeginGrab(AActor* InGrabber)
{
    if (!HasAuthority() || !InGrabber || IsConnected() || IsGrabbed())
    {
        return false;
    }

    Grabber = InGrabber;
    ForceNetUpdate();
    return true;
}

void ACMPowerCableActor::ReleaseGrab()
{
    if (!HasAuthority())
    {
        return;
    }

    Grabber = nullptr;
    ForceNetUpdate();
}

bool ACMPowerCableActor::TryConnectToSocket(
    UCMPowerSocketComponent* Socket
)
{
    return Socket && Socket->TryConnectCable(this);
}

void ACMPowerCableActor::Disconnect()
{
    if (!HasAuthority() || !ConnectedSocket)
    {
        return;
    }

    ConnectedSocket->DisconnectCable(this);
}

void ACMPowerCableActor::SetConnectedSocket(
    UCMPowerSocketComponent* Socket
)
{
    if (!HasAuthority())
    {
        return;
    }

    ConnectedSocket = Socket;
    Grabber = nullptr;
    if (Socket)
    {
        SetActorLocation(Socket->GetComponentLocation());
    }
    OnConnectionChanged.Broadcast(ConnectedSocket != nullptr);
    ForceNetUpdate();
}

void ACMPowerCableActor::OnRep_ConnectedSocket()
{
    OnConnectionChanged.Broadcast(ConnectedSocket != nullptr);
}

void ACMPowerCableActor::OnRep_CableStartLocation()
{
    bCableStartLocationInitialized = true;
    bHasCachedVisualEndpoint = false;
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
    CableMeshes.Reserve(FMath::Max(VisualSegmentCount, 1));
    for (int32 Index = 0; Index < FMath::Max(VisualSegmentCount, 1); ++Index)
    {
        USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(
            this);
        if (!SplineMesh)
        {
            continue;
        }
        SplineMesh->SetMobility(EComponentMobility::Movable);
        SplineMesh->SetStaticMesh(LoadedMesh);
        SplineMesh->SetForwardAxis(ESplineMeshAxis::X, false);
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

    const FVector StartLocation = CableStartLocation;
    const FVector EndLocation = GetCableEndLocation();
    if (bHasCachedVisualEndpoint
        && FVector::DistSquared(CachedVisualEndpoint, EndLocation)
            <= FMath::Square(0.1f))
    {
        return;
    }
    CachedVisualEndpoint = EndLocation;
    bHasCachedVisualEndpoint = true;
    const FVector Delta = EndLocation - StartLocation;
    const FVector Direction = Delta.GetSafeNormal();
    const float SplineLength = Delta.Size();
    CableSpline->ClearSplinePoints(false);
    CableSpline->AddSplinePoint(StartLocation, ESplineCoordinateSpace::World,
        false);
    CableSpline->AddSplinePoint(EndLocation, ESplineCoordinateSpace::World,
        false);
    CableSpline->SetTangentAtSplinePoint(
        0, Direction * FMath::Max(SplineLength * 0.5f, 1.0f)
            + FVector(0.0f, 0.0f, -CableSag),
        ESplineCoordinateSpace::World, false);
    CableSpline->SetTangentAtSplinePoint(
        1, Direction * FMath::Max(SplineLength * 0.5f, 1.0f)
            + FVector(0.0f, 0.0f, CableSag),
        ESplineCoordinateSpace::World, false);
    CableSpline->UpdateSpline();

    const float TotalLength = CableSpline->GetSplineLength();
    for (int32 Index = 0; Index < CableMeshes.Num(); ++Index)
    {
        const float StartDistance = TotalLength * Index / CableMeshes.Num();
        const float EndDistance = TotalLength * (Index + 1)
            / CableMeshes.Num();
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
            CableThicknessScale,
            CableThicknessScale);
        CableMeshes[Index]->SetStartScale(ThicknessScale, true);
        CableMeshes[Index]->SetEndScale(ThicknessScale, true);
    }
}

void ACMPowerCableActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMPowerCableActor, Grabber);
    DOREPLIFETIME(ACMPowerCableActor, ConnectedSocket);
    DOREPLIFETIME(ACMPowerCableActor, CableStartLocation);
}
