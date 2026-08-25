#include "Runtime/Surface/CMBloodSurfaceSubsystem.h"

#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "CoreGlobals.h"
#include "TimerManager.h"

#include "Runtime/Surface/Presentation/CMBloodDecalActor.h"
#include "Settings/CMBloodSettings.h"


void UCMBloodSurfaceSubsystem::Deinitialize()
{
	ClearBloodMarks();
	AvailablePresentationActors.Reset();

	for (ACMBloodDecalActor* PresentationActor
		: OwnedPresentationActors)
	{
		if (IsValid(PresentationActor))
		{
			PresentationActor->Destroy();
		}
	}

	OwnedPresentationActors.Reset();

	Super::Deinitialize();
}


TArray<FCMBloodResidueHandle>
UCMBloodSurfaceSubsystem::SpawnSurfaceBurst(
	const FCMBloodSurfaceBurstRequest& Request)
{
	TArray<FCMBloodResidueHandle> Result;

	UWorld* World =
		GetWorld();

	if (!World ||
		!IsValid(Request.DecalMaterial) ||
		Request.SampleCount <= 0 ||
		Request.TraceDistance <= 0.0f)
	{
		return Result;
	}

	const UCMBloodSettings* Settings =
		GetDefault<UCMBloodSettings>();

	const int32 GlobalSampleLimit =
		Settings
			? FMath::Max(
				0,
				Settings->MaxSurfaceSamplesPerEvent
			)
			: Request.SampleCount;

	const int32 SampleCount =
		FMath::Clamp(
			Request.SampleCount,
			0,
			GlobalSampleLimit
		);

	if (SampleCount <= 0)
	{
		return Result;
	}

	Result.Reserve(
		SampleCount
	);

	const int32 Seed =
		Request.RandomSeed != 0
			? Request.RandomSeed
			: FMath::Rand();

	FRandomStream RandomStream(
		Seed
	);

	FVector BaseDirection =
		Request.Direction.GetSafeNormal();

	if (BaseDirection.IsNearlyZero())
	{
		BaseDirection =
			FVector::DownVector;
	}

	const float ConeHalfAngleRadians =
		FMath::DegreesToRadians(
			FMath::Clamp(
				Request.ConeHalfAngleDegrees,
				0.0f,
				180.0f
			)
		);

	for (int32 SampleIndex = 0;
		 SampleIndex < SampleCount;
		 ++SampleIndex)
	{
		FVector SampleDirection =
			BaseDirection;

		if (ConeHalfAngleRadians >
			KINDA_SMALL_NUMBER)
		{
			SampleDirection =
				RandomStream.VRandCone(
					BaseDirection,
					ConeHalfAngleRadians
				);
		}

		const FVector TraceStart =
			Request.Origin;

		const FVector TraceEnd =
			TraceStart +
			SampleDirection *
			Request.TraceDistance;

		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(
				CMGoreBloodSurfaceTrace
			),
			Request.bTraceComplex
		);

		if (Request.IgnoredActor.IsValid())
		{
			QueryParams.AddIgnoredActor(
				Request.IgnoredActor.Get()
			);
		}

		FHitResult Hit;

		const bool bHit =
			World->LineTraceSingleByChannel(
				Hit,
				TraceStart,
				TraceEnd,
				Request.TraceChannel,
				QueryParams
			);

		if (!bHit ||
			!Hit.bBlockingHit)
		{
			continue;
		}

		const FCMBloodResidueHandle Handle =
			SpawnBloodMarkFromHit(
				Hit,
				Request,
				RandomStream
			);

		if (Handle.IsValid())
		{
			Result.Add(
				Handle
			);
		}
	}

	return Result;
}


FCMBloodResidueHandle UCMBloodSurfaceSubsystem::SpawnStrokeStamp(
	const FCMBloodStrokeStampRequest& Request)
{
	FCMBloodResidueHandle InvalidHandle;
	UWorld* World = GetWorld();
	const UCMBloodSettings* Settings = GetDefault<UCMBloodSettings>();
	if (!World || !Settings || !IsValid(Request.DecalMaterial) ||
		Request.Width <= 0.0f || Request.Length <= 0.0f ||
		Settings->MaxActiveBloodStrokeMarks <= 0 ||
		Settings->MaxStrokeStampsPerFrame <= 0)
	{
		return InvalidHandle;
	}

	if (LastStrokeFrame != GFrameCounter)
	{
		LastStrokeFrame = GFrameCounter;
		StrokeStampsThisFrame = 0;
	}
	if (StrokeStampsThisFrame >= Settings->MaxStrokeStampsPerFrame)
	{
		return InvalidHandle;
	}

	while (ActiveStrokeMarkCount >= Settings->MaxActiveBloodStrokeMarks)
	{
		const FCMBloodResidueHandle* OldestStroke =
			BloodMarkSpawnOrder.FindByPredicate(
				[this](const FCMBloodResidueHandle& Handle)
				{
					const FCMBloodMark* Mark = ActiveBloodMarks.Find(Handle);
					return Mark &&
						Mark->ResidueType == ECMBloodResidueType::Stroke;
				});
		if (!OldestStroke)
		{
			break;
		}
		RemoveBloodMark(*OldestStroke);
	}

	EvictOldestBloodMarkIfNeeded();

	const FVector SurfaceNormal = Request.SurfaceNormal.GetSafeNormal(
		SMALL_NUMBER,
		FVector::UpVector);
	FVector Tangent = FVector::VectorPlaneProject(
		Request.TangentDirection,
		SurfaceNormal).GetSafeNormal();
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector::CrossProduct(
			SurfaceNormal,
			FVector::RightVector).GetSafeNormal();
	}
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector::ForwardVector;
	}

	const FRotator DecalRotation =
		FRotationMatrix::MakeFromXZ(SurfaceNormal, Tangent).Rotator();
	const FVector DecalLocation = Request.SurfaceLocation +
		SurfaceNormal * FMath::Max(0.0f, Request.SurfaceOffset);
	const float Lifetime = FMath::Max(0.0f, Request.LifetimeSeconds);
	const float FadeDuration = Lifetime > 0.0f
		? FMath::Clamp(Request.FadeDurationSeconds, 0.0f, Lifetime)
		: 0.0f;

	TSubclassOf<ACMBloodDecalActor> PresentationClass =
		Request.DecalActorClass;
	if (!PresentationClass)
	{
		PresentationClass = ACMBloodDecalActor::StaticClass();
	}
	ACMBloodDecalActor* PresentationActor =
		AcquirePresentationActor(PresentationClass);
	if (!IsValid(PresentationActor))
	{
		return InvalidHandle;
	}

	FCMBloodDecalSpawnContext PresentationContext;
	PresentationContext.WorldTransform =
		FTransform(DecalRotation, DecalLocation);
	PresentationContext.SurfaceNormal = SurfaceNormal;
	PresentationContext.DecalSize = FVector(
		FMath::Max(0.1f, Request.DecalDepth),
		FMath::Max(0.1f, Request.Width),
		FMath::Max(0.1f, Request.Length));
	PresentationContext.LifetimeSeconds = Lifetime;
	PresentationContext.FadeDurationSeconds = FadeDuration;
	PresentationContext.RandomSeed = FMath::Rand();
	PresentationContext.Magnitude = 1.0f;
	PresentationContext.SurfaceActor = Request.SurfaceActor;
	PresentationContext.SurfaceComponent = Request.SurfaceComponent;

	PresentationActor->ActivatePresentation(
		PresentationContext,
		Request.DecalMaterial);
	if (!PresentationActor->IsPresentationActive())
	{
		ReleasePresentationActor(PresentationActor);
		return InvalidHandle;
	}

	const FCMBloodResidueHandle Handle = FCMBloodResidueHandle::Create();
	FCMBloodMark BloodMark;
	BloodMark.Handle = Handle;
	BloodMark.ResidueType = ECMBloodResidueType::Stroke;
	BloodMark.BloodDefinitionId = Request.BloodDefinitionId;
	BloodMark.WorldTransform = PresentationContext.WorldTransform;
	BloodMark.SurfaceNormal = SurfaceNormal;
	BloodMark.DecalSize = PresentationContext.DecalSize;
	BloodMark.SpawnTimeSeconds = World->GetTimeSeconds();
	BloodMark.LifetimeSeconds = Lifetime;
	BloodMark.PresentationProgress = 1.0f;
	BloodMark.SourceActor = Request.SourceActor;
	BloodMark.SurfaceActor = Request.SurfaceActor;
	BloodMark.SurfaceComponent = Request.SurfaceComponent;
	ActiveBloodMarks.Add(Handle, MoveTemp(BloodMark));
	BloodMarkSpawnOrder.Add(Handle);

	FRuntimeBloodMarkState RuntimeState;
	RuntimeState.PresentationActor = PresentationActor;
	if (Lifetime > 0.0f)
	{
		FTimerDelegate ExpirationDelegate;
		ExpirationDelegate.BindUObject(
			this,
			&UCMBloodSurfaceSubsystem::HandleBloodMarkExpired,
			Handle);
		World->GetTimerManager().SetTimer(
			RuntimeState.ExpirationTimer,
			ExpirationDelegate,
			Lifetime,
			false);
	}
	RuntimeBloodMarkStates.Add(Handle, MoveTemp(RuntimeState));
	++ActiveStrokeMarkCount;
	++StrokeStampsThisFrame;
	return Handle;
}


FCMBloodResidueHandle
UCMBloodSurfaceSubsystem::SpawnBloodMarkFromHit(
	const FHitResult& Hit,
	const FCMBloodSurfaceBurstRequest& Request,
	FRandomStream& RandomStream)
{
	FCMBloodResidueHandle InvalidHandle;

	if (!Hit.bBlockingHit ||
		!IsValid(Request.DecalMaterial))
	{
		return InvalidHandle;
	}

	UWorld* World =
		GetWorld();

	if (!World)
	{
		return InvalidHandle;
	}

	EvictOldestBloodMarkIfNeeded();

	const FVector SurfaceNormal =
		Hit.ImpactNormal.GetSafeNormal();

	if (SurfaceNormal.IsNearlyZero())
	{
		return InvalidHandle;
	}

	const FVector DecalLocation =
		Hit.ImpactPoint +
		SurfaceNormal *
		Request.SurfaceOffset;

	/*
	 * Decal local X axis를 Surface Normal과 정렬.
	 */
	FRotator DecalRotation =
		SurfaceNormal.Rotation();

	/*
	 * Projection axis(local X)를 중심으로 random rotation.
	 */
	DecalRotation.Roll +=
		RandomStream.FRandRange(
			0.0f,
			360.0f
		);

	const float MinExtent =
		FMath::Min(
			Request.DecalExtentRange.X,
			Request.DecalExtentRange.Y
		);

	const float MaxExtent =
		FMath::Max(
			Request.DecalExtentRange.X,
			Request.DecalExtentRange.Y
		);

	const float SurfaceExtent =
		RandomStream.FRandRange(
			FMath::Max(
				0.1f,
				MinExtent
			),
			FMath::Max(
				0.1f,
				MaxExtent
			)
		);

	const FVector DecalSize(
		FMath::Max(
			0.1f,
			Request.DecalDepth
		),
		SurfaceExtent,
		SurfaceExtent
	);

	const float Lifetime = FMath::Max(0.0f, Request.LifetimeSeconds);

	const float FadeDuration =
		Lifetime > 0.0f
			? FMath::Clamp(
				Request.FadeDurationSeconds,
				0.0f,
				Lifetime
			)
			: 0.0f;

	TSubclassOf<ACMBloodDecalActor> PresentationClass =
		Request.DecalActorClass;

	if (!PresentationClass)
	{
		PresentationClass = ACMBloodDecalActor::StaticClass();
	}

	ACMBloodDecalActor* PresentationActor =
		AcquirePresentationActor(PresentationClass);

	if (!IsValid(PresentationActor))
	{
		return InvalidHandle;
	}

	FCMBloodDecalSpawnContext PresentationContext;
	PresentationContext.WorldTransform =
		FTransform(DecalRotation, DecalLocation);
	PresentationContext.SurfaceNormal = SurfaceNormal;
	PresentationContext.DecalSize = DecalSize;
	PresentationContext.LifetimeSeconds = Lifetime;
	PresentationContext.FadeDurationSeconds = FadeDuration;
	PresentationContext.RandomSeed = RandomStream.GetCurrentSeed();
	PresentationContext.Magnitude = FMath::Max(0.0f, Request.PresentationMagnitude);
	PresentationContext.SurfaceActor = Hit.GetActor();
	PresentationContext.SurfaceComponent = Hit.GetComponent();

	PresentationActor->ActivatePresentation(
		PresentationContext,
		Request.DecalMaterial);

	if (!PresentationActor->IsPresentationActive())
	{
		ReleasePresentationActor(PresentationActor);
		return InvalidHandle;
	}

	const FCMBloodResidueHandle Handle =
		FCMBloodResidueHandle::Create();

	FCMBloodMark BloodMark;

	BloodMark.Handle =
		Handle;

	BloodMark.ResidueType = Request.ResidueType;

	BloodMark.BloodDefinitionId = Request.BloodDefinitionId;

	BloodMark.WorldTransform = PresentationContext.WorldTransform;

	BloodMark.SurfaceNormal =
		SurfaceNormal;

	BloodMark.DecalSize =
		DecalSize;

	BloodMark.LifetimeSeconds =
		Lifetime;

	BloodMark.PresentationProgress =
		Request.ResidueType == ECMBloodResidueType::Pool ? 0.0f : 1.0f;

	BloodMark.SourceActor = Request.SourceActor;

	BloodMark.SpawnTimeSeconds =
		World->GetTimeSeconds();

	BloodMark.SurfaceActor =
		Hit.GetActor();

	BloodMark.SurfaceComponent =
		Hit.GetComponent();

	ActiveBloodMarks.Add(
		Handle,
		MoveTemp(BloodMark)
	);

	BloodMarkSpawnOrder.Add(
		Handle
	);

	FRuntimeBloodMarkState RuntimeState;

	RuntimeState.PresentationActor = PresentationActor;

	if (Lifetime > 0.0f)
	{
		FTimerDelegate ExpirationDelegate;

		ExpirationDelegate.BindUObject(
			this,
			&UCMBloodSurfaceSubsystem::
				HandleBloodMarkExpired,
			Handle
		);

		World->GetTimerManager().SetTimer(
			RuntimeState.ExpirationTimer,
			ExpirationDelegate,
			Lifetime,
			false
		);
	}

	RuntimeBloodMarkStates.Add(
		Handle,
		MoveTemp(RuntimeState)
	);

	return Handle;
}


bool UCMBloodSurfaceSubsystem::RemoveBloodMark(
	FCMBloodResidueHandle Handle)
{
	if (!Handle.IsValid())
	{
		return false;
	}

	if (!ActiveBloodMarks.Contains(
			Handle
		))
	{
		return false;
	}

	if (FRuntimeBloodMarkState* RuntimeState =
		RuntimeBloodMarkStates.Find(
			Handle
		))
	{
		if (UWorld* World =
			GetWorld())
		{
			World->GetTimerManager().ClearTimer(
				RuntimeState->ExpirationTimer
			);
		}

		if (RuntimeState
			->PresentationActor
			.IsValid())
		{
			ReleasePresentationActor(
				RuntimeState
					->PresentationActor
					.Get()
			);
		}
	}

	if (const FCMBloodMark* Mark = ActiveBloodMarks.Find(Handle))
	{
		if (Mark->ResidueType == ECMBloodResidueType::Stroke)
		{
			ActiveStrokeMarkCount = FMath::Max(0, ActiveStrokeMarkCount - 1);
		}
	}

	RuntimeBloodMarkStates.Remove(
		Handle
	);

	ActiveBloodMarks.Remove(
		Handle
	);

	BloodMarkSpawnOrder.RemoveSingle(
		Handle
	);

	return true;
}


void UCMBloodSurfaceSubsystem::ClearBloodMarks()
{
	TArray<FCMBloodResidueHandle> Handles;

	ActiveBloodMarks.GetKeys(
		Handles
	);

	for (const FCMBloodResidueHandle& Handle
		: Handles)
	{
		RemoveBloodMark(
			Handle
		);
	}
}


bool UCMBloodSurfaceSubsystem::FindBloodMark(
	FCMBloodResidueHandle Handle,
	FCMBloodMark& OutBloodMark) const
{
	const FCMBloodMark* BloodMark =
		ActiveBloodMarks.Find(
			Handle
		);

	if (!BloodMark)
	{
		return false;
	}

	OutBloodMark =
		*BloodMark;

	return true;
}


bool UCMBloodSurfaceSubsystem::SetBloodMarkPresentationProgress(
	FCMBloodResidueHandle Handle,
	float NormalizedProgress)
{
	FCMBloodMark* BloodMark = ActiveBloodMarks.Find(Handle);
	if (!BloodMark)
	{
		return false;
	}

	FRuntimeBloodMarkState* RuntimeState =
		RuntimeBloodMarkStates.Find(Handle);

	if (!RuntimeState || !RuntimeState->PresentationActor.IsValid())
	{
		return false;
	}

	ACMBloodDecalActor* PresentationActor =
		RuntimeState->PresentationActor.Get();

	if (!PresentationActor->IsPresentationActive())
	{
		return false;
	}

	PresentationActor->SetPresentationProgress(NormalizedProgress);
	BloodMark->PresentationProgress =
		FMath::Clamp(NormalizedProgress, 0.0f, 1.0f);
	return true;
}


void UCMBloodSurfaceSubsystem::GetBloodMarksInRadius(
	FVector Center,
	float Radius,
	TArray<FCMBloodMark>& OutBloodMarks) const
{
	OutBloodMarks.Reset();

	if (Radius < 0.0f)
	{
		return;
	}

	const float RadiusSquared =
		FMath::Square(
			Radius
		);

	for (const TPair<
		FCMBloodResidueHandle,
		FCMBloodMark>& Pair
		: ActiveBloodMarks)
	{
		const FVector MarkLocation =
			Pair.Value
				.WorldTransform
				.GetLocation();

		if (FVector::DistSquared(
				Center,
				MarkLocation
			) <= RadiusSquared)
		{
			OutBloodMarks.Add(
				Pair.Value
			);
		}
	}
}


void UCMBloodSurfaceSubsystem::HandleBloodMarkExpired(
	FCMBloodResidueHandle Handle)
{
	RemoveBloodMark(
		Handle
	);
}


void UCMBloodSurfaceSubsystem::EvictOldestBloodMarkIfNeeded()
{
	const UCMBloodSettings* Settings =
		GetDefault<UCMBloodSettings>();

	if (!Settings)
	{
		return;
	}

	const int32 MaxActiveMarks =
		FMath::Max(
			0,
			Settings->MaxActiveBloodMarks
		);

	if (MaxActiveMarks <= 0)
	{
		return;
	}

	while (
		ActiveBloodMarks.Num() >=
			MaxActiveMarks &&
		!BloodMarkSpawnOrder.IsEmpty())
	{
		const FCMBloodResidueHandle OldestHandle =
			BloodMarkSpawnOrder[0];

		RemoveBloodMark(
			OldestHandle
		);
	}
}


ACMBloodDecalActor*
UCMBloodSurfaceSubsystem::AcquirePresentationActor(
	TSubclassOf<ACMBloodDecalActor> PresentationClass)
{
	if (!PresentationClass)
	{
		return nullptr;
	}

	TArray<TWeakObjectPtr<ACMBloodDecalActor>>& AvailableActors =
		AvailablePresentationActors.FindOrAdd(PresentationClass);

	while (!AvailableActors.IsEmpty())
	{
		const TWeakObjectPtr<ACMBloodDecalActor> WeakActor =
			AvailableActors.Pop();

		if (WeakActor.IsValid())
		{
			ACMBloodDecalActor* PresentationActor = WeakActor.Get();
			PresentationActor->DeactivatePresentation();
			return PresentationActor;
		}
	}

	return CreatePresentationActor(PresentationClass);
}


ACMBloodDecalActor*
UCMBloodSurfaceSubsystem::CreatePresentationActor(
	TSubclassOf<ACMBloodDecalActor> PresentationClass)
{
	UWorld* World = GetWorld();

	if (!World || !PresentationClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACMBloodDecalActor* PresentationActor =
		World->SpawnActor<ACMBloodDecalActor>(
			PresentationClass,
			FTransform::Identity,
			SpawnParameters);

	if (!IsValid(PresentationActor))
	{
		return nullptr;
	}

	PresentationActor->DeactivatePresentation();
	OwnedPresentationActors.Add(PresentationActor);

	return PresentationActor;
}


void UCMBloodSurfaceSubsystem::ReleasePresentationActor(
	ACMBloodDecalActor* PresentationActor)
{
	if (!IsValid(PresentationActor))
	{
		return;
	}

	PresentationActor->DeactivatePresentation();

	TSubclassOf<ACMBloodDecalActor> PresentationClass =
		PresentationActor->GetClass();

	AvailablePresentationActors
		.FindOrAdd(PresentationClass)
		.AddUnique(PresentationActor);
}
