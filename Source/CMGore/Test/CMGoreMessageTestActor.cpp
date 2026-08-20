// Fill out your copyright notice in the Description page of Project Settings.


#include "CMGoreMessageTestActor.h"
#include "CMGoreMessageTestActor.h"

#include "Tags/CMGoreGameplayTags.h"
#include "Messaging/CMGoreMessages.h"
#include "Runtime/Surface/CMBloodSurfaceSubsystem.h"

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
	Message.Direction = FVector::DownVector;
	Message.Amount = 3.0f;
	Message.BloodDefinitionId = TEXT("Human.Red");

	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		CMGoreGameplayTags::Message::Blood::Burst,
		Message
	);

	// -----------------------------------------------------------------
	// Phase 4 Test : Blood Mark Registry
	// -----------------------------------------------------------------

	UCMBloodSurfaceSubsystem* SurfaceSubsystem =
		GetWorld()->GetSubsystem<UCMBloodSurfaceSubsystem>();

	check(SurfaceSubsystem);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Blood Marks: %d"),
		SurfaceSubsystem->GetActiveBloodMarkCount()
	);
}

// Called every frame
void ACMGoreMessageTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

