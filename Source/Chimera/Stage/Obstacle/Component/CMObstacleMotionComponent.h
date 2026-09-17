#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMObstacleMotionComponent.generated.h"

UENUM(BlueprintType)
enum class ECMObstacleMotionType : uint8
{
    None,
    Rotation,
    RotationToAngle,
    Linear,
    PingPong
};

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 회전 칼날과 왕복 장애물의 이동 상태 및 초기 위치 관리
class CHIMERA_API UCMObstacleMotionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMObstacleMotionComponent();

    // 서버에서 설정된 이동 방식에 따라 소유 장애물 Transform 갱신
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Chimera|Obstacle|Motion")
    void StartMotion();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Obstacle|Motion")
    void StopMotion();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Obstacle|Motion")
    void ReverseMotion();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Obstacle|Motion")
    void ResetMotion();

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle|Motion")
    bool IsMotionRunning() const { return bMotionRunning; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Motion")
    ECMObstacleMotionType MotionType = ECMObstacleMotionType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Motion")
    FVector MotionAxis = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Motion")
    float Speed = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Motion",
        meta = (ClampMin = "0.0", EditCondition = "MotionType == ECMObstacleMotionType::Linear || MotionType == ECMObstacleMotionType::PingPong", EditConditionHides))
    float MoveDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Motion",
        meta = (ClampMin = "0.0", EditCondition = "MotionType == ECMObstacleMotionType::RotationToAngle", EditConditionHides))
    float RotationAngle = 90.0f;

protected:
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Obstacle|Motion")
    void OnMotionStateChanged(bool bIsRunning, float NewDirectionSign);

private:
    FTransform InitialTransform;
    bool bMotionRunning = false;
    float DirectionSign = 1.0f;
    float TravelledDistance = 0.0f;
    float CurrentRotationAngle = 0.0f;

    FVector GetWorldMotionAxis() const;
    void TickRotation(float DeltaTime);
    void TickRotationToAngle(float DeltaTime);
    void TickTranslation(float DeltaTime, bool bShouldPingPong);
};
