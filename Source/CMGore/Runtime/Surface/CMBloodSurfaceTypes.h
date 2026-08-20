#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

#include "CMBloodSurfaceTypes.generated.h"

class AActor;
class UMaterialInterface;
class UPrimitiveComponent;

/**
 * 월드에 남은 Blood Residue를 외부에서 참조하기 위한 안정적인 Handle.
 *
 * UDecalComponent를 직접 외부에 노출하지 않는다.
 */
USTRUCT(BlueprintType)
struct CMGORE_API FCMBloodResidueHandle
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CMGore|Blood Surface")
	FGuid Id;

public:
	static FCMBloodResidueHandle Create()
	{
		FCMBloodResidueHandle Handle;
		Handle.Id = FGuid::NewGuid();
		return Handle;
	}

	bool IsValid() const
	{
		return Id.IsValid();
	}

	void Reset()
	{
		Id.Invalidate();
	}

	friend bool operator==(const FCMBloodResidueHandle& A, const FCMBloodResidueHandle& B)
	{
		return A.Id == B.Id;
	}

	friend bool operator!=(const FCMBloodResidueHandle& A, const FCMBloodResidueHandle& B)
	{
		return !(A == B);
	}

	friend uint32 GetTypeHash(const FCMBloodResidueHandle& Handle)
	{
		return GetTypeHash(Handle.Id);
	}
};


/**
 * 논리적인 Blood Mark 데이터.
 *
 * 실제 렌더링에 사용되는 UDecalComponent와 의도적으로 분리한다.
 *
 * 향후:
 * - Material Highlight
 * - Sixth Sense
 * - Footprint tracking
 * - Blood trail querying
 *
 * 등의 시스템은 이 데이터를 조회할 수 있다.
 */
USTRUCT(BlueprintType)
struct CMGORE_API FCMBloodMark
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CMGore|Blood Surface")
	FCMBloodResidueHandle Handle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CMGore|Blood Surface")
	FTransform WorldTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CMGore|Blood Surface")
	FVector SurfaceNormal = FVector::UpVector;

	/**
	 * UDecalComponent::DecalSize에 적용된 값.
	 *
	 * X : projection depth
	 * Y/Z : surface extent
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CMGore|Blood Surface")
	FVector DecalSize = FVector(8.0f, 24.0f, 24.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CMGore|Blood Surface")
	float SpawnTimeSeconds = 0.0f;

	/**
	 * <= 0 이면 subsystem이 자동 제거하지 않는다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CMGore|Blood Surface")
	float LifetimeSeconds = 0.0f;

	/**
	 * 혈흔이 붙어 있는 Actor.
	 *
	 * 강한 참조를 잡지 않기 위해 WeakPtr 사용.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CMGore|Blood Surface")
	TWeakObjectPtr<AActor> SurfaceActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CMGore|Blood Surface")
	TWeakObjectPtr<UPrimitiveComponent> SurfaceComponent;
};


/**
 * 하나의 Blood Event가 Surface에 남길 흔적 생성 요청.
 *
 * Niagara particle collision 하나하나를 decal spawn으로 연결하지 않고
 * Event 단위로 제한된 수의 trace sample을 생성한다.
 */
USTRUCT(BlueprintType)
struct CMGORE_API FCMBloodSurfaceBurstRequest
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface")
	FVector Origin = FVector::ZeroVector;

	/**
	 * 혈액이 진행하는 대표 방향.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface")
	FVector Direction = FVector::DownVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface",
		meta = (ClampMin = "0"))
	int32 SampleCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface",
		meta = (ClampMin = "0.0"))
	float TraceDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface",
		meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ConeHalfAngleDegrees = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface")
	bool bTraceComplex = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface")
	TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

	/**
	 * Decal Y/Z extent random range.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface")
	FVector2D DecalExtentRange = FVector2D(18.0f, 36.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface",
		meta = (ClampMin = "0.1"))
	float DecalDepth = 8.0f;

	/**
	 * <= 0 이면 영구 Blood Mark.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface")
	float LifetimeSeconds = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface",
		meta = (ClampMin = "0.0"))
	float FadeDurationSeconds = 5.0f;

	/**
	 * Z-fighting 방지를 위한 surface normal offset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface",
		meta = (ClampMin = "0.0"))
	float SurfaceOffset = 1.0f;

	/**
	 * 0이면 runtime random seed 사용.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Surface")
	int32 RandomSeed = 0;

	UPROPERTY()
	TWeakObjectPtr<AActor> IgnoredActor;
};