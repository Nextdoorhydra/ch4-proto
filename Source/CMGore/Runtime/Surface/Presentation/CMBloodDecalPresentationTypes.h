#pragma once

#include "CoreMinimal.h"

#include "CMBloodDecalPresentationTypes.generated.h"

class AActor;
class UPrimitiveComponent;


/**
 * SurfaceSubsystem이 Presentation Actor에 전달하는 generic runtime contract.
 *
 * Material parameter 이름이나 renderer implementation detail은 포함하지 않는다.
 */
USTRUCT(BlueprintType)
struct CMGORE_API FCMBloodDecalSpawnContext
{
	GENERATED_BODY()

public:
	/** Actor의 world placement. Component relative transform은 Blueprint가 유지한다. */
	UPROPERTY(BlueprintReadOnly, Category = "CMGore|Blood Decal")
	FTransform WorldTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "CMGore|Blood Decal")
	FVector SurfaceNormal = FVector::UpVector;

	/** X: projection depth, Y/Z: surface extent. */
	UPROPERTY(BlueprintReadOnly, Category = "CMGore|Blood Decal")
	FVector DecalSize = FVector(8.0f, 24.0f, 24.0f);

	/** <= 0이면 SurfaceSubsystem이 자동 만료시키지 않는다. */
	UPROPERTY(BlueprintReadOnly, Category = "CMGore|Blood Decal")
	float LifetimeSeconds = 0.0f;

	/** Lifetime의 마지막 구간에 적용할 visual fade duration. */
	UPROPERTY(BlueprintReadOnly, Category = "CMGore|Blood Decal")
	float FadeDurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "CMGore|Blood Decal")
	int32 RandomSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CMGore|Blood Decal")
	TWeakObjectPtr<AActor> SurfaceActor;

	UPROPERTY(BlueprintReadOnly, Category = "CMGore|Blood Decal")
	TWeakObjectPtr<UPrimitiveComponent> SurfaceComponent;
};
