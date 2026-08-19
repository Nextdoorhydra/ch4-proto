// Fill out your copyright notice in the Description page of Project Settings.


#include "CMGoreMessageTestActor.h"
#include "CMGoreGameplayTags.h"
#include "CMGoreMessages.h"
#include "GameplayMessageRuntime/Public/GameFramework/GameplayMessageSubsystem.h"



// Sets default values
ACMGoreMessageTestActor::ACMGoreMessageTestActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ACMGoreMessageTestActor::BeginPlay()
{
	Super::BeginPlay();
	
	FCMBloodImpactMessage Message;

	Message.Source = this;

	Message.Location = GetActorLocation();

	Message.SurfaceNormal = FVector::UpVector;

	Message.Direction = FVector::ForwardVector;

	Message.Intensity = 2.5f;

	Message.BloodDefinitionId = TEXT("TestBlood");

	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		CMGoreGameplayTags::Message::Blood::Impact,
		Message
	);
}

// Called every frame
void ACMGoreMessageTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

