#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"

#include "CMTargetScannerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMScannerTargetSignature,
    AActor*, TargetActor);

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 서버에서 거리, 시야각, 벽 가림을 검사해 가장 가까운 공격 대상 선택
class CHIMERA_API UCMTargetScannerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMTargetScannerComponent();

    // 자동 탐색을 시작하고 이미 실행 중이면 유지
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|Scanner")
    void StartScanning();

    // 자동 탐색을 중지하고 현재 대상 해제
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|Scanner")
    void StopScanning();

    // 현재 프레임 기준으로 대상 후보를 다시 검사
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|Scanner")
    void ScanNow();

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle|Scanner")
    AActor* GetCurrentTarget() const { return CurrentTarget; }

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Obstacle|Scanner")
    FCMScannerTargetSignature OnTargetAcquired;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Obstacle|Scanner")
    FCMScannerTargetSignature OnTargetLost;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Scanner")
    FComponentReference ScanOrigin;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Scanner",
        meta = (ClampMin = "1.0"))
    float DetectionDistance = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Scanner",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float DetectionHalfAngle = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Scanner",
        meta = (ClampMin = "0.02"))
    float ScanInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Scanner")
    TArray<TEnumAsByte<EObjectTypeQuery>> TargetObjectTypes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Scanner")
    TSubclassOf<AActor> TargetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Scanner")
    FName RequiredActorTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Scanner")
    TEnumAsByte<ECollisionChannel> LineOfSightTraceChannel = ECC_Visibility;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    USceneComponent* ResolveScanOrigin() const;
    bool HasLineOfSight(
        const FVector& OriginLocation,
        const AActor* Candidate) const;
    void SetCurrentTarget(AActor* NewTarget);

    UPROPERTY(Transient)
    TObjectPtr<AActor> CurrentTarget;

    FTimerHandle ScanTimerHandle;
};
