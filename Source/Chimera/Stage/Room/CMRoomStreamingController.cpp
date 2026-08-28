#include "Stage/Room/CMRoomStreamingController.h"

#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Stage/Room/CMRoomCheckpoint.h"
#include "Stage/Room/CMRoomEntryTrigger.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogChimeraRoomStreaming, Log, All);

namespace
{
struct FCMDesiredRoomStreamingState
{
    bool bLoaded = false;
    bool bVisible = false;
};

FCMDesiredRoomStreamingState ResolveDesiredRoomStreamingState(
    int32 RoomIndex,
    int32 CurrentRoomIndex,
    int32 RoomCount)
{
    if (RoomIndex < 0 || RoomIndex >= RoomCount
        || CurrentRoomIndex < 0 || CurrentRoomIndex >= RoomCount)
    {
        return {};
    }

    FCMDesiredRoomStreamingState Result;
    Result.bLoaded = RoomIndex >= CurrentRoomIndex
        && RoomIndex <= CurrentRoomIndex + 2;
    Result.bVisible = RoomIndex >= CurrentRoomIndex
        && RoomIndex <= CurrentRoomIndex + 1;
    return Result;
}
}

ACMRoomStreamingController::ACMRoomStreamingController()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
}

void ACMRoomStreamingController::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, CurrentRoomIndex);
}

// 모든 머신의 등록된 Streaming Level에 초기 로드 창을 적용
void ACMRoomStreamingController::BeginPlay()
{
    Super::BeginPlay();

    for (const FCMRoomStreamingEntry& Room : Rooms)
    {
        if (ULevelStreaming* StreamingLevel = ResolveStreamingLevel(Room))
        {
            StreamingLevel->OnLevelLoaded.AddUniqueDynamic(
                this, &ThisClass::HandleAnyRoomLevelLoaded);
        }
    }

    if (HasAuthority())
    {
        if (Rooms.IsValidIndex(InitialRoomIndex))
        {
            CurrentRoomIndex = InitialRoomIndex;
            ForceNetUpdate();
        }
        else
        {
            UE_LOG(LogChimeraRoomStreaming, Error,
                TEXT("RoomStreamingController has no valid initial room. Controller=%s InitialIndex=%d RoomCount=%d"),
                *GetName(), InitialRoomIndex, Rooms.Num());
        }
    }
    ApplyStreamingWindow();
}

// 현재 룸 다음 순서만 허용해 선형 진행과 중복 Overlap을 안전하게 처리
bool ACMRoomStreamingController::CommitRoom(FName RoomId)
{
    if (!HasAuthority())
    {
        return false;
    }

    const int32 RequestedIndex = FindRoomIndex(RoomId);
    if (!Rooms.IsValidIndex(RequestedIndex))
    {
        UE_LOG(LogChimeraRoomStreaming, Error,
            TEXT("Room commit references an unknown RoomId. Controller=%s Room=%s"),
            *GetName(), *RoomId.ToString());
        return false;
    }
    if (RequestedIndex == CurrentRoomIndex)
    {
        ActiveCheckpointRoomIndex = RequestedIndex;
        return true;
    }
    if (RequestedIndex != CurrentRoomIndex + 1)
    {
        UE_LOG(LogChimeraRoomStreaming, Warning,
            TEXT("Non-sequential room commit was rejected. Controller=%s Current=%d Requested=%d Room=%s"),
            *GetName(), CurrentRoomIndex, RequestedIndex, *RoomId.ToString());
        return false;
    }

    CurrentRoomIndex = RequestedIndex;
    ActiveCheckpointRoomIndex = RequestedIndex;
    ApplyStreamingWindow();
    ForceNetUpdate();

    if (CurrentRoomIndex == Rooms.Num() - 1)
    {
        OnFinalRoomCommitted.Broadcast(RoomId);
    }
    return true;
}

bool ACMRoomStreamingController::TryGetActiveCheckpointTransform(
    FTransform& OutTransform) const
{
    if (!Rooms.IsValidIndex(ActiveCheckpointRoomIndex))
    {
        return false;
    }

    const FCMRoomStreamingEntry& Room = Rooms[ActiveCheckpointRoomIndex];
    const ULevelStreaming* StreamingLevel = ResolveStreamingLevel(Room);
    const ULevel* LoadedLevel = StreamingLevel
        ? StreamingLevel->GetLoadedLevel() : nullptr;
    if (!LoadedLevel)
    {
        return false;
    }

    const ACMRoomCheckpoint* FoundCheckpoint = nullptr;
    for (const AActor* Actor : LoadedLevel->Actors)
    {
        const ACMRoomCheckpoint* Checkpoint = Cast<ACMRoomCheckpoint>(Actor);
        if (!Checkpoint || Checkpoint->GetRoomId() != Room.RoomId)
        {
            continue;
        }
        if (FoundCheckpoint)
        {
            return false;
        }
        FoundCheckpoint = Checkpoint;
    }

    if (!FoundCheckpoint)
    {
        return false;
    }
    OutTransform = FoundCheckpoint->GetActorTransform();
    return true;
}

FName ACMRoomStreamingController::GetCurrentRoomId() const
{
    return Rooms.IsValidIndex(CurrentRoomIndex)
        ? Rooms[CurrentRoomIndex].RoomId
        : NAME_None;
}

int32 ACMRoomStreamingController::FindRoomIndex(FName RoomId) const
{
    return Rooms.IndexOfByPredicate(
        [RoomId](const FCMRoomStreamingEntry& Room)
        {
            return Room.RoomId == RoomId;
        });
}

// Persistent Level에 등록된 서브레벨을 에셋 이름으로 조회
ULevelStreaming* ACMRoomStreamingController::ResolveStreamingLevel(
    const FCMRoomStreamingEntry& Room) const
{
    if (Room.Level.IsNull())
    {
        return nullptr;
    }
    return UGameplayStatics::GetStreamingLevel(
        this,
        Room.Level.ToSoftObjectPath().GetAssetFName());
}

// Room N 기준 N과 N+1은 표시하고 N+2는 숨김 로드하며 이전 룸은 해제
void ACMRoomStreamingController::ApplyStreamingWindow()
{
    if (!Rooms.IsValidIndex(CurrentRoomIndex))
    {
        return;
    }

    for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
    {
        const FCMDesiredRoomStreamingState Desired =
            ResolveDesiredRoomStreamingState(
                RoomIndex, CurrentRoomIndex, Rooms.Num());
        SetLocalRoomStreamingState(
            RoomIndex, Desired.bLoaded, Desired.bVisible);
    }
}

void ACMRoomStreamingController::SetLocalRoomStreamingState(
    int32 RoomIndex,
    bool bShouldBeLoaded,
    bool bShouldBeVisible)
{
    if (!Rooms.IsValidIndex(RoomIndex))
    {
        return;
    }

    ULevelStreaming* StreamingLevel = ResolveStreamingLevel(Rooms[RoomIndex]);
    if (!StreamingLevel)
    {
        UE_LOG(LogChimeraRoomStreaming, Error,
            TEXT("Room level is not registered as a Persistent Level sublevel. Controller=%s Room=%s Level=%s"),
            *GetName(),
            *Rooms[RoomIndex].RoomId.ToString(),
            *Rooms[RoomIndex].Level.ToSoftObjectPath().ToString());
        return;
    }

    if (!bShouldBeLoaded)
    {
        StreamingLevel->SetShouldBeVisible(false);
        StreamingLevel->SetShouldBeLoaded(false);
        return;
    }

    StreamingLevel->SetShouldBeLoaded(true);
    StreamingLevel->SetShouldBeVisible(bShouldBeVisible);
}

void ACMRoomStreamingController::OnRep_CurrentRoomIndex()
{
    ApplyStreamingWindow();
}

// 로드 완료 시 룸마다 진입 트리거가 정확히 하나인지 런타임에서도 확인
void ACMRoomStreamingController::HandleAnyRoomLevelLoaded()
{
    ValidateLoadedRoomEntryTriggers();
}

void ACMRoomStreamingController::ValidateLoadedRoomEntryTriggers() const
{
    for (const FCMRoomStreamingEntry& Room : Rooms)
    {
        const ULevelStreaming* StreamingLevel = ResolveStreamingLevel(Room);
        const ULevel* LoadedLevel = StreamingLevel
            ? StreamingLevel->GetLoadedLevel() : nullptr;
        if (!LoadedLevel)
        {
            continue;
        }

        int32 MatchingTriggerCount = 0;
        int32 MatchingCheckpointCount = 0;
        for (const AActor* Actor : LoadedLevel->Actors)
        {
            const ACMRoomEntryTrigger* Trigger = Cast<ACMRoomEntryTrigger>(Actor);
            if (Trigger && Trigger->GetRoomId() == Room.RoomId)
            {
                ++MatchingTriggerCount;
            }
            const ACMRoomCheckpoint* Checkpoint = Cast<ACMRoomCheckpoint>(Actor);
            if (Checkpoint && Checkpoint->GetRoomId() == Room.RoomId)
            {
                ++MatchingCheckpointCount;
            }
        }
        if (MatchingCheckpointCount != 1)
        {
            UE_LOG(LogChimeraRoomStreaming, Warning,
                TEXT("Room requires exactly one matching RoomCheckpoint. Room=%s Level=%s Count=%d"),
                *Room.RoomId.ToString(),
                *Room.Level.ToSoftObjectPath().ToString(),
                MatchingCheckpointCount);
        }
        if (MatchingTriggerCount != 1)
        {
            UE_LOG(LogChimeraRoomStreaming, Warning,
                TEXT("Room requires exactly one matching RoomEntryTrigger. Room=%s Level=%s Count=%d"),
                *Room.RoomId.ToString(),
                *Room.Level.ToSoftObjectPath().ToString(),
                MatchingTriggerCount);
        }
    }
}

#if WITH_EDITOR
// 콘텐츠 검증에서 룸 ID, 서브레벨, 진입 트리거 누락과 중복을 경고
EDataValidationResult ACMRoomStreamingController::IsDataValid(
    FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    TSet<FName> RoomIds;
    TSet<FSoftObjectPath> RoomLevels;

    if (Rooms.IsEmpty())
    {
        Context.AddWarning(FText::FromString(
            TEXT("RoomStreamingController에 Room이 등록되지 않았습니다.")));
        return Result;
    }

    for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
    {
        const FCMRoomStreamingEntry& Room = Rooms[RoomIndex];
        if (Room.RoomId.IsNone() || RoomIds.Contains(Room.RoomId))
        {
            Context.AddWarning(FText::FromString(FString::Printf(
                TEXT("Room %d의 RoomId가 비어 있거나 중복되었습니다: %s"),
                RoomIndex, *Room.RoomId.ToString())));
        }
        RoomIds.Add(Room.RoomId);

        if (Room.Level.IsNull() || RoomLevels.Contains(Room.Level.ToSoftObjectPath()))
        {
            Context.AddWarning(FText::FromString(FString::Printf(
                TEXT("Room %d (%s)의 Level이 비어 있거나 중복되었습니다."),
                RoomIndex, *Room.RoomId.ToString())));
            continue;
        }
        RoomLevels.Add(Room.Level.ToSoftObjectPath());

        const UWorld* RoomWorld = Room.Level.LoadSynchronous();
        const ULevel* RoomLevel = RoomWorld ? RoomWorld->PersistentLevel : nullptr;
        int32 MatchingTriggerCount = 0;
        int32 MatchingCheckpointCount = 0;
        if (RoomLevel)
        {
            for (const AActor* Actor : RoomLevel->Actors)
            {
                const ACMRoomEntryTrigger* Trigger = Cast<ACMRoomEntryTrigger>(Actor);
                if (Trigger && Trigger->GetRoomId() == Room.RoomId)
                {
                    ++MatchingTriggerCount;
                }
                const ACMRoomCheckpoint* Checkpoint = Cast<ACMRoomCheckpoint>(Actor);
                if (Checkpoint && Checkpoint->GetRoomId() == Room.RoomId)
                {
                    ++MatchingCheckpointCount;
                }
            }
        }
        if (MatchingCheckpointCount != 1)
        {
            Context.AddWarning(FText::FromString(FString::Printf(
                TEXT("Room %s에는 같은 RoomId를 가진 RoomCheckpoint가 정확히 하나 필요합니다. 현재: %d"),
                *Room.RoomId.ToString(), MatchingCheckpointCount)));
        }
        if (MatchingTriggerCount != 1)
        {
            Context.AddWarning(FText::FromString(FString::Printf(
                TEXT("Room %s에는 같은 RoomId를 가진 RoomEntryTrigger가 정확히 하나 필요합니다. 현재: %d"),
                *Room.RoomId.ToString(), MatchingTriggerCount)));
        }
    }

    return Result;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMRoomStreamingWindowAutomationTest,
    "Chimera.Stage.RoomStreamingWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMRoomStreamingWindowAutomationTest::RunTest(const FString& Parameters)
{
    const FCMDesiredRoomStreamingState Current =
        ResolveDesiredRoomStreamingState(1, 1, 5);
    const FCMDesiredRoomStreamingState Next =
        ResolveDesiredRoomStreamingState(2, 1, 5);
    const FCMDesiredRoomStreamingState NextNext =
        ResolveDesiredRoomStreamingState(3, 1, 5);
    const FCMDesiredRoomStreamingState Previous =
        ResolveDesiredRoomStreamingState(0, 1, 5);

    TestTrue(TEXT("Current room is loaded and visible"),
        Current.bLoaded && Current.bVisible);
    TestTrue(TEXT("Next room is loaded and visible"),
        Next.bLoaded && Next.bVisible);
    TestTrue(TEXT("Next-next room is loaded but hidden"),
        NextNext.bLoaded && !NextNext.bVisible);
    TestTrue(TEXT("Previous room is unloaded"),
        !Previous.bLoaded && !Previous.bVisible);

    const FCMDesiredRoomStreamingState Final =
        ResolveDesiredRoomStreamingState(4, 4, 5);
    const FCMDesiredRoomStreamingState BeyondFinal =
        ResolveDesiredRoomStreamingState(5, 4, 5);
    TestTrue(TEXT("Final room remains loaded and visible"),
        Final.bLoaded && Final.bVisible);
    TestTrue(TEXT("No room is requested beyond the final room"),
        !BeyondFinal.bLoaded && !BeyondFinal.bVisible);
    return true;
}

#endif
