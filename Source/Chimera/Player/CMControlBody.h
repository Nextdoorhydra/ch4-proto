#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Player/CMControlTypes.h"

#include "CMControlBody.generated.h"

class ACMChimera;
class AActor;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraControlSlotsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraControlPlayerStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FChimeraControlInputChanged,
    int32,
    SlotIndex,
    bool,
    bPressed
);

/**
 * 플레이어 한 명이 소유하고 Possess하는 논리 Pawn이다.
 *
 * 실제 물리 몸통과 공용 능력치는 Shared Chimera(ACMChimera)가 담당한다.
 * ControlBody는 플레이어별 네트워크 소유권의 기준점만 제공하며,
 * 카메라·충돌·물리 시뮬레이션은 소유하지 않는다.
 */
UCLASS()
class CHIMERA_API ACMControlBody : public APawn
{
    GENERATED_BODY()

public:
    ACMControlBody();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    /** GameState에 복제된 현재 공용 키메라를 반환한다. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Control Body")
    ACMChimera* GetSharedChimera() const;

    /** PlayerController는 키 입력을 슬롯 번호로만 전달한다. */
    void SetControlSlotPressed(
        int32 SlotIndex,
        bool bPressed,
        bool bReverseMovement
    );

    /** Pickup/UI calls this on the owning client after selecting a Part. */
    UFUNCTION(BlueprintCallable, Category = "Chimera|Parts")
    void RequestAttachPartToControlSlot(
        int32 SlotIndex,
        AActor* PartActor
    );

    /** Used by Shift+Q/W/E/R on the owning client. */
    UFUNCTION(BlueprintCallable, Category = "Chimera|Parts")
    void RequestDetachPartFromControlSlot(int32 SlotIndex);

    /** Used by Ctrl+Q/W/E/R to consume a Part and fully heal its body. */
    UFUNCTION(BlueprintCallable, Category = "Chimera|Parts")
    void RequestConsumePartFromControlSlot(int32 SlotIndex);

    /** 서버의 할당 정책이 계산한 Q/W/E/R 슬롯을 이 ControlBody에 저장한다. */
    void SetControlSlots(
        const TArray<FCMPartSlotAddress>& NewControlSlots
    );

    UFUNCTION(BlueprintPure, Category = "Chimera|Control Body")
    FCMPartSlotAddress GetPartSlotAddressForControlSlot(
        int32 SlotIndex
    ) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Control Body")
    int32 GetAssignedControlCount() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Control Body")
    int32 GetEnabledControlCount() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Control Body")
    bool IsControlSlotEnabled(int32 SlotIndex) const;

    const TArray<FCMPartSlotAddress>& GetControlSlots() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Control Body")
    bool OwnsSegment(int32 SegmentIndex) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Control Body")
    bool IsControlInputEnabled() const;

    /** 혼란이 적용된 물리 키가 실제로 가리키는 Q/W/E/R 인덱스. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Control Status")
    int32 ResolveControlInputSlot(int32 PhysicalSlotIndex) const;

    /** 혼란과 착란을 모두 반영해 물리 Q/W/E/R이 실제 제어하는 파츠 슬롯을 반환한다. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Control Status")
    FCMPartSlotAddress GetEffectivePartSlotAddressForControlInput(
        int32 PhysicalSlotIndex
    ) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Control Status")
    bool IsConfused() const { return !ConfusionSlotRemap.IsEmpty(); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Control Status")
    bool IsDelirious() const { return !DeliriumControlSlots.IsEmpty(); }

    void ApplyConfusion(float Duration, UObject* Source);
    void RemoveConfusion(UObject* Source = nullptr);
    bool ApplyDelirium(
        ACMControlBody& OtherControlBody, float Duration, UObject* Source);
    void RemoveDelirium(UObject* Source = nullptr);

    /** Called by the authoritative Chimera when one Segment is destroyed. */
    void HandleSegmentDestroyed(int32 DestroyedSegmentIndex);

    // 체크포인트 복귀 시 사망으로 잠긴 Q/W/E/R 입력 복구
    void RestoreControlsAfterRespawn();

    void ClearPressedControlSlots();

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Control Body")
    FChimeraControlSlotsChanged OnControlSlotsChanged;

    /** Local input feedback for HUDs; gameplay remains server-authoritative. */
    UPROPERTY(BlueprintAssignable, Category = "Chimera|Control Body")
    FChimeraControlInputChanged OnControlInputChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Control Body")
    FChimeraControlPlayerStateChanged OnPlayerStateChanged;

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnRep_Controller() override;
    virtual void OnRep_PlayerState() override;

    UFUNCTION()
    void OnRep_ControlSlots();

    UFUNCTION()
    void OnRep_ControlState();

    UFUNCTION(Server, Reliable)
    void ServerSetControlSlotPressed(
        int32 SlotIndex,
        bool bPressed,
        bool bReverseMovement
    );

    UFUNCTION(Server, Reliable)
    void ServerRequestAttachPartToControlSlot(
        int32 SlotIndex,
        AActor* PartActor
    );

    UFUNCTION(Server, Reliable)
    void ServerRequestDetachPartFromControlSlot(int32 SlotIndex);

    UFUNCTION(Server, Reliable)
    void ServerRequestConsumePartFromControlSlot(int32 SlotIndex);

    /**
     * ControlBody는 위치 기반 게임 판정을 하지 않는 논리 Pawn이다.
     * SceneRoot는 Pawn에 필요한 최소 루트일 뿐, Shared Chimera를 따라 움직이지 않는다.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Body")
    TObjectPtr<USceneComponent> SceneRoot;

    /** 인덱스 0~3이 각각 Q/W/E/R이며, 서버가 배정하고 모두에게 복제한다. */
    UPROPERTY(ReplicatedUsing = OnRep_ControlSlots, BlueprintReadOnly,
        Category = "Chimera|Control Body",
        meta = (AllowPrivateAccess = "true"))
    TArray<FCMPartSlotAddress> ControlSlots;

    UPROPERTY(ReplicatedUsing = OnRep_ControlState, BlueprintReadOnly,
        Category = "Chimera|Control Body",
        meta = (AllowPrivateAccess = "true"))
    bool bControlInputEnabled = true;

    /** Q/W/E/R bits disabled because their physical Segment is dead. */
    UPROPERTY(ReplicatedUsing = OnRep_ControlState, BlueprintReadOnly,
        Category = "Chimera|Control Body",
        meta = (AllowPrivateAccess = "true"))
    uint8 DisabledControlSlotMask = 0;

    /** 비어 있으면 identity. 값은 물리 Q/W/E/R이 가리키는 슬롯 인덱스다. */
    UPROPERTY(ReplicatedUsing = OnRep_ControlSlots, BlueprintReadOnly,
        Category = "Chimera|Control Status",
        meta = (AllowPrivateAccess = "true"))
    TArray<int32> ConfusionSlotRemap;

    /** 비어 있으면 본인 슬롯, 값이 있으면 착란 상대의 슬롯을 제어한다. */
    UPROPERTY(ReplicatedUsing = OnRep_ControlSlots, BlueprintReadOnly,
        Category = "Chimera|Control Status",
        meta = (AllowPrivateAccess = "true"))
    TArray<FCMPartSlotAddress> DeliriumControlSlots;

    /** Temporary slot-based validation until the Head pickup rule exists. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Parts",
        meta = (ClampMin = "0.0"))
    float MaximumPartAttachDistance = 250.0f;

private:
    /** 눌렀을 때 배정됐던 파츠를 기억해 Release와 재할당을 안전하게 처리한다. */
    FCMPartSlotAddress PressedPartSlots[CMControl::MaxKeysPerPlayer];

    TMap<TWeakObjectPtr<UObject>, FTimerHandle> ConfusionSources;
    TMap<TWeakObjectPtr<UObject>, FTimerHandle> DeliriumSources;
    TWeakObjectPtr<ACMControlBody> DeliriumPartner;

    FCMPartSlotAddress GetEffectivePartSlotAddress(int32 SlotIndex) const;
    void RemoveConfusionSource(TWeakObjectPtr<UObject> Source);
    void RemoveDeliriumSource(TWeakObjectPtr<UObject> Source);
    void RemoveDeliriumSourceLocal(TWeakObjectPtr<UObject> Source);

    uint8 LocalPressedControlSlotMask = 0;
};
