// Fill out your copyright notice in the Description page of Project Settings.


#include "CMGoreMessageTestActor.h"

#include "Components/DecalComponent.h"
#include "Components/CMBloodPoolSourceComponent.h"
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
	BloodPoolSourceComponent = CreateDefaultSubobject<UCMBloodPoolSourceComponent>(
		TEXT("BloodPoolSource"));
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
	else if (FParse::Param(
		FCommandLine::Get(),
		TEXT("CMGorePhase5SmokeTest")))
	{
		RunPhase5SmokeTest();
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


void ACMGoreMessageTestActor::RunPhase5SmokeTest()
{
	UCMBloodSurfaceSubsystem* SurfaceSubsystem =
		GetWorld()->GetSubsystem<UCMBloodSurfaceSubsystem>();

	UCMBloodDefinition* Definition = LoadObject<UCMBloodDefinition>(
		nullptr,
		TEXT("/Game/CMGore/Definitions/DA_CMBlood_HumanRed.DA_CMBlood_HumanRed"));

	RecordPhase5SmokeTestResult(
		IsValid(SurfaceSubsystem),
		TEXT("Blood Surface Subsystem is unavailable."));
	RecordPhase5SmokeTestResult(
		IsValid(Definition) &&
		Definition->Pool.bEnabled &&
		IsValid(Definition->Pool.DecalMaterial) &&
		Definition->Pool.DecalActorClass != nullptr,
		TEXT("Human.Red pool definition is incomplete."));

	if (!SurfaceSubsystem ||
		!Definition ||
		!Definition->Pool.bEnabled ||
		!Definition->Pool.DecalMaterial ||
		!Definition->Pool.DecalActorClass)
	{
		FinishPhase5SmokeTest();
		return;
	}

	SurfaceSubsystem->ClearBloodMarks();
	BloodPoolSourceComponent->GrowthDurationSeconds = 0.2f;
	BloodPoolSourceComponent->LifetimeSeconds = 0.6f;
	BloodPoolSourceComponent->FadeDurationSeconds = 0.1f;
	BloodPoolSourceComponent->TraceDistance = 5000.0f;

	FCMBloodPoolMessage StartMessage;
	StartMessage.Source = this;
	StartMessage.Location = GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
	StartMessage.SurfaceNormal = FVector::UpVector;
	StartMessage.Amount = 1.0f;
	StartMessage.BloodDefinitionId = TEXT("Human.Red");

	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		CMGoreGameplayTags::Message::Blood::Pool::Start,
		StartMessage);

	RecordPhase5SmokeTestResult(
		BloodPoolSourceComponent->IsBloodPoolActive() &&
		BloodPoolSourceComponent->IsBloodPoolGrowing(),
		TEXT("PoolStart message did not activate growth."));

	for (TActorIterator<ACMBloodDecalActor> It(GetWorld()); It; ++It)
	{
		if (It->IsPresentationActive())
		{
			Phase5FirstPresentationActor = *It;
			Phase5FirstMID = It->GetDynamicMaterial();
			break;
		}
	}

	RecordPhase5SmokeTestResult(
		IsValid(Phase5FirstPresentationActor) && IsValid(Phase5FirstMID),
		TEXT("Pool presentation actor or MID was not created."));

	FTimerHandle Timer;
	GetWorld()->GetTimerManager().SetTimer(
		Timer,
		this,
		&ThisClass::CheckPhase5GrowthAndStop,
		0.08f,
		false);
}


void ACMGoreMessageTestActor::CheckPhase5GrowthAndStop()
{
	const float Progress = BloodPoolSourceComponent->GetGrowthProgress();
	RecordPhase5SmokeTestResult(
		Progress > 0.0f && Progress < 1.0f,
		TEXT("Pool did not advance through an intermediate growth value."));
	RecordPhase5SmokeTestResult(
		IsValid(Phase5FirstPresentationActor) &&
		FMath::IsNearlyEqual(
			Phase5FirstPresentationActor->GetPresentationProgress(),
			Progress,
			KINDA_SMALL_NUMBER),
		TEXT("Semantic growth was not forwarded to the presentation actor."));

	FCMBloodPoolMessage StopMessage;
	StopMessage.Source = this;
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		CMGoreGameplayTags::Message::Blood::Pool::Stop,
		StopMessage);

	Phase5StoppedProgress = BloodPoolSourceComponent->GetGrowthProgress();

	FTimerHandle Timer;
	GetWorld()->GetTimerManager().SetTimer(
		Timer,
		this,
		&ThisClass::CheckPhase5StopAndReuse,
		0.08f,
		false);
}


void ACMGoreMessageTestActor::CheckPhase5StopAndReuse()
{
	RecordPhase5SmokeTestResult(
		BloodPoolSourceComponent->IsBloodPoolActive() &&
		!BloodPoolSourceComponent->IsBloodPoolGrowing() &&
		FMath::IsNearlyEqual(
			BloodPoolSourceComponent->GetGrowthProgress(),
			Phase5StoppedProgress,
			KINDA_SMALL_NUMBER),
		TEXT("PoolStop did not freeze the active pool at its current progress."));

	BloodPoolSourceComponent->RemoveBloodPool();
	BloodPoolSourceComponent->LifetimeSeconds = 0.25f;

	FCMBloodPoolMessage StartMessage;
	StartMessage.Source = this;
	StartMessage.Location = GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
	StartMessage.SurfaceNormal = FVector::UpVector;
	StartMessage.Amount = 1.0f;
	StartMessage.BloodDefinitionId = TEXT("Human.Red");
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		CMGoreGameplayTags::Message::Blood::Pool::Start,
		StartMessage);

	ACMBloodDecalActor* ReusedActor = nullptr;
	for (TActorIterator<ACMBloodDecalActor> It(GetWorld()); It; ++It)
	{
		if (It->IsPresentationActive())
		{
			ReusedActor = *It;
			break;
		}
	}

	RecordPhase5SmokeTestResult(
		ReusedActor == Phase5FirstPresentationActor,
		TEXT("Pool presentation actor was not reused."));
	RecordPhase5SmokeTestResult(
		ReusedActor &&
		ReusedActor->GetPresentationProgress() <= KINDA_SMALL_NUMBER,
		TEXT("Reused pool presentation did not reset progress to zero."));
	RecordPhase5SmokeTestResult(
		ReusedActor &&
		IsValid(ReusedActor->GetDynamicMaterial()) &&
		ReusedActor->GetDynamicMaterial() != Phase5FirstMID,
		TEXT("Reused pool presentation did not receive an isolated fresh MID."));

	FTimerHandle Timer;
	GetWorld()->GetTimerManager().SetTimer(
		Timer,
		this,
		&ThisClass::FinishPhase5SmokeTest,
		0.4f,
		false);
}


void ACMGoreMessageTestActor::FinishPhase5SmokeTest()
{
	RecordPhase5SmokeTestResult(
		!BloodPoolSourceComponent->IsBloodPoolActive(),
		TEXT("Pool lifetime expiration did not remove the semantic mark."));

	for (TActorIterator<ACMBloodDecalActor> It(GetWorld()); It; ++It)
	{
		RecordPhase5SmokeTestResult(
			!It->IsPresentationActive(),
			TEXT("Pool lifetime expiration did not release the presentation actor."));
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("CMGore Phase 5 Smoke Test: %s"),
		bPhase5SmokeTestPassed ? TEXT("PASS") : TEXT("FAIL"));

	FPlatformMisc::RequestExitWithStatus(
		false,
		bPhase5SmokeTestPassed ? 0 : 1,
		TEXT("CMGorePhase5SmokeTest"));
}


void ACMGoreMessageTestActor::RecordPhase5SmokeTestResult(
	bool bCondition,
	const TCHAR* FailureMessage)
{
	if (bCondition)
	{
		return;
	}

	bPhase5SmokeTestPassed = false;
	UE_LOG(
		LogTemp,
		Error,
		TEXT("CMGore Phase 5 Smoke Test: %s"),
		FailureMessage);
}



