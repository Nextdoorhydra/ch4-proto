// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CMGoreMessageTestActor.generated.h"

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
};
