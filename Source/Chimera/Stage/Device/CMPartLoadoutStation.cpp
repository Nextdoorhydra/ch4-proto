#include "Stage/Device/CMPartLoadoutStation.h"

#include "Collision/CMCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"
#include "Stage/Device/CMPartLoadoutStationWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMPartLoadoutStation, Log, All);

namespace
{
struct FExistingPartRecord
{
    FCMPartSlotAddress SlotAddress;
    TObjectPtr<ACMPartActorBase> Part;
};
}

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
    StoredLoadouts.SetNum(StorageSlotCount);
}

void ACMPartLoadoutStation::BeginPlay()
{
    Super::BeginPlay();

    StoredLoadouts.SetNum(StorageSlotCount);
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
    if (!HasAuthority() || !IsValidStorageSlot(SlotIndex)
        || !IsValid(Chimera))
    {
        return false;
    }

    FCMStoredPartLoadout NewLoadout;
    NewLoadout.bOccupied = true;
    const int32 ActiveSlotCount = Chimera->GetActiveSegmentCount()
        * CMControl::PartSlotsPerSegment;
    for (int32 FlatIndex = 0; FlatIndex < ActiveSlotCount; ++FlatIndex)
    {
        const FCMPartSlotAddress Address =
            CMControl::FromFlatPartSlotIndex(FlatIndex);
        const UCMPartSlotComponent* PartSlot =
            Chimera->GetPartSlotComponent(Address);
        const ACMPartActorBase* Part = PartSlot
            ? Cast<ACMPartActorBase>(PartSlot->GetAttachedPart())
            : nullptr;
        if (!IsValid(Part))
        {
            continue;
        }

        FCMStoredPartLoadoutRecord& Record =
            NewLoadout.Parts.AddDefaulted_GetRef();
        Record.SlotAddress = Address;
        Record.PartClass = Part->GetClass();
        Record.PartRowName = Part->GetPartRowName();
        Record.TierRowName = Part->GetTierRowName();
    }

    StoredLoadouts[SlotIndex] = MoveTemp(NewLoadout);
    if (StationWidget)
    {
        StationWidget->RefreshSlots();
    }
    UE_LOG(LogCMPartLoadoutStation, Display,
        TEXT("Loadout saved. Station=%s Slot=%d Parts=%d"),
        *GetName(), SlotIndex + 1, StoredLoadouts[SlotIndex].Parts.Num());
    return true;
}

bool ACMPartLoadoutStation::LoadSavedLoadout(int32 SlotIndex)
{
    ACMChimera* Chimera = NearbyChimera.Get();
    if (!HasAuthority() || !IsValidStorageSlot(SlotIndex)
        || !IsValid(Chimera) || !StoredLoadouts[SlotIndex].bOccupied)
    {
        return false;
    }

    const FCMStoredPartLoadout& Loadout = StoredLoadouts[SlotIndex];
    const int32 ActiveSlotCount = Chimera->GetActiveSegmentCount()
        * CMControl::PartSlotsPerSegment;
    for (const FCMStoredPartLoadoutRecord& Record : Loadout.Parts)
    {
        const int32 FlatIndex =
            CMControl::ToFlatPartSlotIndex(Record.SlotAddress);
        if (!Record.PartClass || FlatIndex < 0 || FlatIndex >= ActiveSlotCount
            || !Chimera->GetPartSlotComponent(Record.SlotAddress))
        {
            return false;
        }
    }

    TArray<TObjectPtr<ACMPartActorBase>> NewParts;
    NewParts.Reserve(Loadout.Parts.Num());
    for (const FCMStoredPartLoadoutRecord& Record : Loadout.Parts)
    {
        const UCMPartSlotComponent* PartSlot =
            Chimera->GetPartSlotComponent(Record.SlotAddress);
        ACMPartActorBase* NewPart = ACMPartActorBase::SpawnPartFromDataRows(
            this,
            Record.PartClass,
            Record.PartRowName,
            Record.TierRowName,
            PartSlot->GetComponentTransform(),
            Chimera);
        if (!NewPart)
        {
            for (ACMPartActorBase* SpawnedPart : NewParts)
            {
                if (IsValid(SpawnedPart))
                {
                    SpawnedPart->Destroy();
                }
            }
            return false;
        }
        NewParts.Add(NewPart);
    }

    TArray<FExistingPartRecord> ExistingParts;
    for (int32 FlatIndex = 0; FlatIndex < ActiveSlotCount; ++FlatIndex)
    {
        const FCMPartSlotAddress Address =
            CMControl::FromFlatPartSlotIndex(FlatIndex);
        UCMPartSlotComponent* PartSlot =
            Chimera->GetPartSlotComponent(Address);
        if (ACMPartActorBase* Part = PartSlot
            ? Cast<ACMPartActorBase>(PartSlot->DetachPart())
            : nullptr)
        {
            ExistingParts.Add({Address, Part});
        }
    }

    int32 AttachedCount = 0;
    for (int32 Index = 0; Index < Loadout.Parts.Num(); ++Index)
    {
        UCMPartSlotComponent* PartSlot = Chimera->GetPartSlotComponent(
            Loadout.Parts[Index].SlotAddress);
        if (!PartSlot->AttachPart(NewParts[Index]))
        {
            break;
        }
        ++AttachedCount;
    }

    if (AttachedCount != NewParts.Num())
    {
        for (int32 Index = 0; Index < AttachedCount; ++Index)
        {
            if (UCMPartSlotComponent* PartSlot = Chimera->GetPartSlotComponent(
                Loadout.Parts[Index].SlotAddress))
            {
                PartSlot->DetachPart();
            }
        }
        for (ACMPartActorBase* NewPart : NewParts)
        {
            if (IsValid(NewPart))
            {
                NewPart->Destroy();
            }
        }
        for (const FExistingPartRecord& Existing : ExistingParts)
        {
            if (UCMPartSlotComponent* PartSlot =
                Chimera->GetPartSlotComponent(Existing.SlotAddress))
            {
                PartSlot->AttachPart(Existing.Part);
            }
        }
        return false;
    }

    for (const FExistingPartRecord& Existing : ExistingParts)
    {
        if (IsValid(Existing.Part))
        {
            Existing.Part->Destroy();
        }
    }

    UE_LOG(LogCMPartLoadoutStation, Display,
        TEXT("Loadout restored. Station=%s Slot=%d Parts=%d"),
        *GetName(), SlotIndex + 1, AttachedCount);
    return true;
}

bool ACMPartLoadoutStation::IsStorageSlotOccupied(int32 SlotIndex) const
{
    return IsValidStorageSlot(SlotIndex)
        && StoredLoadouts[SlotIndex].bOccupied;
}

int32 ACMPartLoadoutStation::GetStoredPartCount(int32 SlotIndex) const
{
    return IsStorageSlotOccupied(SlotIndex)
        ? StoredLoadouts[SlotIndex].Parts.Num()
        : 0;
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

bool ACMPartLoadoutStation::IsValidStorageSlot(int32 SlotIndex) const
{
    return StoredLoadouts.IsValidIndex(SlotIndex)
        && SlotIndex >= 0 && SlotIndex < StorageSlotCount;
}
