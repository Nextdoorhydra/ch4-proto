// Fill out your copyright notice in the Description page of Project Settings.


#include "CMGoreMessageTestActor.h"

#include "Components/DecalComponent.h"
#include "Data/CMBloodDefinition.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tags/CMGoreGameplayTags.h"
#include "Messaging/CMGoreMessages.h"
#include "Runtime/Surface/Presentation/CMBloodDecalActor.h"
#include "Runtime/Surface/CMBloodSurfaceSubsystem.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

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

	if (FParse::Param(
		FCommandLine::Get(),
		TEXT("CMGorePhase4SmokeTest")))
	{
		RunPhase4SmokeTest();
	}
}

// Called every frame
void ACMGoreMessageTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


void ACMGoreMessageTestActor::RunPhase4SmokeTest()
{
	UCMBloodSurfaceSubsystem* SurfaceSubsystem =
		GetWorld()->GetSubsystem<UCMBloodSurfaceSubsystem>();

	UCMBloodDefinition* Definition = LoadObject<UCMBloodDefinition>(
		nullptr,
		TEXT("/Game/CMGore/Definitions/DA_CMBlood_HumanRed.DA_CMBlood_HumanRed"));

	RecordPhase4SmokeTestResult(
		IsValid(SurfaceSubsystem),
		TEXT("Blood Surface Subsystem is unavailable."));

	RecordPhase4SmokeTestResult(
		IsValid(Definition) && IsValid(Definition->Surface.DecalMaterial),
		TEXT("Human.Red test definition or decal material is unavailable."));

	if (!IsValid(SurfaceSubsystem) ||
		!IsValid(Definition) ||
		!IsValid(Definition->Surface.DecalMaterial))
	{
		FinishPhase4SmokeTest();
		return;
	}

	SurfaceSubsystem->ClearBloodMarks();

	FCMBloodSurfaceBurstRequest Request;
	Request.Origin = GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
	Request.Direction = FVector::DownVector;
	Request.SampleCount = 1;
	Request.TraceDistance = 5000.0f;
	Request.ConeHalfAngleDegrees = 0.0f;
	Request.DecalMaterial = Definition->Surface.DecalMaterial;
	Request.DecalActorClass = Definition->Surface.DecalActorClass;
	Request.DecalExtentRange = FVector2D(24.0f, 24.0f);
	Request.DecalDepth = 8.0f;
	Request.LifetimeSeconds = 0.0f;
	Request.FadeDurationSeconds = 0.0f;
	Request.RandomSeed = 41001;
	Request.IgnoredActor = this;

	auto CountPresentationActors = [this]()
	{
		int32 Count = 0;
		for (TActorIterator<ACMBloodDecalActor> It(GetWorld()); It; ++It)
		{
			++Count;
		}
		return Count;
	};

	const int32 InitialActorCount = CountPresentationActors();

	const TArray<FCMBloodResidueHandle> FirstHandles =
		SurfaceSubsystem->SpawnSurfaceBurst(Request);

	RecordPhase4SmokeTestResult(
		FirstHandles.Num() == 1,
		TEXT("First surface mark was not created."));

	const int32 ActorCountAfterFirstSpawn = CountPresentationActors();

	RecordPhase4SmokeTestResult(
		ActorCountAfterFirstSpawn >= InitialActorCount &&
		ActorCountAfterFirstSpawn <= InitialActorCount + 1,
		TEXT("First surface mark created an unexpected number of presentation actors."));

	TWeakObjectPtr<ACMBloodDecalActor> FirstPresentationActor;
	for (TActorIterator<ACMBloodDecalActor> It(GetWorld()); It; ++It)
	{
		if (It->IsPresentationActive())
		{
			FirstPresentationActor = *It;
			break;
		}
	}

	if (!FirstHandles.IsEmpty())
	{
		SurfaceSubsystem->RemoveBloodMark(FirstHandles[0]);
	}

	Request.RandomSeed = 41002;
	const TArray<FCMBloodResidueHandle> ReusedHandles =
		SurfaceSubsystem->SpawnSurfaceBurst(Request);

	RecordPhase4SmokeTestResult(
		ReusedHandles.Num() == 1,
		TEXT("Reused surface mark was not created."));

	RecordPhase4SmokeTestResult(
		CountPresentationActors() == ActorCountAfterFirstSpawn,
		TEXT("Released presentation actor was not reused."));

	TWeakObjectPtr<ACMBloodDecalActor> ReusedPresentationActor;
	for (TActorIterator<ACMBloodDecalActor> It(GetWorld()); It; ++It)
	{
		if (It->IsPresentationActive())
		{
			ReusedPresentationActor = *It;
			break;
		}
	}

	RecordPhase4SmokeTestResult(
		FirstPresentationActor.IsValid() &&
		ReusedPresentationActor == FirstPresentationActor,
		TEXT("Remove and reacquire did not return the same presentation actor."));

	Request.RandomSeed = 41003;
	const TArray<FCMBloodResidueHandle> ConcurrentHandles =
		SurfaceSubsystem->SpawnSurfaceBurst(Request);

	RecordPhase4SmokeTestResult(
		ConcurrentHandles.Num() == 1,
		TEXT("Concurrent surface mark was not created."));

	int32 ActiveActorCount = 0;
	TArray<UMaterialInstanceDynamic*> ActiveMIDs;

	for (TActorIterator<ACMBloodDecalActor> It(GetWorld()); It; ++It)
	{
		ACMBloodDecalActor* PresentationActor = *It;

		if (!PresentationActor->IsPresentationActive())
		{
			continue;
		}

		++ActiveActorCount;
		ActiveMIDs.Add(PresentationActor->GetDynamicMaterial());

		RecordPhase4SmokeTestResult(
			PresentationActor->GetDecalComponent() &&
			PresentationActor->GetDecalComponent()->IsRegistered(),
			TEXT("Active presentation decal is not registered."));
	}

	RecordPhase4SmokeTestResult(
		ActiveActorCount == 2,
		TEXT("Expected two active presentation actors."));

	RecordPhase4SmokeTestResult(
		ActiveMIDs.Num() == 2 &&
		IsValid(ActiveMIDs[0]) &&
		IsValid(ActiveMIDs[1]) &&
		ActiveMIDs[0] != ActiveMIDs[1],
		TEXT("Concurrent presentations do not own isolated MIDs."));

	SurfaceSubsystem->ClearBloodMarks();

	for (TActorIterator<ACMBloodDecalActor> It(GetWorld()); It; ++It)
	{
		ACMBloodDecalActor* PresentationActor = *It;

		RecordPhase4SmokeTestResult(
			!PresentationActor->IsPresentationActive(),
			TEXT("ClearBloodMarks left an active presentation actor."));

		RecordPhase4SmokeTestResult(
			!PresentationActor->GetDecalComponent() ||
			!PresentationActor->GetDecalComponent()->IsRegistered(),
			TEXT("ClearBloodMarks left a registered decal component."));
	}

	Request.LifetimeSeconds = 0.1f;
	Request.FadeDurationSeconds = 0.05f;
	Request.RandomSeed = 41004;

	const TArray<FCMBloodResidueHandle> LifetimeHandles =
		SurfaceSubsystem->SpawnSurfaceBurst(Request);

	RecordPhase4SmokeTestResult(
		LifetimeHandles.Num() == 1,
		TEXT("Lifetime surface mark was not created."));

	FTimerHandle FinishTimer;
	GetWorld()->GetTimerManager().SetTimer(
		FinishTimer,
		this,
		&ThisClass::FinishPhase4SmokeTest,
		0.25f,
		false);
}


void ACMGoreMessageTestActor::FinishPhase4SmokeTest()
{
	UCMBloodSurfaceSubsystem* SurfaceSubsystem =
		GetWorld()->GetSubsystem<UCMBloodSurfaceSubsystem>();

	RecordPhase4SmokeTestResult(
		IsValid(SurfaceSubsystem) &&
		SurfaceSubsystem->GetActiveBloodMarkCount() == 0,
		TEXT("Lifetime expiration did not remove the semantic blood mark."));

	for (TActorIterator<ACMBloodDecalActor> It(GetWorld()); It; ++It)
	{
		ACMBloodDecalActor* PresentationActor = *It;

		RecordPhase4SmokeTestResult(
			!PresentationActor->IsPresentationActive(),
			TEXT("Lifetime expiration did not release the presentation actor."));

		RecordPhase4SmokeTestResult(
			IsValid(PresentationActor->GetDecalComponent()) &&
			!PresentationActor->GetDecalComponent()->IsRegistered(),
			TEXT("Lifetime fade destroyed or left registered the pooled decal component."));
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("CMGore Phase 4 Smoke Test: %s"),
		bPhase4SmokeTestPassed ? TEXT("PASS") : TEXT("FAIL"));

	FPlatformMisc::RequestExitWithStatus(
		false,
		bPhase4SmokeTestPassed ? 0 : 1,
		TEXT("CMGorePhase4SmokeTest"));
}


void ACMGoreMessageTestActor::RecordPhase4SmokeTestResult(
	bool bCondition,
	const TCHAR* FailureMessage)
{
	if (bCondition)
	{
		return;
	}

	bPhase4SmokeTestPassed = false;

	UE_LOG(
		LogTemp,
		Error,
		TEXT("CMGore Phase 4 Smoke Test: %s"),
		FailureMessage);
}

