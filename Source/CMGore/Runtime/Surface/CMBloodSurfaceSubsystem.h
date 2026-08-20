#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Runtime/Surface/CMBloodSurfaceTypes.h"

#include "CMBloodSurfaceSubsystem.generated.h"

class ACMBloodDecalActor;

UCLASS()
class CMGORE_API UCMBloodSurfaceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
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
		TWeakObjectPtr<ACMBloodDecalActor> PresentationActor;
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
	ACMBloodDecalActor* AcquirePresentationActor(
		TSubclassOf<ACMBloodDecalActor> PresentationClass);

	ACMBloodDecalActor* CreatePresentationActor(
		TSubclassOf<ACMBloodDecalActor> PresentationClass);

	void ReleasePresentationActor(
		ACMBloodDecalActor* PresentationActor);

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

	/** GC 및 actor lifetime을 subsystem이 명시적으로 소유한다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ACMBloodDecalActor>> OwnedPresentationActors;

	/** Presentation class가 서로 다른 Actor를 섞지 않는 lazy pool. */
	TMap<
		TSubclassOf<ACMBloodDecalActor>,
		TArray<TWeakObjectPtr<ACMBloodDecalActor>>
	> AvailablePresentationActors;
};
