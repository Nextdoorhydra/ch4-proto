#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CMCombatHitTarget.generated.h"

USTRUCT(BlueprintType)
struct CHIMERA_API FCMCombatHitRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<AActor> Attacker;

    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<AActor> SourcePart;

    UPROPERTY(BlueprintReadWrite)
    FGuid AttackId;

    UPROPERTY(BlueprintReadWrite)
    FVector ImpactPoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    FVector ImpactDirection = FVector::ForwardVector;
};
/** Generic arm-hit contract; receivers decide whether the hit has any effect. */
UINTERFACE(BlueprintType)
class CHIMERA_API UCMCombatHitTarget : public UInterface
{
    GENERATED_BODY()
};

class CHIMERA_API ICMCombatHitTarget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Combat")
    bool ReceiveCombatHit(const FCMCombatHitRequest& Request);
};
