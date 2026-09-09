#include "Ping/CMWorldPing.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Ping/CMPingGroundWidget.h"
#include "Ping/CMWorldPingWidget.h"

namespace
{
    void ConfigureWorldWidget(UWidgetComponent& Component)
    {
        Component.SetWidgetSpace(EWidgetSpace::World);
        Component.SetBlendMode(EWidgetBlendMode::Transparent);
        Component.SetTwoSided(true);
        Component.SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component.SetGenerateOverlapEvents(false);
        Component.SetTranslucentSortPriority(TNumericLimits<int16>::Max());
        Component.ComponentTags.Add(TEXT("NoVisionOccluder"));
        Component.SetBoundsScale(4.0f);
    }
}

ACMWorldPing::ACMWorldPing()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.0f;
    bReplicates = true;
    SetReplicateMovement(false);
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(20.0f);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    Tags.Add(TEXT("NoVisionOccluder"));

    GroundWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(
        TEXT("PingGroundWidget"));
    GroundWidgetComponent->SetupAttachment(SceneRoot);
    GroundWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
    GroundWidgetComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    GroundWidgetComponent->SetRelativeScale3D(FVector(0.7f));
    GroundWidgetComponent->SetWidgetClass(UCMPingGroundWidget::StaticClass());
    GroundWidgetComponent->SetDrawSize(FVector2D(256.0f, 256.0f));
    GroundWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
    GroundWidgetComponent->SetBackgroundColor(FLinearColor::Transparent);
    ConfigureWorldWidget(*GroundWidgetComponent);

    MarkerWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(
        TEXT("PingMarkerWidget"));
    MarkerWidgetComponent->SetupAttachment(SceneRoot);
    MarkerWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 105.0f));
    MarkerWidgetComponent->SetWidgetClass(UCMWorldPingWidget::StaticClass());
    MarkerWidgetComponent->SetDrawSize(FVector2D(190.0f, 155.0f));
    MarkerWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
    ConfigureWorldWidget(*MarkerWidgetComponent);
}

void ACMWorldPing::BeginPlay()
{
    Super::BeginPlay();

    SetActorTickEnabled(GetNetMode() != NM_DedicatedServer);
    GroundWidgetComponent->InitWidget();
    MarkerWidgetComponent->InitWidget();
    ApplyPresentation();
    if (HasAuthority())
    {
        SetLifeSpan(CMPing::DisplayDuration);
    }
}

void ACMWorldPing::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const APlayerController* LocalController =
        UGameplayStatics::GetPlayerController(this, 0);
    const APlayerCameraManager* CameraManager = LocalController
        ? LocalController->PlayerCameraManager
        : nullptr;
    if (CameraManager)
    {
        MarkerWidgetComponent->SetWorldRotation(
            (CameraManager->GetCameraLocation()
                - MarkerWidgetComponent->GetComponentLocation()).Rotation());
    }
}

void ACMWorldPing::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMWorldPing, PingType);
    DOREPLIFETIME(ACMWorldPing, PingingPlayerName);
    DOREPLIFETIME(ACMWorldPing, PingingPlayerColor);
}

void ACMWorldPing::InitializePing(
    ECMPingType Type,
    const FString& PlayerName,
    const FLinearColor& PlayerColor)
{
    check(HasAuthority());
    PingType = Type;
    PingingPlayerName = PlayerName;
    PingingPlayerColor = PlayerColor;
    ApplyPresentation();
}

void ACMWorldPing::EnforceServerLimit(UWorld& World)
{
    check(World.GetNetMode() != NM_Client);

    TArray<ACMWorldPing*> ActivePings;
    TArray<float> Ages;
    for (TActorIterator<ACMWorldPing> It(&World); It; ++It)
    {
        if (IsValid(*It) && !It->IsActorBeingDestroyed())
        {
            ActivePings.Add(*It);
            Ages.Add(It->GetGameTimeSinceCreation());
        }
    }

    while (ActivePings.Num() >= CMPing::MaxActivePings)
    {
        const int32 OldestIndex = CMPing::FindOldestAgeIndex(Ages);
        if (!ActivePings.IsValidIndex(OldestIndex))
        {
            return;
        }
        ActivePings[OldestIndex]->Destroy();
        ActivePings.RemoveAtSwap(OldestIndex);
        Ages.RemoveAtSwap(OldestIndex);
    }
}

void ACMWorldPing::OnRep_Presentation()
{
    ApplyPresentation();
}

void ACMWorldPing::ApplyPresentation()
{
    if (UCMPingGroundWidget* GroundWidget = Cast<UCMPingGroundWidget>(
            GroundWidgetComponent->GetUserWidgetObject()))
    {
        GroundWidget->SetPingType(PingType);
    }
    if (UCMWorldPingWidget* Widget = Cast<UCMWorldPingWidget>(
            MarkerWidgetComponent->GetUserWidgetObject()))
    {
        Widget->SetPresentation(
            PingType, PingingPlayerName, PingingPlayerColor);
    }
}
