#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataForgeBindingPreset.generated.h"

UENUM(BlueprintType)
enum class EDataForgeBindingCardinality : uint8
{
	One,
	OptionalOne,
	Many
};

UENUM(BlueprintType)
enum class EDataForgeBindingReconcileMode : uint8
{
	Assign,
	ReplaceManaged,
	MergeByKey,
	Manual
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeBindingPresetSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding Slot")
	FName SlotId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding Slot")
	FName AssetKind = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding Slot")
	FName Role = NAME_None;

	/** Association Source id on the RuleSet. Empty automatically selects the only configured source; with no source the slot remains semantic-only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Association")
	FName AssociationSourceId = NAME_None;

	/** Primary source column matched against the Association Source Match Column. Empty uses the RuleSet primary key. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Association")
	FName SourceKeyColumn = NAME_None;

	/** Explicit property path on Target Class. Leave empty only when exactly one compatible top-level property exists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding Slot")
	FString TargetProperty;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding Slot")
	TSoftClassPtr<UObject> ExpectedAssetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding Slot")
	EDataForgeBindingCardinality Cardinality = EDataForgeBindingCardinality::One;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding Slot")
	EDataForgeBindingReconcileMode Reconcile = EDataForgeBindingReconcileMode::Assign;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding Slot")
	bool bRequired = true;
};

UCLASS(BlueprintType)
class DATAFORGECORE_API UDataForgeBindingPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DataForge")
	FGuid PresetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generated Output")
	FName OutputName = TEXT("Data");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generated Output")
	TSubclassOf<UDataAsset> TargetClass;

	/** Generated name prefix. The RuleSet primary-key column is appended as a token, for example DA_{Id}. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generated Output")
	FString AssetNamePrefix = TEXT("DA");

	/** Optional DataTable Row property receiving the generated output. Empty means infer only when one compatible property exists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generated Output")
	FString RowReferenceProperty;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding Slots")
	TArray<FDataForgeBindingPresetSlot> Slots;

	virtual void PostInitProperties() override;
};
