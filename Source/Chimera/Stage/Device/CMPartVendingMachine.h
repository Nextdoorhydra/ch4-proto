#pragma once

#include "CoreMinimal.h"
#include "Combat/CMCombatHitTarget.h"
#include "GameFramework/Actor.h"

#include "CMPartVendingMachine.generated.h"

class ACMPartActorBase;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

/** 팔로 타격할 때 설정된 부착용 파츠 하나를 배출하는 맵 배치 장치다. */
UCLASS(Blueprintable)
class CHIMERA_API ACMPartVendingMachine
    : public AActor
    , public ICMCombatHitTarget
{
    GENERATED_BODY()

public:
    ACMPartVendingMachine();

    virtual bool ReceiveCombatHit_Implementation(
        const FCMCombatHitRequest& Request) override;

protected:
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

    /** 자판기 로컬 좌표 기준 배출 속도 변화량이다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Vending Machine|Output",
        meta = (Units = "cm/s"))
    FVector LocalEjectVelocity = FVector(300.0f, 0.0f, 150.0f);

    UFUNCTION(BlueprintImplementableEvent,
        Category = "Chimera|Part Vending Machine")
    void OnPartDispensed(ACMPartActorBase* DispensedPart);

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCMPartVendingMachineTest;
#endif

    ACMPartActorBase* DispensePart();

    FGuid LastAcceptedAttackId;
};
