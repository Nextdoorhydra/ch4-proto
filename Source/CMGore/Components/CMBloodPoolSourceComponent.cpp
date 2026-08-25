#include "Components/CMBloodPoolSourceComponent.h"

#include "Data/CMBloodDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Runtime/CMBloodSubsystem.h"
#include "Runtime/Surface/CMBloodSurfaceSubsystem.h"


UCMBloodPoolSourceComponent::UCMBloodPoolSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void UCMBloodPoolSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStart && GetOwner())
	{
		StartBloodPool(
			GetOwner()->GetActorLocation(),
			FVector::UpVector,
			1.0f,
			BloodDefinitionId);
	}
}


void UCMBloodPoolSourceComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	RemoveBloodPool();
	Super::EndPlay(EndPlayReason);
}


void UCMBloodPoolSourceComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bGrowing)
	{
		SetComponentTickEnabled(false);
		return;
	}

	UWorld* World = GetWorld();
	UCMBloodSurfaceSubsystem* SurfaceSubsystem =
		World ? World->GetSubsystem<UCMBloodSurfaceSubsystem>() : nullptr;

	if (!SurfaceSubsystem ||
		!SurfaceSubsystem->IsBloodMarkActive(ActivePoolHandle))
	{
		ResetRuntimeState();
		return;
	}

	GrowthElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	GrowthProgress =
		GrowthDurationSeconds <= 0.0f
			? 1.0f
			: FMath::Clamp(
				GrowthElapsedSeconds / GrowthDurationSeconds,
				0.0f,
				1.0f);

	if (!SurfaceSubsystem->SetBloodMarkPresentationProgress(
		ActivePoolHandle,
		GrowthProgress))
	{
		ResetRuntimeState();
		return;
	}

	if (GrowthProgress >= 1.0f)
	{
		bGrowing = false;
		SetComponentTickEnabled(false);
	}
}


bool UCMBloodPoolSourceComponent::StartBloodPool(
	FVector Location,
	FVector SurfaceNormal,
	float Amount,
	FName RequestedDefinitionId)
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	RemoveBloodPool();

	UCMBloodSubsystem* BloodSubsystem =
		World->GetSubsystem<UCMBloodSubsystem>();
	UCMBloodSurfaceSubsystem* SurfaceSubsystem =
		World->GetSubsystem<UCMBloodSurfaceSubsystem>();

	if (!BloodSubsystem || !SurfaceSubsystem)
	{
		return false;
	}

	const FName EffectiveDefinitionId =
		RequestedDefinitionId.IsNone()
			? BloodDefinitionId
			: RequestedDefinitionId;

	const UCMBloodDefinition* Definition =
		BloodSubsystem->ResolveBloodDefinition(EffectiveDefinitionId);

	if (!IsValid(Definition) ||
		!Definition->Pool.bEnabled ||
		!IsValid(Definition->Pool.DecalMaterial))
	{
		return false;
	}

	FVector Normal = SurfaceNormal.GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::UpVector;
	}

	FCMBloodSurfaceBurstRequest Request;
	Request.ResidueType = ECMBloodResidueType::Pool;
	Request.BloodDefinitionId = EffectiveDefinitionId;
	Request.SourceActor = GetOwner();
	Request.Origin = Location + Normal * FMath::Max(0.0f, TraceStartOffset);
	Request.Direction = -Normal;
	Request.SampleCount = 1;
	Request.TraceDistance = FMath::Max(0.0f, TraceDistance);
	Request.ConeHalfAngleDegrees = 0.0f;
	Request.TraceChannel = TraceChannel;
	Request.bTraceComplex = bTraceComplex;
	Request.DecalMaterial = Definition->Pool.DecalMaterial;
	Request.DecalActorClass = Definition->Pool.DecalActorClass;
	Request.DecalExtentRange = Definition->Pool.DecalExtentRange;
	Request.DecalDepth = Definition->Pool.DecalDepth;
	Request.LifetimeSeconds = LifetimeSeconds;
	Request.FadeDurationSeconds = FadeDurationSeconds;
	Request.SurfaceOffset = SurfaceOffset;
	Request.PresentationMagnitude = FMath::Max(0.0f, Amount);
	Request.IgnoredActor = GetOwner();

	const TArray<FCMBloodResidueHandle> Handles =
		SurfaceSubsystem->SpawnSurfaceBurst(Request);

	if (Handles.IsEmpty())
	{
		return false;
	}

	ActivePoolHandle = Handles[0];
	GrowthElapsedSeconds = 0.0f;
	GrowthProgress = 0.0f;
	SurfaceSubsystem->SetBloodMarkPresentationProgress(
		ActivePoolHandle,
		GrowthProgress);

	if (GrowthDurationSeconds <= 0.0f)
	{
		GrowthProgress = 1.0f;
		SurfaceSubsystem->SetBloodMarkPresentationProgress(
			ActivePoolHandle,
			GrowthProgress);
		bGrowing = false;
		SetComponentTickEnabled(false);
	}
	else
	{
		bGrowing = true;
		SetComponentTickEnabled(true);
	}

	return true;
}


void UCMBloodPoolSourceComponent::StopBloodPool()
{
	bGrowing = false;
	SetComponentTickEnabled(false);
}


void UCMBloodPoolSourceComponent::RemoveBloodPool()
{
	if (ActivePoolHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UCMBloodSurfaceSubsystem* SurfaceSubsystem =
				World->GetSubsystem<UCMBloodSurfaceSubsystem>())
			{
				SurfaceSubsystem->RemoveBloodMark(ActivePoolHandle);
			}
		}
	}

	ResetRuntimeState();
}


bool UCMBloodPoolSourceComponent::IsBloodPoolActive() const
{
	UWorld* World = GetWorld();
	UCMBloodSurfaceSubsystem* SurfaceSubsystem =
		World ? World->GetSubsystem<UCMBloodSurfaceSubsystem>() : nullptr;

	return SurfaceSubsystem &&
		SurfaceSubsystem->IsBloodMarkActive(ActivePoolHandle);
}


void UCMBloodPoolSourceComponent::ResetRuntimeState()
{
	SetComponentTickEnabled(false);
	ActivePoolHandle.Reset();
	GrowthElapsedSeconds = 0.0f;
	GrowthProgress = 0.0f;
	bGrowing = false;
}
