#include "World/Mechanism/CMPowerSourceIndicatorComponent.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Stage/Trigger/Component/CMPowerSocketComponent.h"
#include "Stage/Trigger/Component/CMPowerSourceComponent.h"
#include "World/Mechanism/CMPowerSourceIndicatorWidget.h"

UCMPowerSourceIndicatorComponent::UCMPowerSourceIndicatorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickInterval = 0.0f;
    SetWidgetSpace(EWidgetSpace::World);
    SetBlendMode(EWidgetBlendMode::Transparent);
    SetTwoSided(true);
    SetDrawAtDesiredSize(true);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetPivot(FVector2D(0.5f, 0.5f));
    ConnectionBurstEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
        TEXT("/Game/Chimera/Environment/Obstacle/Powercable/NS/NS_PowerSocket_ConnectBurst.NS_PowerSocket_ConnectBurst")));
}

void UCMPowerSourceIndicatorComponent::BeginPlay()
{
    Super::BeginPlay();

    FadeEndDistance = 0.0f;
    SetBlendMode(EWidgetBlendMode::Transparent);
    SetTwoSided(true);
    PowerSource = ResolvePowerSource();
    if (!PowerSource.IsValid())
    {
        PowerSocket = ResolvePowerSocket();
    }
    if (!PowerSource.IsValid() && !PowerSocket.IsValid())
    {
        SetComponentTickEnabled(false);
        SetVisibility(false);
        return;
    }

    ConnectionBurstEffect.LoadSynchronous();

    InitWidget();
    if (!Cast<UCMPowerSourceIndicatorWidget>(GetUserWidgetObject()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Power source indicator has no compatible WidgetClass: %s"),
            *GetNameSafe(GetOwner()));
        SetComponentTickEnabled(false);
        SetVisibility(false);
        return;
    }

    if (PowerSource.IsValid())
    {
        PowerSource->OnConnectionChanged.AddUniqueDynamic(
            this, &ThisClass::HandleSourceStateChanged);
        PowerSource->OnPowerStateChanged.AddUniqueDynamic(
            this, &ThisClass::HandleSourceStateChanged);
    }
    else
    {
        PowerSocket->OnConnectionChanged.AddUniqueDynamic(
            this, &ThisClass::HandleSourceStateChanged);
        PowerSocket->OnPowerStateChanged.AddUniqueDynamic(
            this, &ThisClass::HandleSourceStateChanged);
    }
    SetComponentTickEnabled(true);
    ApplySourceState(false);
}

void UCMPowerSourceIndicatorComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (PowerSource.IsValid())
    {
        PowerSource->OnConnectionChanged.RemoveDynamic(
            this, &ThisClass::HandleSourceStateChanged);
        PowerSource->OnPowerStateChanged.RemoveDynamic(
            this, &ThisClass::HandleSourceStateChanged);
    }
    if (PowerSocket.IsValid())
    {
        PowerSocket->OnConnectionChanged.RemoveDynamic(
            this, &ThisClass::HandleSourceStateChanged);
        PowerSocket->OnPowerStateChanged.RemoveDynamic(
            this, &ThisClass::HandleSourceStateChanged);
    }
    PowerSource.Reset();
    PowerSocket.Reset();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ConnectionBurstTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void UCMPowerSourceIndicatorComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (FadeEndDistance <= 0.0f)
    {
        return;
    }

    DistanceUpdateElapsed += DeltaTime;
    if (bInsideFadeRange
        || DistanceUpdateElapsed >= FMath::Max(
            DistanceUpdateInterval, 0.02f))
    {
        DistanceUpdateElapsed = 0.0f;
        UpdateDistanceFade();
    }
}

void UCMPowerSourceIndicatorComponent::HandleSourceStateChanged(bool bState)
{
    ApplySourceState(bHasAppliedState);
}

UCMPowerSourceComponent*
UCMPowerSourceIndicatorComponent::ResolvePowerSource() const
{
    if (UCMPowerSourceComponent* AttachedSource =
            Cast<UCMPowerSourceComponent>(GetAttachParent()))
    {
        return AttachedSource;
    }

    TArray<UCMPowerSourceComponent*> Sources;
    if (AActor* Owner = GetOwner())
    {
        Owner->GetComponents(Sources);
    }
    if (Sources.Num() == 1)
    {
        return Sources[0];
    }

    return nullptr;
}

UCMPowerSocketComponent*
UCMPowerSourceIndicatorComponent::ResolvePowerSocket() const
{
    if (UCMPowerSocketComponent* AttachedSocket =
            Cast<UCMPowerSocketComponent>(GetAttachParent()))
    {
        return AttachedSocket;
    }

    TArray<UCMPowerSocketComponent*> Sockets;
    if (AActor* Owner = GetOwner())
    {
        Owner->GetComponents(Sockets);
    }
    if (Sockets.Num() == 1)
    {
        return Sockets[0];
    }

    UE_LOG(LogTemp, Warning,
        TEXT("Power indicator requires exactly one Source/Socket or direct endpoint attachment. Owner=%s Sockets=%d"),
        *GetNameSafe(GetOwner()),
        Sockets.Num());
    return nullptr;
}

void UCMPowerSourceIndicatorComponent::ApplySourceState(bool bAnimate)
{
    UCMPowerSourceComponent* Source = PowerSource.Get();
    UCMPowerSocketComponent* Socket = PowerSocket.Get();
    UCMPowerSourceIndicatorWidget* IndicatorWidget =
        Cast<UCMPowerSourceIndicatorWidget>(GetUserWidgetObject());
    if ((!Source && !Socket) || !IndicatorWidget)
    {
        return;
    }

    const bool bPoweredConnection = Source
        ? Source->IsPhysicallyConnected() && Source->IsProvidingPower()
        : Socket->IsPhysicallyConnected() && Socket->IsPowered();

    if (bAnimate && bPoweredConnection && !bLastPoweredConnection)
    {
        GetWorld()->GetTimerManager().SetTimer(
            ConnectionBurstTimerHandle,
            this,
            &ThisClass::SpawnConnectionBurst,
            IndicatorWidget->GetPowerOnFlashDelay(),
            false);
    }
    else if (!bPoweredConnection)
    {
        GetWorld()->GetTimerManager().ClearTimer(ConnectionBurstTimerHandle);
    }

    bLastPoweredConnection = bPoweredConnection;
    IndicatorWidget->SetPowerState(bPoweredConnection, bAnimate);
    bHasAppliedState = true;
    UpdateDistanceFade();
}

void UCMPowerSourceIndicatorComponent::SpawnConnectionBurst()
{
    USceneComponent* Endpoint = PowerSocket.IsValid()
        ? static_cast<USceneComponent*>(PowerSocket.Get())
        : static_cast<USceneComponent*>(PowerSource.Get());
    UNiagaraSystem* Effect = ConnectionBurstEffect.LoadSynchronous();
    if (!Endpoint || !Effect || !IsVisible())
    {
        return;
    }

    const FVector SpawnLocation = Endpoint->GetComponentLocation()
        + Endpoint->GetUpVector() * ConnectionBurstOffset;
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        this,
        Effect,
        SpawnLocation,
        Endpoint->GetComponentRotation(),
        FVector::OneVector,
        true,
        true,
        ENCPoolMethod::AutoRelease);
}

void UCMPowerSourceIndicatorComponent::UpdateDistanceFade()
{
    if (FadeEndDistance <= 0.0f)
    {
        bInsideFadeRange = false;
        SetVisibility(true);
        if (UUserWidget* IndicatorWidget = GetUserWidgetObject())
        {
            IndicatorWidget->SetRenderOpacity(1.0f);
        }
        return;
    }

    const APlayerController* LocalPlayerController =
        UGameplayStatics::GetPlayerController(this, 0);
    const APlayerCameraManager* CameraManager = LocalPlayerController
        ? LocalPlayerController->PlayerCameraManager
        : nullptr;
    if (!CameraManager)
    {
        bInsideFadeRange = false;
        SetVisibility(true);
        if (UUserWidget* IndicatorWidget = GetUserWidgetObject())
        {
            IndicatorWidget->SetRenderOpacity(1.0f);
        }
        return;
    }

    const float Distance = FVector::Dist2D(
        CameraManager->GetCameraLocation(), GetComponentLocation());
    bInsideFadeRange = FadeEndDistance > FadeStartDistance
        && Distance > FadeStartDistance
        && Distance < FadeEndDistance;

    const float Opacity = FadeEndDistance <= FadeStartDistance
        ? Distance < FadeEndDistance ? 1.0f : 0.0f
        : 1.0f - FMath::Clamp(
            (Distance - FadeStartDistance)
                / (FadeEndDistance - FadeStartDistance),
            0.0f,
            1.0f);

    SetVisibility(Opacity > KINDA_SMALL_NUMBER);
    if (UUserWidget* IndicatorWidget = GetUserWidgetObject())
    {
        IndicatorWidget->SetRenderOpacity(Opacity);
    }
}
