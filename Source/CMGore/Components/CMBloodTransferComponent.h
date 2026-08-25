#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Runtime/Surface/CMBloodSurfaceTypes.h"

#include "CMBloodTransferComponent.generated.h"

class UPrimitiveComponent;
struct FHitResult;

UENUM(BlueprintType)
enum class ECMBloodTransferState : uint8
{
	Dry,
	Loaded,
	Painting
};

/** Transfers blood from a logical pool into a bounded surface brush stroke. */
UCLASS(ClassGroup = (CMGore), meta = (BlueprintSpawnableComponent))
class CMGORE_API UCMBloodTransferComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCMBloodTransferComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "CMGore|Blood Transfer")
	void InitializeTransfer(
		UPrimitiveComponent* InTargetPrimitive,
		FName InDefaultBloodDefinitionId);

	/** Feeds one surface contact into the same state machine used by Chaos hits. */
	UFUNCTION(BlueprintCallable, Category = "CMGore|Blood Transfer")
	bool ProcessContactSample(
		FVector ContactLocation,
		FVector ContactNormal,
		float ContactSpeed);

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Transfer")
	ECMBloodTransferState GetTransferState() const
	{
		return TransferState;
	}

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Transfer")
	float GetRemainingStrokeDistance() const
	{
		return RemainingStrokeDistance;
	}

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Transfer")
	float GetTotalPaintedDistance() const
	{
		return TotalPaintedDistance;
	}

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Transfer")
	int32 GetSpawnedStrokeStampCount() const
	{
		return SpawnedStrokeStampCount;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Transfer")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Transfer")
	bool bAutoFindTargetPrimitive = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Transfer")
	FName DefaultBloodDefinitionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Transfer",
		meta = (ClampMin = "0.0"))
	float PoolQueryRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Transfer",
		meta = (ClampMin = "0.0"))
	float SurfaceTraceDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Transfer")
	TEnumAsByte<ECollisionChannel> SurfaceTraceChannel = ECC_Visibility;

private:
	void BindTargetPrimitive();
	void UnbindTargetPrimitive();
	bool FindPoolAtContact(
		const FVector& ContactLocation,
		FCMBloodMark& OutPoolMark) const;
	void LoadFromPool(
		const FCMBloodMark& PoolMark,
		const FVector& ContactLocation,
		const FVector& ContactNormal);
	void PaintSegment(
		const FVector& ContactLocation,
		const FVector& ContactNormal);

	UFUNCTION()
	void HandleTargetHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> TargetPrimitive;

	UPROPERTY(Transient)
	ECMBloodTransferState TransferState = ECMBloodTransferState::Dry;

	UPROPERTY(Transient)
	FName LoadedBloodDefinitionId = NAME_None;

	UPROPERTY(Transient)
	FCMBloodResidueHandle LoadedPoolHandle;

	UPROPERTY(Transient)
	float RemainingStrokeDistance = 0.0f;

	UPROPERTY(Transient)
	float TotalPaintedDistance = 0.0f;

	UPROPERTY(Transient)
	int32 SpawnedStrokeStampCount = 0;

	FVector LastPaintPoint = FVector::ZeroVector;
	FVector LastContactNormal = FVector::UpVector;
	bool bHasLastPaintPoint = false;
};
