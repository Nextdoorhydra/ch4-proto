#pragma once

#include "CoreMinimal.h"
#include "Combat/CMCombatHitTarget.h"
#include "GameFramework/Actor.h"

#include "CMPartVendingMachine.generated.h"

class ACMPartActorBase;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTexture2D;
class UWidgetComponent;

/** 팔로 타격할 때 설정된 부착용 파츠 하나를 배출하는 맵 배치 장치다. */
UCLASS(Blueprintable)
class CHIMERA_API ACMPartVendingMachine
    : public AActor
    , public ICMCombatHitTarget
{
    GENERATED_BODY()

public:
    ACMPartVendingMachine();

    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual bool ReceiveCombatHit_Implementation(
        const FCMCombatHitRequest& Request) override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine")
    TObjectPtr<UStaticMeshComponent> MachineMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine")
    TObjectPtr<UBoxComponent> HitVolume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine")
    TObjectPtr<USceneComponent> DispensePoint;

    /** 자판기 종류 아이콘과 남은 횟수를 월드에 표시한다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Display")
    TObjectPtr<UWidgetComponent> DisplayWidget;

    /** 팔, 다리, 머리 자판기를 구분할 UI 아이콘이다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Display")
    TObjectPtr<UTexture2D> PartIcon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Display",
        meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
    float DisplayFloatAmplitude = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Display",
        meta = (ClampMin = "0.0", UIMin = "0.0"))
    float DisplayFloatSpeed = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Hit Shake",
        meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
    float HitShakeDuration = 0.3f;

    /** 자판기 로컬 X(앞뒤), Y(좌우), Z 방향의 최대 흔들림 거리다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Hit Shake",
        meta = (Units = "cm"))
    FVector HitShakeDistance = FVector(3.0f, 3.0f, 0.5f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Hit Shake",
        meta = (ClampMin = "0.0", UIMin = "0.0", Units = "Hz"))
    float HitShakeFrequency = 12.0f;

    /** BP 자식마다 팔, 다리 또는 머리 Part 클래스를 하나 지정한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Output")
    TSubclassOf<ACMPartActorBase> PartClass;

    /** 비어 있으면 PartClass 기본값을 그대로 사용한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Output")
    FName PartRowName = NAME_None;

    /** 비어 있으면 PartClass 기본값을 그대로 사용한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Output")
    FName TierRowName = NAME_None;

    /** DispensePoint 로컬 좌표 기준 배출 속도 변화량이다. +X가 앞 방향이다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Output",
        meta = (Units = "cm/s"))
    FVector LocalEjectVelocity = FVector(500.0f, 0.0f, 150.0f);

    /** 배출 가능한 남은 횟수다. 성공적으로 배출됐을 때만 감소한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_RemainingUses,
        Category = "Chimera|Part Vending Machine|Output",
        meta = (ClampMin = "0", UIMin = "0"))
    int32 RemainingUses = 99;

    UFUNCTION(BlueprintPure,
        Category = "Chimera|Part Vending Machine|Output")
    int32 GetRemainingUses() const { return RemainingUses; }

    UFUNCTION(BlueprintImplementableEvent,
        Category = "Chimera|Part Vending Machine")
    void OnPartDispensed(ACMPartActorBase* DispensedPart);

    UFUNCTION(BlueprintImplementableEvent,
        Category = "Chimera|Part Vending Machine")
    void OnRemainingUsesChanged(int32 NewRemainingUses);

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCMPartVendingMachineTest;
#endif

    ACMPartActorBase* DispensePart();

    UFUNCTION()
    void OnRep_RemainingUses();

    void ApplyDisplayPresentation();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayHitShake();

    void UpdateHitShake(float DeltaSeconds);

    FGuid LastAcceptedAttackId;
    FVector DisplayBaseLocation = FVector::ZeroVector;
    FVector MachineMeshBaseLocation = FVector::ZeroVector;
    float HitShakeElapsed = 0.0f;
};
