#include "Stage/Device/CMPartLoadoutStation.h"

#include "Collision/CMCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Player/CMChimera.h"
#include "Stage/Device/CMPartLoadoutStorageSubsystem.h"
#include "Stage/Device/CMPartLoadoutStationWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMPartLoadoutStation, Log, All);

ACMPartLoadoutStation::ACMPartLoadoutStation()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("StationMesh"));
    StationMesh->SetupAttachment(SceneRoot);
    StationMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    InteractionVolume = CreateDefaultSubobject<UBoxComponent>(
        TEXT("InteractionVolume"));
    InteractionVolume->SetupAttachment(SceneRoot);
    InteractionVolume->SetBoxExtent(FVector(250.0f));
    InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionVolume->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionVolume->SetCollisionResponseToChannel(
        CMCollision::ChimeraHurtbox, ECR_Overlap);
    InteractionVolume->SetGenerateOverlapEvents(true);
    InteractionVolume->SetCanEverAffectNavigation(false);

    StationWidgetClass = UCMPartLoadoutStationWidget::StaticClass();
}

void ACMPartLoadoutStation::BeginPlay()
{
    Super::BeginPlay();

    InteractionVolume->OnComponentBeginOverlap.AddUniqueDynamic(
        this, &ThisClass::HandleInteractionBeginOverlap);
    InteractionVolume->OnComponentEndOverlap.AddUniqueDynamic(
        this, &ThisClass::HandleInteractionEndOverlap);
}

void ACMPartLoadoutStation::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    HideStationUI();
    Super::EndPlay(EndPlayReason);
}

bool ACMPartLoadoutStation::SaveCurrentLoadout(int32 SlotIndex)
{
    ACMChimera* Chimera = NearbyChimera.Get();
    UCMPartLoadoutStorageSubsystem* Storage = GetStorageSubsystem();
    if (!HasAuthority() || !Storage
        || !Storage->IsValidStorageSlot(SlotIndex) || !IsValid(Chimera))
    {
        return false;
    }

    if (!Storage->SaveCurrentLoadout(Chimera, SlotIndex))
    {
        return false;
    }

    const int32 SavedPartCount = Storage->GetStoredPartCount(SlotIndex);
    if (StationWidget)
    {
        StationWidget->RefreshSlots();
    }
    UE_LOG(LogCMPartLoadoutStation, Display,
        TEXT("Loadout saved. Station=%s Slot=%d Parts=%d"),
        *GetName(), SlotIndex + 1, SavedPartCount);
    return true;
}

bool ACMPartLoadoutStation::LoadSavedLoadout(int32 SlotIndex)
{
    ACMChimera* Chimera = NearbyChimera.Get();
    UCMPartLoadoutStorageSubsystem* Storage = GetStorageSubsystem();
    if (!HasAuthority() || !Storage || !IsValid(Chimera)
        || !Storage->LoadSavedLoadout(Chimera, SlotIndex))
    {
        return false;
    }

    UE_LOG(LogCMPartLoadoutStation, Display,
        TEXT("Loadout restored. Station=%s Slot=%d Parts=%d"),
        *GetName(), SlotIndex + 1,
        Storage->GetStoredPartCount(SlotIndex));
    return true;
}

bool ACMPartLoadoutStation::IsStorageSlotOccupied(int32 SlotIndex) const
{
    const UCMPartLoadoutStorageSubsystem* Storage = GetStorageSubsystem();
    return Storage && Storage->IsStorageSlotOccupied(SlotIndex);
}

int32 ACMPartLoadoutStation::GetStoredPartCount(int32 SlotIndex) const
{
    const UCMPartLoadoutStorageSubsystem* Storage = GetStorageSubsystem();
    return Storage ? Storage->GetStoredPartCount(SlotIndex) : 0;
}

int32 ACMPartLoadoutStation::GetStorageSlotCount() const
{
    return UCMPartLoadoutStorageSubsystem::StorageSlotCount;
}

void ACMPartLoadoutStation::CloseStationUI()
{
    HideStationUI();
}

void ACMPartLoadoutStation::HandleInteractionBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    ACMChimera* Chimera = Cast<ACMChimera>(OtherActor);
    if (!HasAuthority() || !IsValid(Chimera) || !IsValid(OtherComponent))
    {
        return;
    }

    NearbyChimera = Chimera;
    OverlappingChimeraComponents.Add(OtherComponent);
    ShowStationUI();
}

void ACMPartLoadoutStation::HandleInteractionEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex)
{
    if (!HasAuthority())
    {
        return;
    }

    OverlappingChimeraComponents.Remove(OtherComponent);
    for (auto It = OverlappingChimeraComponents.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            It.RemoveCurrent();
        }
    }
    if (OverlappingChimeraComponents.IsEmpty())
    {
        NearbyChimera.Reset();
        HideStationUI();
    }
}

void ACMPartLoadoutStation::ShowStationUI()
{
    if (StationWidget || GetNetMode() == NM_DedicatedServer
        || !StationWidgetClass)
    {
        return;
    }

    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        return;
    }

    StationWidget = CreateWidget<UCMPartLoadoutStationWidget>(
        PlayerController, StationWidgetClass);
    if (!StationWidget)
    {
        return;
    }

    StationWidget->InitializeStation(this);
    StationWidget->AddToViewport(100);
    bPreviousShowMouseCursor = PlayerController->bShowMouseCursor;
    PlayerController->bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(StationWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController->SetInputMode(InputMode);
}

void ACMPartLoadoutStation::HideStationUI()
{
    if (!StationWidget)
    {
        return;
    }

    StationWidget->RemoveFromParent();
    StationWidget = nullptr;
    if (APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0))
    {
        PlayerController->bShowMouseCursor = bPreviousShowMouseCursor;
        PlayerController->SetInputMode(FInputModeGameOnly());
    }
}

UCMPartLoadoutStorageSubsystem*
ACMPartLoadoutStation::GetStorageSubsystem() const
{
    UGameInstance* GameInstance = GetGameInstance();
    return GameInstance
        ? GameInstance->GetSubsystem<UCMPartLoadoutStorageSubsystem>()
        : nullptr;
}
