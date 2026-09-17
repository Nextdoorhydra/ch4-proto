#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"

#include "Runtime/Surface/CMBloodSurfaceTypes.h"

#include "CMBloodPoolSourceComponent.generated.h"


/**
 * Corpse/Wounded source 하나가 소유하는 Growing Blood Pool 상태.
 *
 * 언제 생성하고 얼마나 성장/유지할지는 이 Component가 담당하지만,
 * Material parameter와 renderer lifecycle은 Presentation Actor에 위임한다.
 */
UCLASS(ClassGroup = (CMGore), meta = (BlueprintSpawnableComponent))
class CMGORE_API UCMBloodPoolSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCMBloodPoolSourceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "CMGore|Blood Pool")
	bool StartBloodPool(
		FVector Location,
		FVector SurfaceNormal,
		float Amount = 1.0f,
		FName RequestedDefinitionId = NAME_None);

	/** 성장을 현재 값에서 멈춘다. 이미 생성된 mark는 lifetime까지 유지한다. */
	UFUNCTION(BlueprintCallable, Category = "CMGore|Blood Pool")
	void StopBloodPool();

	/** mark를 즉시 제거하고 Source 상태를 초기화한다. */
	UFUNCTION(BlueprintCallable, Category = "CMGore|Blood Pool")
	void RemoveBloodPool();

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Pool")
	bool IsBloodPoolActive() const;

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Pool")
	bool IsBloodPoolGrowing() const
	{
		return bGrowing;
	}

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Pool")
	float GetGrowthProgress() const
	{
		return GrowthProgress;
	}

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Pool")
	FCMBloodResidueHandle GetBloodPoolHandle() const
	{
		return ActivePoolHandle;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Pool")
	FName BloodDefinitionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Pool")
	bool bAutoStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Pool",
		meta = (ClampMin = "0.0"))
	float GrowthDurationSeconds = 4.0f;

	/** <= 0이면 자동 만료하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Pool")
	float LifetimeSeconds = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Pool",
		meta = (ClampMin = "0.0"))
	float FadeDurationSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Pool",
		meta = (ClampMin = "0.0"))
	float TraceDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Pool",
		meta = (ClampMin = "0.0"))
	float TraceStartOffset = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Pool",
		meta = (ClampMin = "0.0"))
	float SurfaceOffset = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Pool")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CMGore|Blood Pool")
	bool bTraceComplex = false;

private:
	void ResetRuntimeState();

private:
	UPROPERTY(Transient)
	FCMBloodResidueHandle ActivePoolHandle;

	UPROPERTY(Transient)
	float GrowthElapsedSeconds = 0.0f;

	UPROPERTY(Transient)
	float GrowthProgress = 0.0f;

	UPROPERTY(Transient)
	bool bGrowing = false;
};
