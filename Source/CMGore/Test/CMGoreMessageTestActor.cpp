// Fill out your copyright notice in the Description page of Project Settings.


#include "CMGoreMessageTestActor.h"
#include "Tags/CMGoreGameplayTags.h"
#include "Messaging/CMGoreMessages.h"
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
	
	FCMBloodBurstMessage Message;

	Message.Source = this;
	Message.Location = GetActorLocation();
	Message.Direction = GetActorForwardVector();
	Message.Amount = 3.0f;
	Message.BloodDefinitionId = TEXT("Human.Red");

	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		CMGoreGameplayTags::Message::Blood::Burst,
		Message
	);
}

// Called every frame
void ACMGoreMessageTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

