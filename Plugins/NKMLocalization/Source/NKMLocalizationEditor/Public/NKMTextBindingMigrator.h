#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationTypes.h"

/** One-time editor migration from legacy reflected parser rows to authoritative JSON. */
class NKMLOCALIZATIONEDITOR_API FNKMTextBindingMigrator
{
public:
	static bool MigrateConfiguredProfiles(bool bOverwriteExisting, FNKMLocalizationResult& OutResult);
};
