#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CMBloodDefinition.generated.h"

class ACMBloodDecalActor;
class UMaterialInterface;
class UNiagaraSystem;


/**
 * 순간적으로 실행되고 자체 종료되는 Blood Niagara 정의.
 *
 * Phase 3에서는 Impact / Burst 표현에만 사용한다.
 */
USTRUCT(BlueprintType)
struct CMGORE_API FCMBloodInstantVFXDefinition
{
	GENERATED_BODY()

	/** 실행할 Niagara System. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	/** Niagara Component의 Spawn Scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	FVector Scale = FVector::OneVector;
};

USTRUCT(BlueprintType)
struct CMGORE_API FCMBloodSurfaceDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Surface")
	bool bEnabled = false;

	/** Decal의 표현 방식과 Blueprint 확장 지점을 선택한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Surface")
	TSubclassOf<ACMBloodDecalActor> DecalActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Surface")
	TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Surface",
		meta = (ClampMin = "0"))
	int32 SampleCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Surface",
		meta = (ClampMin = "0.0"))
	float TraceDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Surface",
		meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ConeHalfAngleDegrees = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Surface")
	FVector2D DecalExtentRange = FVector2D(18.0f, 36.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Surface")
	float DecalDepth = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Surface")
	float LifetimeSeconds = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Surface")
	float FadeDurationSeconds = 5.0f;
};

USTRUCT(BlueprintType)
struct CMGORE_API FCMBloodPoolDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Pool")
	bool bEnabled = false;

	/** Pool의 표현 방식. Growth parameter 해석은 이 Actor/BP가 소유한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Pool")
	TSubclassOf<ACMBloodDecalActor> DecalActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Pool")
	TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Pool")
	FVector2D DecalExtentRange = FVector2D(36.0f, 48.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Pool",
		meta = (ClampMin = "0.1"))
	float DecalDepth = 8.0f;
};

USTRUCT(BlueprintType)
struct CMGORE_API FCMBloodStrokeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Stroke")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Stroke")
	TSubclassOf<ACMBloodDecalActor> DecalActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Stroke")
	TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Stroke")
	FVector2D TransferDistanceRange = FVector2D(100.0f, 200.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Stroke")
	FVector2D WidthRange = FVector2D(15.0f, 25.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Stroke",
		meta = (ClampMin = "0.1"))
	float StampLength = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Stroke",
		meta = (ClampMin = "0.1"))
	float StampSpacing = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Stroke",
		meta = (ClampMin = "0.0"))
	float MinimumPaintSpeed = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Stroke")
	float LifetimeSeconds = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Stroke",
		meta = (ClampMin = "0.0"))
	float FadeDurationSeconds = 5.0f;
};

/**
 * 하나의 혈액 종류에 대한 표현 Definition.
 *
 * 예:
 * Human.Red
 * Alien.Green
 * Robot.Oil
 */
UCLASS(BlueprintType)
class CMGORE_API UCMBloodDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * GameplayMessage에서 전달되는 논리적인 ID.
	 *
	 * Registry 내부에서 반드시 유일해야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition")
	FName DefinitionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood")
	FCMBloodSurfaceDefinition Surface;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood")
	FCMBloodPoolDefinition Pool;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood")
	FCMBloodStrokeDefinition Stroke;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Instant VFX")
	FCMBloodInstantVFXDefinition Impact;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Instant VFX")
	FCMBloodInstantVFXDefinition Burst;
};
