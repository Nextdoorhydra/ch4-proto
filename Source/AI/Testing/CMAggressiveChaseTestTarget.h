#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMAggressiveChaseTestTarget.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

namespace CMAggressiveChaseTest
{
    /** NavMesh 후보가 순간이동 거리와 높이 범위 안에 있는지 반환한다. */
    AI_API bool IsTeleportCandidateWithinBounds(const FVector& OriginNavLocation, const FVector& CandidateNavLocation, float MinimumDistance, float MaximumDistance, float MaximumHeightDifference);
}

/** 공격적 AI와 접촉하면 가까운 안전한 NavMesh 위치로 순간이동하는 추격 테스트 액터다. */
UCLASS(BlueprintType)
class AI_API ACMAggressiveChaseTestTarget : public AActor
{
    GENERATED_BODY()

public:
    ACMAggressiveChaseTestTarget();

    virtual void BeginPlay() override;

    /** 현재 위치에서 가까운 안전한 NavMesh 위치로 순간이동한다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Test")
    bool TeleportToRandomReachableLocation();

    UBoxComponent* GetContactTrigger() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Test")
    float GetTeleportRadius() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Test")
    float GetMinimumTeleportDistance() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Test")
    float GetMaximumHeightDifference() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Test")
    TObjectPtr<UBoxComponent> ContactTrigger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Test")
    TObjectPtr<UStaticMeshComponent> TargetMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Test", meta = (ClampMin = "0.0"))
    float TeleportRadius = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Test", meta = (ClampMin = "0.0"))
    float MinimumTeleportDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Test", meta = (ClampMin = "0.0"))
    float MaximumHeightDifference = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Test")
    FName NavigationAgentName = TEXT("RipperAI");

private:
    UFUNCTION()
    void HandleContact(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    bool TeleportToRandomReachableLocationUsingAgent(FName InNavigationAgentName);
    bool IsTeleportLocationClear(const FVector& CandidateLocation) const;

    bool bTeleporting = false;
};
