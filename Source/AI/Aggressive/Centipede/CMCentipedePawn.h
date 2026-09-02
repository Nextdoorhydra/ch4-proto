#pragma once

#include "CoreMinimal.h"
#include "Aggressive/Common/Core/CMAggressivePawnBase.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"
#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"

#include "CMCentipedePawn.generated.h"

class UBoxComponent;
class UCMAggressiveBehaviorComponent;
class UCMAIFixedLegActuatorComponent;
class UCMAggressiveMovementCommandComponent;
class UCMAggressiveOmnidirectionalPathComponent;
class UCMAggressiveSightComponent;
class UPhysicsConstraintComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 180x120x120cm 몸통 네 마디와 여덟 다리로 움직이는 관절형 Centipede AI다. */
UCLASS(BlueprintType)
class AI_API ACMCentipedePawn : public ACMAggressivePawnBase, public ICMAggressiveMovementAgent, public ICMAggressiveLegActuationAgent
{
    GENERATED_BODY()

public:
    ACMCentipedePawn();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Centipede|Movement")
    bool ActivateLeg(int32 LegIndex);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Centipede|Movement")
    virtual int32 ActivateLegs(const TArray<int32>& LegIndices) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Centipede|Movement")
    bool StartPathMoveToLocation(FVector WorldGoal, float AcceptanceRadius = 100.0f);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Centipede|Movement")
    void StopPathMove();

    virtual int32 GetLegCount() const override;
    virtual UPrimitiveComponent* GetAggressiveMovementBody() const override;
    virtual FVector GetAggressiveNavigationReferenceLocation() const override;
    virtual void PrepareAggressivePathMove(FVector WorldGoal) override;
    virtual void HandleAggressivePathMoveCompleted(ECMAggressivePathMoveResult Result) override;
    virtual void StopAggressiveMovementForReaction() override;
    virtual void ResetLegActuation() override;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Body")
    int32 GetBodySegmentCount() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Body")
    int32 GetJointCount() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Body")
    float GetSegmentCenterSpacing() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Body")
    float GetHorizontalBendLimitDegrees() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Movement")
    float GetMaxPlanarSpeed() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Movement")
    bool IsTailLeading() const
    {
        return bTailLeading;
    }

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Movement")
    bool IsAligningLeadingEnd() const
    {
        return bAligningLeadingEnd;
    }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Centipede|Movement")
    void SetTailLeading(bool bInTailLeading);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Centipede|Movement")
    void SwapLeadingEnd();

    UBoxComponent* GetHeadBody() const;
    UBoxComponent* GetLeadingBody() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Body")
    FVector GetLeadingTipLocation() const;

    UCMAggressiveMovementCommandComponent* GetMovementCommand() const;
    UCMAggressiveOmnidirectionalPathComponent* GetPathMovement() const;
    const TArray<TObjectPtr<UBoxComponent>>& GetBodySegments() const;
    const TArray<TObjectPtr<UPhysicsConstraintComponent>>& GetSegmentConstraints() const;
    const TArray<TObjectPtr<UStaticMeshComponent>>& GetBodyMeshes() const;
    const TArray<TObjectPtr<UStaticMeshComponent>>& GetLegMeshes() const;

    /** 경로 진행 전에 선두를 현재 경유지 방향으로 회전시키고, 정렬 중이면 true를 반환한다. */
    bool UpdateLeadingEndAlignment();

    /** 현재 경로와 머리 이동 이력으로 세 관절의 목표각을 갱신한다. */
    void RefreshJointTargets();

    /** 현재각, 목표각, 상대 각속도를 고정된 세 관절 순서로 반환한다. */
    void GetJointState(TArray<float>& OutCurrentAnglesDegrees, TArray<float>& OutTargetAnglesDegrees, TArray<float>& OutAngularVelocitiesRadians) const;

    float GetMeanNormalizedJointError() const;
    float GetMinimumSegmentUprightDot() const;

    /** 학습 에피소드에서 매끄러운 S자 목표를 절차적으로 섞는다. 0이면 비활성화한다. */
    void SetTrainingCurveProfile(int32 ProfileIndex);

    /** 머리 기준 자세로 네 물리 마디를 직선 배치하고 속도와 이동 이력을 초기화한다. */
    void ResetArticulatedBody(const FTransform& HeadTransform);
    void StopArticulatedBodyMotion();

    UPROPERTY(BlueprintAssignable, Category = "Aggressive AI|Centipede|Movement")
    FCMAggressivePathMoveCompletedSignature OnPathMoveCompleted;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Behavior")
    TObjectPtr<UCMAggressiveBehaviorComponent> Behavior;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body")
    TObjectPtr<UBoxComponent> HeadBody;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Movement")
    TObjectPtr<UCMAIFixedLegActuatorComponent> LegActuator;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Movement")
    TObjectPtr<UCMAggressiveMovementCommandComponent> MovementCommand;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Movement")
    TObjectPtr<UCMAggressiveOmnidirectionalPathComponent> PathMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Sight")
    TObjectPtr<UCMAggressiveSightComponent> HeadSight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Sight")
    TObjectPtr<UCMAggressiveSightComponent> TailSight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body")
    TArray<TObjectPtr<UBoxComponent>> BodySegments;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body")
    TArray<TObjectPtr<UPhysicsConstraintComponent>> SegmentConstraints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body")
    TArray<TObjectPtr<UStaticMeshComponent>> BodyMeshes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body")
    TArray<TObjectPtr<UStaticMeshComponent>> LegMeshes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body")
    TArray<TObjectPtr<UBoxComponent>> LegCollisions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body")
    TArray<TObjectPtr<USceneComponent>> LegContactPoints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body")
    TArray<TObjectPtr<UBoxComponent>> LegBodies;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body", meta = (ClampMin = "1.0"))
    float BodyLength = 180.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body", meta = (ClampMin = "1.0"))
    float BodyWidth = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body", meta = (ClampMin = "1.0"))
    float BodyHeight = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body", meta = (ClampMin = "1.0"))
    float SegmentCenterSpacing = 220.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float HorizontalBendLimitDegrees = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float OperatingBendLimitDegrees = 90.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body", meta = (ClampMin = "0.0"))
    float TargetJointAngleRateDegreesPerSecond = 90.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body", meta = (ClampMin = "0.1"))
    float BodyMassPerSegmentKg = 140.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body", meta = (ClampMin = "0.0"))
    float LinearDamping = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body", meta = (ClampMin = "0.0"))
    float AngularDamping = 7.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body", meta = (ClampMin = "0.0"))
    float ConstraintVelocityDamping = 9.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Movement", meta = (ClampMin = "0.0"))
    float MaxPlanarSpeed = 180.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Movement|Alignment", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float LeadingAlignmentStartAngleDegrees = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Movement|Alignment", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float LeadingAlignmentStopAngleDegrees = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Movement|Alignment", meta = (ClampMin = "0.0"))
    float LeadingAlignmentAngularSpeedDegreesPerSecond = 180.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Movement|Alignment", meta = (ClampMin = "0.0"))
    float LeadingAlignmentAngularSpeedGain = 6.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Movement|Alignment", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AlignmentMovementOutputScale = 0.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Movement|Alignment", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TrailingAlignmentAngularSpeedScale = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Body")
    bool bUseGravity = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Movement")
    FCMAIFixedLegActuationSettings LegActuationSettings;

private:
    UBoxComponent* AddBodySegment(int32 SegmentIndex, UStaticMesh* CubeMesh);
    void AddLeg(int32 SegmentIndex, bool bLeftLeg, UStaticMesh* CubeMesh);
    void ApplyBodySettings();
    void CreateSegmentConstraints();
    void DestroySegmentConstraints();
    void ResetHeadTrail();
    void UpdateHeadTrail();
    FVector SampleHeadTrail(float DistanceBehindHead) const;

    TArray<float> TargetJointAnglesDegrees;
    TArray<FVector> HeadTrailPoints;
    double LastTargetUpdateTime = 0.0;
    int32 TrainingCurveProfile = 0;
    bool bTailLeading = false;
    bool bAligningLeadingEnd = false;
};
