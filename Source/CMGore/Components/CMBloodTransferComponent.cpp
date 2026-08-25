#include "Components/CMBloodTransferComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/CMBloodDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Runtime/CMBloodSubsystem.h"
#include "Runtime/Surface/CMBloodSurfaceSubsystem.h"
#include "Settings/CMBloodSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMBloodTransfer, Log, All);

UCMBloodTransferComponent::UCMBloodTransferComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCMBloodTransferComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!TargetPrimitive && bAutoFindTargetPrimitive && GetOwner())
	{
		TargetPrimitive = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
		if (!TargetPrimitive)
		{
			TargetPrimitive = GetOwner()->FindComponentByClass<UPrimitiveComponent>();
		}
	}
	BindTargetPrimitive();
}

void UCMBloodTransferComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	UnbindTargetPrimitive();
	Super::EndPlay(EndPlayReason);
}

void UCMBloodTransferComponent::InitializeTransfer(
	UPrimitiveComponent* InTargetPrimitive,
	const FName InDefaultBloodDefinitionId)
{
	UnbindTargetPrimitive();
	TargetPrimitive = InTargetPrimitive;
	DefaultBloodDefinitionId = InDefaultBloodDefinitionId;
	if (IsRegistered())
	{
		BindTargetPrimitive();
	}
}

bool UCMBloodTransferComponent::ProcessContactSample(
	const FVector ContactLocation,
	const FVector ContactNormal,
	const float ContactSpeed)
{
	if (!bEnabled)
	{
		return false;
	}

	const FVector SafeNormal = ContactNormal.GetSafeNormal(
		SMALL_NUMBER,
		FVector::UpVector);
	FCMBloodMark PoolMark;
	if (FindPoolAtContact(ContactLocation, PoolMark))
	{
		if (TransferState == ECMBloodTransferState::Dry ||
			LoadedPoolHandle != PoolMark.Handle)
		{
			LoadFromPool(PoolMark, ContactLocation, SafeNormal);
		}
		else
		{
			LastPaintPoint = ContactLocation;
			LastContactNormal = SafeNormal;
			bHasLastPaintPoint = true;
		}
		return TransferState != ECMBloodTransferState::Dry;
	}

	UWorld* World = GetWorld();
	UCMBloodSubsystem* BloodSubsystem = World
		? World->GetSubsystem<UCMBloodSubsystem>()
		: nullptr;
	const UCMBloodDefinition* Definition = BloodSubsystem
		? BloodSubsystem->ResolveBloodDefinition(LoadedBloodDefinitionId)
		: nullptr;
	if (TransferState == ECMBloodTransferState::Dry || !Definition ||
		ContactSpeed < Definition->Stroke.MinimumPaintSpeed)
	{
		return false;
	}

	const int32 PreviousStampCount = SpawnedStrokeStampCount;
	PaintSegment(ContactLocation, SafeNormal);
	return SpawnedStrokeStampCount > PreviousStampCount;
}

void UCMBloodTransferComponent::BindTargetPrimitive()
{
	if (!bEnabled || !TargetPrimitive)
	{
		return;
	}

	TargetPrimitive->SetNotifyRigidBodyCollision(true);
	if (USkeletalMeshComponent* SkeletalMesh =
		Cast<USkeletalMeshComponent>(TargetPrimitive))
	{
		SkeletalMesh->SetAllBodiesNotifyRigidBodyCollision(true);
	}
	TargetPrimitive->OnComponentHit.AddUniqueDynamic(
		this,
		&UCMBloodTransferComponent::HandleTargetHit);
}

void UCMBloodTransferComponent::UnbindTargetPrimitive()
{
	if (TargetPrimitive)
	{
		TargetPrimitive->OnComponentHit.RemoveDynamic(
			this,
			&UCMBloodTransferComponent::HandleTargetHit);
	}
}

bool UCMBloodTransferComponent::FindPoolAtContact(
	const FVector& ContactLocation,
	FCMBloodMark& OutPoolMark) const
{
	const UWorld* World = GetWorld();
	const UCMBloodSurfaceSubsystem* SurfaceSubsystem = World
		? World->GetSubsystem<UCMBloodSurfaceSubsystem>()
		: nullptr;
	if (!SurfaceSubsystem || PoolQueryRadius <= 0.0f)
	{
		return false;
	}

	TArray<FCMBloodMark> NearbyMarks;
	SurfaceSubsystem->GetBloodMarksInRadius(
		ContactLocation,
		PoolQueryRadius,
		NearbyMarks);
	for (const FCMBloodMark& Mark : NearbyMarks)
	{
		if (Mark.ResidueType != ECMBloodResidueType::Pool ||
			Mark.PresentationProgress <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector LocalPoint =
			Mark.WorldTransform.InverseTransformPosition(ContactLocation);
		const float RadiusY = FMath::Max(
			0.1f,
			Mark.DecalSize.Y * Mark.PresentationProgress);
		const float RadiusZ = FMath::Max(
			0.1f,
			Mark.DecalSize.Z * Mark.PresentationProgress);
		const float EllipseDistance =
			FMath::Square(LocalPoint.Y / RadiusY) +
			FMath::Square(LocalPoint.Z / RadiusZ);
		if (EllipseDistance <= 1.0f)
		{
			OutPoolMark = Mark;
			return true;
		}
	}
	return false;
}

void UCMBloodTransferComponent::LoadFromPool(
	const FCMBloodMark& PoolMark,
	const FVector& ContactLocation,
	const FVector& ContactNormal)
{
	UWorld* World = GetWorld();
	UCMBloodSubsystem* BloodSubsystem = World
		? World->GetSubsystem<UCMBloodSubsystem>()
		: nullptr;
	const FName EffectiveDefinitionId = PoolMark.BloodDefinitionId.IsNone()
		? DefaultBloodDefinitionId
		: PoolMark.BloodDefinitionId;
	const UCMBloodDefinition* Definition = BloodSubsystem
		? BloodSubsystem->ResolveBloodDefinition(EffectiveDefinitionId)
		: nullptr;
	if (!Definition || !Definition->Stroke.bEnabled ||
		!IsValid(Definition->Stroke.DecalMaterial))
	{
		return;
	}

	const float MinDistance = FMath::Max(
		0.0f,
		FMath::Min(
			Definition->Stroke.TransferDistanceRange.X,
			Definition->Stroke.TransferDistanceRange.Y));
	const float MaxDistance = FMath::Max(
		MinDistance,
		FMath::Max(
			Definition->Stroke.TransferDistanceRange.X,
			Definition->Stroke.TransferDistanceRange.Y));
	LoadedBloodDefinitionId = EffectiveDefinitionId;
	LoadedPoolHandle = PoolMark.Handle;
	RemainingStrokeDistance = FMath::FRandRange(MinDistance, MaxDistance);
	LastPaintPoint = ContactLocation;
	LastContactNormal = ContactNormal.GetSafeNormal(
		SMALL_NUMBER,
		FVector::UpVector);
	bHasLastPaintPoint = true;
	TransferState = ECMBloodTransferState::Loaded;
}

void UCMBloodTransferComponent::PaintSegment(
	const FVector& ContactLocation,
	const FVector& ContactNormal)
{
	if (!bHasLastPaintPoint || RemainingStrokeDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	UWorld* World = GetWorld();
	UCMBloodSubsystem* BloodSubsystem = World
		? World->GetSubsystem<UCMBloodSubsystem>()
		: nullptr;
	UCMBloodSurfaceSubsystem* SurfaceSubsystem = World
		? World->GetSubsystem<UCMBloodSurfaceSubsystem>()
		: nullptr;
	const UCMBloodSettings* Settings = GetDefault<UCMBloodSettings>();
	const UCMBloodDefinition* Definition = BloodSubsystem
		? BloodSubsystem->ResolveBloodDefinition(LoadedBloodDefinitionId)
		: nullptr;
	if (!World || !SurfaceSubsystem || !Settings || !Definition ||
		!Definition->Stroke.bEnabled ||
		!IsValid(Definition->Stroke.DecalMaterial))
	{
		return;
	}

	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			const float CullDistance = FMath::Max(0.0f, Settings->StrokeCullDistance);
			if (CullDistance > 0.0f && FVector::DistSquared(
				Camera->GetCameraLocation(),
				ContactLocation) > FMath::Square(CullDistance))
			{
				return;
			}
		}
	}

	const FVector Segment = ContactLocation - LastPaintPoint;
	const float SegmentLength = Segment.Size();
	const float Spacing = FMath::Max(
		FMath::Max(0.1f, Definition->Stroke.StampSpacing),
		FMath::Max(0.1f, Settings->MinimumStrokeSpacing));
	if (SegmentLength < Spacing)
	{
		return;
	}

	const int32 RemainingStampBudget = FMath::Max(
		0,
		Settings->MaxStrokeStampsPerObject - SpawnedStrokeStampCount);
	const int32 RequestedStampCount = FMath::Min3(
		FMath::FloorToInt(SegmentLength / Spacing),
		FMath::FloorToInt(RemainingStrokeDistance / Spacing),
		RemainingStampBudget);
	if (RequestedStampCount <= 0)
	{
		TransferState = ECMBloodTransferState::Dry;
		RemainingStrokeDistance = 0.0f;
		return;
	}

	const FVector Tangent = Segment / SegmentLength;
	const float MinWidth = FMath::Max(
		0.1f,
		FMath::Min(Definition->Stroke.WidthRange.X,
			Definition->Stroke.WidthRange.Y));
	const float MaxWidth = FMath::Max(
		MinWidth,
		FMath::Max(Definition->Stroke.WidthRange.X,
			Definition->Stroke.WidthRange.Y));
	int32 SuccessfulStamps = 0;
	for (int32 StampIndex = 1; StampIndex <= RequestedStampCount; ++StampIndex)
	{
		const FVector SamplePoint =
			LastPaintPoint + Tangent * (Spacing * StampIndex);
		const FVector Normal = FMath::Lerp(
			LastContactNormal,
			ContactNormal,
			static_cast<float>(StampIndex) / RequestedStampCount)
			.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);

		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(CMBloodStrokeSurfaceTrace),
			false,
			GetOwner());
		FHitResult SurfaceHit;
		const FVector TraceStart = SamplePoint + Normal * 10.0f;
		const FVector TraceEnd = SamplePoint - Normal * SurfaceTraceDistance;
		if (!World->LineTraceSingleByChannel(
			SurfaceHit,
			TraceStart,
			TraceEnd,
			SurfaceTraceChannel,
			QueryParams))
		{
			continue;
		}

		FCMBloodStrokeStampRequest Request;
		Request.SurfaceLocation = SurfaceHit.ImpactPoint;
		Request.SurfaceNormal = SurfaceHit.ImpactNormal;
		Request.TangentDirection = Tangent;
		Request.BloodDefinitionId = LoadedBloodDefinitionId;
		Request.DecalMaterial = Definition->Stroke.DecalMaterial;
		Request.DecalActorClass = Definition->Stroke.DecalActorClass;
		Request.Width = FMath::FRandRange(MinWidth, MaxWidth);
		Request.Length = Definition->Stroke.StampLength;
		Request.LifetimeSeconds = Definition->Stroke.LifetimeSeconds;
		Request.FadeDurationSeconds = Definition->Stroke.FadeDurationSeconds;
		Request.SourceActor = GetOwner();
		Request.SurfaceActor = SurfaceHit.GetActor();
		Request.SurfaceComponent = SurfaceHit.GetComponent();
		if (!SurfaceSubsystem->SpawnStrokeStamp(Request).IsValid())
		{
			break;
		}
		++SuccessfulStamps;
	}

	if (SuccessfulStamps <= 0)
	{
		return;
	}

	const float PaintedDistance = SuccessfulStamps * Spacing;
	LastPaintPoint += Tangent * PaintedDistance;
	LastContactNormal = ContactNormal.GetSafeNormal(
		SMALL_NUMBER,
		FVector::UpVector);
	RemainingStrokeDistance = FMath::Max(
		0.0f,
		RemainingStrokeDistance - PaintedDistance);
	TotalPaintedDistance += PaintedDistance;
	SpawnedStrokeStampCount += SuccessfulStamps;
	TransferState = RemainingStrokeDistance > KINDA_SMALL_NUMBER
		? ECMBloodTransferState::Painting
		: ECMBloodTransferState::Dry;
}

void UCMBloodTransferComponent::HandleTargetHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!bEnabled || !TargetPrimitive || HitComponent != TargetPrimitive)
	{
		return;
	}

	const FVector ContactLocation = Hit.ImpactPoint.IsNearlyZero()
		? TargetPrimitive->Bounds.Origin
		: FVector(Hit.ImpactPoint);
	ProcessContactSample(
		ContactLocation,
		FVector(Hit.ImpactNormal),
		TargetPrimitive->GetPhysicsLinearVelocity().Size());
}
