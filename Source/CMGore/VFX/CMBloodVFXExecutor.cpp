#include "VFX/CMBloodVFXExecutor.h"

#include "Data/CMBloodDefinition.h"
#include "Runtime/CMBloodEvent.h"
#include "Runtime/Surface/CMBloodSurfaceSubsystem.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"


namespace CMBloodVFXParameters
{
	static const FName Magnitude(
		TEXT("User.CMGoreMagnitude")
	);

	static const FName Direction(
		TEXT("User.CMGoreDirection")
	);

	static const FName SurfaceNormal(
		TEXT("User.CMGoreSurfaceNormal")
	);
}


bool FCMBloodVFXExecutor::ExecuteInstant(
	UObject* WorldContextObject,
	const FCMBloodEvent& Event,
	const UCMBloodDefinition& Definition
)
{
	const FCMBloodInstantVFXDefinition* VFXDefinition = nullptr;

	switch (Event.Type)
	{
	case ECMBloodEventType::Impact:
		VFXDefinition = &Definition.Impact;
		break;

	case ECMBloodEventType::Burst:
		VFXDefinition = &Definition.Burst;
		break;

	default:
		// Phase 3에서는 순간성 Impact/Burst만 처리한다.
		return false;
	}

	if (!VFXDefinition)
	{
		return false;
	}

	UNiagaraSystem* NiagaraSystem =
		VFXDefinition->NiagaraSystem.Get();

	if (!IsValid(NiagaraSystem))
	{
		return false;
	}

	FVector EffectDirection = Event.Direction.GetSafeNormal();

	// Direction이 없는 Impact의 경우 SurfaceNormal을 fallback으로 사용한다.
	if (EffectDirection.IsNearlyZero())
	{
		EffectDirection = Event.SurfaceNormal.GetSafeNormal();
	}

	if (EffectDirection.IsNearlyZero())
	{
		EffectDirection = FVector::UpVector;
	}

	const FRotator Rotation =
		EffectDirection.Rotation();

	/**
	 * User Parameter를 설정한 뒤 첫 Activation이 일어나도록
	 * AutoActivate = false로 Spawn한다.
	 */
	UNiagaraComponent* NiagaraComponent =
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			WorldContextObject,
			NiagaraSystem,
			Event.Location,
			Rotation,
			VFXDefinition->Scale,

			// Auto Destroy
			true,

			// Auto Activate
			false,

			// Phase 3에서는 별도 Pooling 정책을 적용하지 않는다.
			ENCPoolMethod::None,

			// Pre Cull Check
			true
		);

	if (!IsValid(NiagaraComponent))
	{
		return false;
	}

	NiagaraComponent->SetVariableFloat(
		CMBloodVFXParameters::Magnitude,
		Event.Magnitude
	);

	NiagaraComponent->SetVariableVec3(
		CMBloodVFXParameters::Direction,
		Event.Direction
	);

	NiagaraComponent->SetVariableVec3(
		CMBloodVFXParameters::SurfaceNormal,
		Event.SurfaceNormal
	);

	NiagaraComponent->Activate(true);

	if (Definition.Surface.bEnabled &&
	IsValid(Definition.Surface.DecalMaterial))
	{
		if (UWorld* World = WorldContextObject->GetWorld())
		{
			if (UCMBloodSurfaceSubsystem* SurfaceSubsystem =
				World->GetSubsystem<UCMBloodSurfaceSubsystem>())
			{
				FCMBloodSurfaceBurstRequest SurfaceRequest;

				SurfaceRequest.Origin =
					Event.Location;

				SurfaceRequest.Direction =
					EffectDirection;

				SurfaceRequest.DecalMaterial =
					Definition.Surface.DecalMaterial;

				SurfaceRequest.SampleCount =
					Definition.Surface.SampleCount;

				SurfaceRequest.TraceDistance =
					Definition.Surface.TraceDistance;

				SurfaceRequest.ConeHalfAngleDegrees =
					Definition.Surface.ConeHalfAngleDegrees;

				SurfaceRequest.DecalExtentRange =
					Definition.Surface.DecalExtentRange;

				SurfaceRequest.DecalDepth =
					Definition.Surface.DecalDepth;

				SurfaceRequest.LifetimeSeconds =
					Definition.Surface.LifetimeSeconds;

				SurfaceRequest.FadeDurationSeconds =
					Definition.Surface.FadeDurationSeconds;

				SurfaceSubsystem->SpawnSurfaceBurst(
					SurfaceRequest);
			}
		}
	}

	
	return true;
}