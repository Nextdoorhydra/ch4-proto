#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "CMVisionSettings.generated.h"

class UCMVisionRenderConfig;

/** Project-wide entry point for selecting the default Vision renderer. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Vision"))
class CHIMERA_API UCMVisionSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly,
        Category = "Rendering")
    TSoftObjectPtr<UCMVisionRenderConfig> DefaultRenderConfig;
};
