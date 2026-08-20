#include "Runtime/Surface/CMBloodSurfaceSubsystem.h"

#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

#include "Settings/CMBloodSettings.h"


void UCMBloodSurfaceSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UCMBloodSettings* Settings =
		GetDefault<UCMBloodSettings>();

	if (!Settings)
	{
		return;
	}

	const int32 PrewarmCount =
		FMath::Max(
			0,
			Settings->InitialBloodDecalPoolSize
		);

	OwnedDecalComponents.Reserve(
		PrewarmCount
	);

	AvailableDecalComponents.Reserve(
		PrewarmCount
	);

	/*
	 * Prewarm 단계에서는 UObject만 생성한다.
	 *
	 * Render State는 실제 Decal이 사용될 때
	 * Material / Transform / Size 설정 후 생성한다.
	 */
	for (int32 Index = 0;
		 Index < PrewarmCount;
		 ++Index)
	{
		if (UDecalComponent* DecalComponent =
			CreateDecalComponent())
		{
			AvailableDecalComponents.Add(
				DecalComponent
			);
		}
	}
}


void UCMBloodSurfaceSubsystem::Deinitialize()
{
	UWorld* World =
		GetWorld();

	if (World)
	{
		FTimerManager& TimerManager =
			World->GetTimerManager();

		for (TPair<
			FCMBloodResidueHandle,
			FRuntimeBloodMarkState>& Pair
			: RuntimeBloodMarkStates)
		{
			TimerManager.ClearTimer(
				Pair.Value.ExpirationTimer
			);
		}
	}

	ActiveBloodMarks.Reset();
	RuntimeBloodMarkStates.Reset();
	BloodMarkSpawnOrder.Reset();
	AvailableDecalComponents.Reset();

	for (UDecalComponent* DecalComponent
		: OwnedDecalComponents)
	{
		if (IsValid(DecalComponent))
		{
			DecalComponent->DestroyComponent();
		}
	}

	OwnedDecalComponents.Reset();

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

	UDecalComponent* DecalComponent =
		AcquireDecalComponent();

	if (!IsValid(DecalComponent))
	{
		return InvalidHandle;
	}

	/*
	 * Pool의 inactive Decal은 항상 unregistered 상태여야 한다.
	 *
	 * 예외적으로 등록되어 있다면 먼저 render state를 제거한다.
	 */
	if (DecalComponent->IsRegistered())
	{
		DecalComponent->UnregisterComponent();
	}

	const FVector SurfaceNormal =
		Hit.ImpactNormal.GetSafeNormal();

	if (SurfaceNormal.IsNearlyZero())
	{
		ReleaseDecalComponent(
			DecalComponent
		);

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

	/*
	 * ---------------------------------------------------------
	 * 중요:
	 *
	 * Render State 생성 전에 Decal의 완전한 상태를 구성한다.
	 * ---------------------------------------------------------
	 */

	DecalComponent->SetDecalMaterial(
		Request.DecalMaterial
	);

	DecalComponent->DecalSize =
		DecalSize;

	DecalComponent->SetWorldLocationAndRotation(
		DecalLocation,
		DecalRotation
	);

	DecalComponent->SetHiddenInGame(
		false
	);

	DecalComponent->SetVisibility(
		true
	);

	/*
	 * 이전 사용에서 남은 Fade state 초기화.
	 */
	DecalComponent->SetFadeOut(
		0.0f,
		0.0f,
		false
	);

	/*
	 * Material / Size / Transform / Visibility가 준비된 뒤
	 * 처음으로 World에 등록한다.
	 *
	 * 이 시점에 Decal Render State가 생성된다.
	 */
	DecalComponent->RegisterComponentWithWorld(
		World
	);

	if (!DecalComponent->IsRegistered())
	{
		ReleaseDecalComponent(
			DecalComponent
		);

		return InvalidHandle;
	}

	const float Lifetime =
		Request.LifetimeSeconds;

	const float FadeDuration =
		Lifetime > 0.0f
			? FMath::Clamp(
				Request.FadeDurationSeconds,
				0.0f,
				Lifetime
			)
			: 0.0f;

	if (Lifetime > 0.0f &&
		FadeDuration > 0.0f)
	{
		const float FadeStartDelay =
			FMath::Max(
				0.0f,
				Lifetime -
				FadeDuration
			);

		DecalComponent->SetFadeOut(
			FadeStartDelay,
			FadeDuration,
			false
		);
	}

	const FCMBloodResidueHandle Handle =
		FCMBloodResidueHandle::Create();

	FCMBloodMark BloodMark;

	BloodMark.Handle =
		Handle;

	BloodMark.WorldTransform =
		FTransform(
			DecalRotation,
			DecalLocation
		);

	BloodMark.SurfaceNormal =
		SurfaceNormal;

	BloodMark.DecalSize =
		DecalSize;

	BloodMark.LifetimeSeconds =
		Lifetime;

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

	RuntimeState.DecalComponent =
		DecalComponent;

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
			->DecalComponent
			.IsValid())
		{
			ReleaseDecalComponent(
				RuntimeState
					->DecalComponent
					.Get()
			);
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


UDecalComponent*
UCMBloodSurfaceSubsystem::AcquireDecalComponent()
{
	while (!AvailableDecalComponents.IsEmpty())
	{
		const TWeakObjectPtr<UDecalComponent> WeakDecal =
			AvailableDecalComponents.Pop();

		if (WeakDecal.IsValid())
		{
			UDecalComponent* DecalComponent =
				WeakDecal.Get();

			/*
			 * Pool invariant:
			 * inactive Decal은 scene에 등록되어 있지 않는다.
			 */
			if (DecalComponent->IsRegistered())
			{
				DecalComponent->UnregisterComponent();
			}

			return DecalComponent;
		}
	}

	return CreateDecalComponent();
}


UDecalComponent*
UCMBloodSurfaceSubsystem::CreateDecalComponent()
{
	UWorld* World =
		GetWorld();

	if (!World)
	{
		return nullptr;
	}

	AWorldSettings* WorldSettings =
		World->GetWorldSettings();

	if (!IsValid(WorldSettings))
	{
		return nullptr;
	}

	UDecalComponent* DecalComponent =
		NewObject<UDecalComponent>(
			WorldSettings,
			NAME_None,
			RF_Transient
		);

	if (!IsValid(DecalComponent))
	{
		return nullptr;
	}

	/*
	 * Subsystem이 world-space transform을 직접 관리한다.
	 */
	DecalComponent->SetAbsolute(
		true,
		true,
		true
	);

	/*
	 * Prewarm에서는 Render State를 만들지 않는다.
	 *
	 * RegisterComponentWithWorld()는
	 * SpawnBloodMarkFromHit()에서 모든 상태가 준비된 후 호출한다.
	 */
	DecalComponent->SetHiddenInGame(
		true
	);

	DecalComponent->SetVisibility(
		false
	);

	OwnedDecalComponents.Add(
		DecalComponent
	);

	return DecalComponent;
}


void UCMBloodSurfaceSubsystem::ReleaseDecalComponent(
	UDecalComponent* DecalComponent)
{
	if (!IsValid(DecalComponent))
	{
		return;
	}

	/*
	 * 가장 먼저 Scene에서 제거한다.
	 *
	 * Render State가 제거된 뒤 아래 데이터를
	 * 안전하게 다음 사용을 위해 초기화한다.
	 */
	if (DecalComponent->IsRegistered())
	{
		DecalComponent->UnregisterComponent();
	}

	DecalComponent->SetFadeOut(
		0.0f,
		0.0f,
		false
	);

	DecalComponent->SetHiddenInGame(
		true
	);

	DecalComponent->SetVisibility(
		false
	);

	DecalComponent->SetDecalMaterial(
		nullptr
	);

	AvailableDecalComponents.Add(
		DecalComponent
	);
}