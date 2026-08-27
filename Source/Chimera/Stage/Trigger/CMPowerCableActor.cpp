#include "Stage/Trigger/CMPowerCableActor.h"

#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Stage/Trigger/Component/CMPowerSocketComponent.h"

ACMPowerCableActor::ACMPowerCableActor()
{
    PrimaryActorTick.bCanEverTick = true;
    CableSpline = CreateDefaultSubobject<USplineComponent>(TEXT("CableSpline"));
    SetRootComponent(CableSpline);
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
    UpdateCableVisual();
}

FVector ACMPowerCableActor::GetCableEndLocation() const
{
    return ConnectedSocket
        ? ConnectedSocket->GetComponentLocation()
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
            TEXT("Power cable visual load failed. Cable=%s Mesh=%s LoadGroup=%s"),
            *GetName(), *CableMesh.ToSoftObjectPath().ToString(),
            *LoadGroupId.ToString());
        return;
    }
    TryBuildCableVisual();
}

void ACMPowerCableActor::RefreshCableVisualState()
{
    if (CableMesh.IsNull() || bCableVisualReady || bCableVisualFailed)
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
    UStaticMesh* LoadedMesh = CableMesh.Get();
    if (!LoadedMesh || !CableSpline)
    {
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
        SplineMesh->SetStaticMesh(LoadedMesh);
        SplineMesh->SetForwardAxis(ESplineMeshAxis::X, false);
        SplineMesh->SetupAttachment(CableSpline);
        AddInstanceComponent(SplineMesh);
        SplineMesh->RegisterComponent();
        CableMeshes.Add(SplineMesh);
    }
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
        CableMeshes[Index]->SetStartScale(ThicknessScale, false);
        CableMeshes[Index]->SetEndScale(ThicknessScale, false);
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
