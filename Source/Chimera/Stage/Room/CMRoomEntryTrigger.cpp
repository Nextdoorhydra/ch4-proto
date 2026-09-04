#include "Stage/Room/CMRoomEntryTrigger.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Player/CMChimera.h"
#include "Stage/Device/CMStageDoorBase.h"
#include "Stage/Room/CMRoomStreamingController.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogChimeraRoomEntry, Log, All);

ACMRoomEntryTrigger::ACMRoomEntryTrigger()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    EntryVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryVolume"));
    SetRootComponent(EntryVolume);
    EntryVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    EntryVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    EntryVolume->SetGenerateOverlapEvents(true);
}

// 서버에서 하나의 Persistent RoomStreamingController를 찾고 Overlap 구독
void ACMRoomEntryTrigger::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority())
    {
        return;
    }

    int32 ControllerCount = 0;
    for (TActorIterator<ACMRoomStreamingController> It(GetWorld()); It; ++It)
    {
        StreamingController = *It;
        ++ControllerCount;
    }
    if (ControllerCount != 1)
    {
        UE_LOG(LogChimeraRoomEntry, Error,
            TEXT("RoomEntryTrigger requires exactly one RoomStreamingController. Trigger=%s Count=%d"),
            *GetName(), ControllerCount);
        StreamingController.Reset();
    }

    EntryVolume->OnComponentBeginOverlap.AddUniqueDynamic(
        this, &ThisClass::HandleBeginOverlap);
}

void ACMRoomEntryTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (EntryBlockerDoor)
    {
        EntryBlockerDoor->OnDoorTransitionFinished.RemoveAll(this);
    }
    Super::EndPlay(EndPlayReason);
}

// 활성 몸통 마디 하나 이상이 진입하면 룸 확정 시작
void ACMRoomEntryTrigger::HandleBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!bCommitStarted)
    {
        TryCommitForChimera(Cast<ACMChimera>(OtherActor));
    }
}

void ACMRoomEntryTrigger::TryCommitForChimera(ACMChimera* Chimera)
{
    if (!IsValid(Chimera)
        || !Chimera->IsAnyActiveBodySegmentOverlapping(EntryVolume))
    {
        return;
    }

    bCommitStarted = true;
    if (!IsValid(EntryBlockerDoor) || !EntryBlockerDoor->IsElementActive())
    {
        CommitRoom();
        return;
    }

    if (bWaitForDoorClosed)
    {
        EntryBlockerDoor->OnDoorTransitionFinished.AddUniqueDynamic(
            this, &ThisClass::HandleDoorTransitionFinished);
    }
    EntryBlockerDoor->DeactivateDevice();
    if (!bWaitForDoorClosed)
    {
        CommitRoom();
    }
}

// 닫힘 애니메이션과 블로킹 Collision 완료 신호에서만 룸 전환 진행
void ACMRoomEntryTrigger::HandleDoorTransitionFinished(bool bIsOpen)
{
    if (!bIsOpen)
    {
        EntryBlockerDoor->OnDoorTransitionFinished.RemoveAll(this);
        CommitRoom();
    }
}

void ACMRoomEntryTrigger::CommitRoom()
{
    if (bCommitFinished || !StreamingController.IsValid())
    {
        return;
    }
    bCommitFinished = StreamingController->CommitRoom(RoomId);
    if (!bCommitFinished)
    {
        bCommitStarted = false;
    }
}

#if WITH_EDITOR
EDataValidationResult ACMRoomEntryTrigger::IsDataValid(
    FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    if (RoomId.IsNone())
    {
        Context.AddWarning(FText::FromString(
            TEXT("RoomEntryTrigger의 RoomId가 비어 있습니다.")));
    }
    if (bWaitForDoorClosed && !EntryBlockerDoor)
    {
        Context.AddWarning(FText::FromString(
            TEXT("문 완료를 기다리도록 설정했지만 EntryBlockerDoor가 없습니다. 첫 룸이면 무시할 수 있습니다.")));
    }
    return Result;
}
#endif
