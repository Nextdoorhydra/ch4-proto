#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CMBloodDefinition.generated.h"

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Instant VFX")
	FCMBloodInstantVFXDefinition Impact;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Instant VFX")
	FCMBloodInstantVFXDefinition Burst;
};