#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Runtime/Surface/Presentation/CMBloodDecalPresentationTypes.h"

#include "CMBloodDecalActor.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;


/**
 * Blood surface mark 하나의 renderer lifecycle을 소유하는 Blueprintable base actor.
 *
 * Sampling, semantic mark registry, global lifetime, eviction, pooling은 담당하지 않는다.
 */
UCLASS(Blueprintable)
class CMGORE_API ACMBloodDecalActor : public AActor
{
	GENERATED_BODY()

public:
	ACMBloodDecalActor();

	UFUNCTION(BlueprintCallable, Category = "CMGore|Blood Decal")
	void ActivatePresentation(
		const FCMBloodDecalSpawnContext& Context,
		UMaterialInterface* MaterialOverride);

	UFUNCTION(BlueprintCallable, Category = "CMGore|Blood Decal")
	void DeactivatePresentation();

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Decal")
	bool IsPresentationActive() const
	{
		return bPresentationActive;
	}

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Decal")
	UDecalComponent* GetDecalComponent() const
	{
		return DecalComponent;
	}

	UFUNCTION(BlueprintPure, Category = "CMGore|Blood Decal")
	UMaterialInstanceDynamic* GetDynamicMaterial() const
	{
		return DynamicMaterial;
	}

protected:
	virtual void PostInitializeComponents() override;

	/** Material별 parameter 설정은 C++ base가 아니라 subclass/BP가 소유한다. */
	UFUNCTION(BlueprintNativeEvent, Category = "CMGore|Blood Decal")
	void ConfigureMaterial(
		UMaterialInstanceDynamic* MID,
		const FCMBloodDecalSpawnContext& Context);

	virtual void ConfigureMaterial_Implementation(
		UMaterialInstanceDynamic* MID,
		const FCMBloodDecalSpawnContext& Context);

	/** Optional Niagara/mesh component를 구성할 Blueprint extension point. */
	UFUNCTION(BlueprintImplementableEvent, Category = "CMGore|Blood Decal")
	void OnPresentationActivated(
		const FCMBloodDecalSpawnContext& Context);

	UFUNCTION(BlueprintImplementableEvent, Category = "CMGore|Blood Decal")
	void OnPresentationDeactivated();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CMGore|Blood Decal")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CMGore|Blood Decal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDecalComponent> DecalComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CMGore|Blood Decal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> DefaultMaterial;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "CMGore|Blood Decal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "CMGore|Blood Decal", meta = (AllowPrivateAccess = "true"))
	bool bPresentationActive = false;
};
