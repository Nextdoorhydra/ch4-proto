#pragma once

#include "CoreMinimal.h"
#include "Aggressive/Common/Core/CMAggressivePawnBase.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"
#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"

#include "CMTetraPawn.generated.h"

class UBoxComponent;
class UCMAggressiveBehaviorComponent;
class UCMAggressiveAccelerationMovementComponent;
class UCMAggressiveMovementCommandComponent;
class UCMAggressiveOmnidirectionalPathComponent;
class UCMAggressiveSightComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 큐브 몸통 하나와 시각 다리 네 개로 학습된 가속 이동을 수행하는 Tetra AI다. */
UCLASS()
class AI_API ACMTetraPawn : public ACMAggressivePawnBase, public ICMAggressiveMovementAgent
{
    GENERATED_BODY()

public:
    ACMTetraPawn();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Tetra|Movement")
    bool SetManualMoveDirection(ECMAggressiveMoveDirection Direction);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Tetra|Movement")
    void StopManualMovement();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Tetra|Path Movement")
    bool StartPathMoveToLocation(FVector WorldGoal, float AcceptanceRadius = 50.0f);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Tetra|Path Movement")
    void StopPathMove();

    int32 GetBodyCubeCount() const;
    int32 GetLegCount() const;
    UBoxComponent* GetPhysicsRoot() const;
    UStaticMeshComponent* GetBodyMesh() const;
    USceneComponent* GetVisualBodyRoot() const;
    UCMAggressiveAccelerationMovementComponent* GetAccelerationMovement() const;
    UCMAggressiveMovementCommandComponent* GetMovementCommand() const;
    UCMAggressiveOmnidirectionalPathComponent* GetPathMovement() const;
    const TArray<TObjectPtr<UStaticMeshComponent>>& GetLegMeshes() const;
    virtual UPrimitiveComponent* GetAggressiveMovementBody() const override;
    virtual FVector GetAggressiveNavigationReferenceLocation() const override;
    virtual void HandleAggressivePathMoveCompleted(ECMAggressivePathMoveResult Result) override;
    virtual void StopAggressiveMovementForReaction() override;

    UPROPERTY(BlueprintAssignable, Category = "Aggressive AI|Tetra|Path Movement")
    FCMAggressivePathMoveCompletedSignature OnPathMoveCompleted;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Behavior")
    TObjectPtr<UCMAggressiveBehaviorComponent> Behavior;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Body")
    TObjectPtr<UBoxComponent> PhysicsRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Body")
    TObjectPtr<USceneComponent> VisualBodyRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Body")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Body")
    TArray<TObjectPtr<UStaticMeshComponent>> LegMeshes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Movement")
    TObjectPtr<UCMAggressiveAccelerationMovementComponent> AccelerationMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Movement")
    TObjectPtr<UCMAggressiveMovementCommandComponent> MovementCommand;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Movement")
    TObjectPtr<UCMAggressiveOmnidirectionalPathComponent> PathMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Sight")
    TObjectPtr<UCMAggressiveSightComponent> Sight;

private:
    void AddVisualLeg(const TCHAR* Name, const FVector& RelativeContactLocation, UStaticMesh* CubeMesh);
    void ApplyPlanarPhysicsSettings();
};
