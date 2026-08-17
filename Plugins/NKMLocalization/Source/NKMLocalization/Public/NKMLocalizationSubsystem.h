#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NKMLocalizationSubsystem.generated.h"

struct FStreamableHandle;
class UStringTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNKMLocalizationTablesReady);

/**
 * Preloads generated NKM String Tables once per GameInstance. Runtime text
 * resolution uses a find-only policy and therefore depends on this subsystem.
 */
UCLASS()
class NKMLOCALIZATION_API UNKMLocalizationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="NKM|Localization")
	bool AreStringTablesReady() const { return bLoadComplete && FailedTableCount == 0; }

	UFUNCTION(BlueprintPure, Category="NKM|Localization")
	int32 GetFailedTableCount() const { return FailedTableCount; }

	UPROPERTY(BlueprintAssignable, Category="NKM|Localization")
	FNKMLocalizationTablesReady OnStringTablesReady;

private:
	void HandleTablesLoaded();
	void CompleteLoad(int32 InFailedTableCount);

	TArray<FSoftObjectPath> RequestedTablePaths;
	TSharedPtr<FStreamableHandle> LoadHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStringTable>> LoadedTables;

	bool bLoadComplete = false;
	int32 FailedTableCount = 0;
};
