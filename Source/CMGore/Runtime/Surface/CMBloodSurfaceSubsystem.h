#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Runtime/Surface/CMBloodSurfaceTypes.h"

#include "CMBloodSurfaceSubsystem.generated.h"

class UDecalComponent;

UCLASS()
class CMGORE_API UCMBloodSurfaceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	/**
	 * 하나의 Blood Event에 대응하는 제한된 Surface Sample을 생성한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "CMGore|Blood Surface")
	TArray<FCMBloodResidueHandle> SpawnSurfaceBurst(
		const FCMBloodSurfaceBurstRequest& Request);

	/**
	 * 특정 Blood Mark 제거.
	 */
	UFUNCTION(BlueprintCallable, Category = "CMGore|Blood Surface")
	bool RemoveBloodMark(FCMBloodResidueHandle Handle);

	/**
	 * 현재 월드의 모든 Blood Mark 제거.
	 */
	UFUNCTION(BlueprintCallable, Category = "CMGore|Blood Surface")
	void ClearBloodMarks();

	/**
	 * Handle -> semantic Blood Mark lookup.
	 */
	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Surface")
	bool FindBloodMark(
		FCMBloodResidueHandle Handle,
		FCMBloodMark& OutBloodMark) const;

	/**
	 * 미래의 Highlight / Footprint / Investigation 기능을 위한 조회 API.
	 *
	 * Phase 4에서는 단순 linear query로 충분하다.
	 */
	UFUNCTION(BlueprintCallable, Category = "CMGore|Blood Surface")
	void GetBloodMarksInRadius(
		FVector Center,
		float Radius,
		TArray<FCMBloodMark>& OutBloodMarks) const;

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Surface")
	int32 GetActiveBloodMarkCount() const
	{
		return ActiveBloodMarks.Num();
	}

private:
	struct FRuntimeBloodMarkState
	{
		TWeakObjectPtr<UDecalComponent> DecalComponent;
		FTimerHandle ExpirationTimer;
	};

private:
	FCMBloodResidueHandle SpawnBloodMarkFromHit(
		const FHitResult& Hit,
		const FCMBloodSurfaceBurstRequest& Request,
		FRandomStream& RandomStream);

	void HandleBloodMarkExpired(FCMBloodResidueHandle Handle);

	void EvictOldestBloodMarkIfNeeded();

private:
	UDecalComponent* AcquireDecalComponent();
	UDecalComponent* CreateDecalComponent();
	void ReleaseDecalComponent(UDecalComponent* DecalComponent);

private:
	/**
	 * Semantic state.
	 *
	 * 이후 gameplay/presentation system은 이 registry를 대상으로 조회한다.
	 */
	TMap<FCMBloodResidueHandle, FCMBloodMark> ActiveBloodMarks;

	/**
	 * Rendering state.
	 *
	 * FCMBloodMark와 의도적으로 별도 보관.
	 */
	TMap<FCMBloodResidueHandle, FRuntimeBloodMarkState> RuntimeBloodMarkStates;

	/**
	 * oldest eviction을 위한 생성 순서.
	 */
	TArray<FCMBloodResidueHandle> BloodMarkSpawnOrder;

	/**
	 * GC가 pooled decal component를 제거하지 않도록 subsystem이 strong reference 보유.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> OwnedDecalComponents;

	/**
	 * 사용 가능 pool.
	 */
	TArray<TWeakObjectPtr<UDecalComponent>> AvailableDecalComponents;
};