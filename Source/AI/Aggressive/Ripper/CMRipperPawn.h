#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"
#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"

#include "CMRipperPawn.generated.h"

class UBoxComponent;
class UCMAIFixedLegActuatorComponent;
class UCMAggressiveMovementCommandComponent;
class UCMAggressiveOmnidirectionalPathComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 큐브 세 개와 다리 세 개를 사용해 회전과 가속을 학습하는 Ripper AI다. */
UCLASS()
class AI_API ACMRipperPawn : public APawn, public ICMAggressiveMovementAgent, public ICMAggressiveLegActuationAgent
{
    GENERATED_BODY()

public:
    ACMRipperPawn();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Ripper|Movement")
    bool ActivateLeg(int32 LegIndex);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Ripper|Movement")
    virtual int32 ActivateLegs(const TArray<int32>& LegIndices) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Ripper|Movement")
    bool StartPathMoveToLocation(FVector WorldGoal, float AcceptanceRadius = 50.0f);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Ripper|Movement")
    void StopPathMove();

    virtual int32 GetLegCount() const override;
    virtual UPrimitiveComponent* GetAggressiveMovementBody() const override;
    virtual FVector GetAggressiveNavigationReferenceLocation() const override;
    virtual void HandleAggressivePathMoveCompleted(ECMAggressivePathMoveResult Result) override;
    UBoxComponent* GetPhysicsRoot() const;
    UCMAIFixedLegActuatorComponent* GetLegActuator() const;
    UCMAggressiveMovementCommandComponent* GetMovementCommand() const;
    UCMAggressiveOmnidirectionalPathComponent* GetPathMovement() const;
    const TArray<TObjectPtr<UBoxComponent>>& GetBodyCollisions() const;
    const TArray<TObjectPtr<UBoxComponent>>& GetLegCollisions() const;
    const TArray<TObjectPtr<UStaticMeshComponent>>& GetBodyMeshes() const;
    const TArray<TObjectPtr<UStaticMeshComponent>>& GetLegMeshes() const;
    virtual void ResetLegActuation() override;

    UPROPERTY(BlueprintAssignable, Category = "Aggressive AI|Ripper|Movement")
    FCMAggressivePathMoveCompletedSignature OnPathMoveCompleted;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Body")
    TObjectPtr<UBoxComponent> PhysicsRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Movement")
    TObjectPtr<UCMAIFixedLegActuatorComponent> LegActuator;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Movement")
    TObjectPtr<UCMAggressiveMovementCommandComponent> MovementCommand;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Movement")
    TObjectPtr<UCMAggressiveOmnidirectionalPathComponent> PathMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Body")
    TArray<TObjectPtr<UBoxComponent>> BodyCollisions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Body")
    TArray<TObjectPtr<UStaticMeshComponent>> BodyMeshes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Body")
    TArray<TObjectPtr<UStaticMeshComponent>> LegMeshes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Body")
    TArray<TObjectPtr<UBoxComponent>> LegCollisions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Body")
    TArray<TObjectPtr<USceneComponent>> LegContactPoints;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Body", meta = (ClampMin = "0.1"))
    float BodyMassKg = 150.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Body", meta = (ClampMin = "0.0"))
    float LinearDamping = 0.75f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Body", meta = (ClampMin = "0.0"))
    float AngularDamping = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Movement", meta = (ClampMin = "0.0"))
    float MaxPlanarSpeed = 500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Body")
    bool bUseGravity = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Movement")
    FCMAIFixedLegActuationSettings LegActuationSettings;

private:
    void BalanceLegYawImpulse(int32 LegIndex, const FCMAIFixedLegActuationResult& Result);
    void AddBodyCube(const TCHAR* Name, const FVector& RelativeLocation, UStaticMesh* CubeMesh);
    void AddLeg(const TCHAR* Name, const FVector& RelativeLocation, UStaticMesh* CubeMesh);
    void ApplyBodySettings();
};
