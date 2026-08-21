// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CMGoreMessageTestActor.generated.h"

class ACMBloodDecalActor;
class UCMBloodPoolSourceComponent;
class UMaterialInstanceDynamic;

UCLASS()
class CMGORE_API ACMGoreMessageTestActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACMGoreMessageTestActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	void RunPhase4SmokeTest();
	void FinishPhase4SmokeTest();
	void RecordPhase4SmokeTestResult(bool bCondition, const TCHAR* FailureMessage);

	void RunPhase5SmokeTest();
	void CheckPhase5GrowthAndStop();
	void CheckPhase5StopAndReuse();
	void FinishPhase5SmokeTest();
	void RecordPhase5SmokeTestResult(bool bCondition, const TCHAR* FailureMessage);

private:
	bool bPhase4SmokeTestPassed = true;
	bool bPhase5SmokeTestPassed = true;
	float Phase5StoppedProgress = 0.0f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCMBloodPoolSourceComponent> BloodPoolSourceComponent;

	UPROPERTY(Transient)
	TObjectPtr<ACMBloodDecalActor> Phase5FirstPresentationActor;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Phase5FirstMID;
};
