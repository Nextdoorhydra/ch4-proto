#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMRoomStreamingController.generated.h"

class ULevelStreaming;
class UWorld;

USTRUCT(BlueprintType)
struct FCMRoomStreamingEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName RoomId;

    // Persistent Level의 Levels 창에 Blueprint Streaming으로 등록된 룸 맵
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UWorld> Level;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMFinalRoomCommittedSignature,
    FName, RoomId);

UCLASS(Blueprintable)
// 선형 룸 순서에 따라 현재·다음 룸을 표시하고 다다음 룸을 숨김 프리로드
class CHIMERA_API ACMRoomStreamingController : public AActor
{
    GENERATED_BODY()

public:
    ACMRoomStreamingController();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 모든 활성 키메라 몸통이 진입한 룸을 서버 진행 상태로 확정
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Stage|Room Streaming")
    bool CommitRoom(FName RoomId);

    UFUNCTION(BlueprintPure, Category = "Chimera|Stage|Room Streaming")
    FName GetCurrentRoomId() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Stage|Room Streaming")
    int32 GetCurrentRoomIndex() const { return CurrentRoomIndex; }

    // 마지막으로 통과가 확정된 룸의 체크포인트 위치 반환
    bool TryGetActiveCheckpointTransform(FTransform& OutTransform) const;

    // 개발용: Rooms 배열 순서(1부터)로 미도달 룸도 선택한다. 클리어 이벤트는 발생시키지 않는다.
    bool TryCheatSelectCheckpoint(int32 OneBasedCheckpointNumber);

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Stage|Room Streaming")
    FCMFinalRoomCommittedSignature OnFinalRoomCommitted;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(
        FDataValidationContext& Context) const override;
#endif

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Stage|Room Streaming",
        meta = (TitleProperty = "RoomId"))
    TArray<FCMRoomStreamingEntry> Rooms;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Stage|Room Streaming",
        meta = (ClampMin = "0"))
    int32 InitialRoomIndex = 0;

private:
    UFUNCTION()
    void OnRep_CurrentRoomIndex();

    UFUNCTION()
    void HandleAnyRoomLevelLoaded();

    int32 FindRoomIndex(FName RoomId) const;
    ULevelStreaming* ResolveStreamingLevel(const FCMRoomStreamingEntry& Room) const;
    void ApplyStreamingWindow();
    void SetLocalRoomStreamingState(
        int32 RoomIndex,
        bool bShouldBeLoaded,
        bool bShouldBeVisible);
    void ValidateLoadedRoomEntryTriggers() const;

    // 시작 구역에서는 아직 체크포인트를 통과하지 않은 상태로 유지
    int32 ActiveCheckpointRoomIndex = INDEX_NONE;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentRoomIndex)
    int32 CurrentRoomIndex = INDEX_NONE;
};
