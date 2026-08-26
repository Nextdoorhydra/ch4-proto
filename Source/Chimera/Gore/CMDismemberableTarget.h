#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CMDismemberableTarget.generated.h"

USTRUCT(BlueprintType)
struct CHIMERA_API FCMDismembermentHitRequest
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

UINTERFACE(BlueprintType)
class CHIMERA_API UCMDismemberableTarget : public UInterface
{
    GENERATED_BODY()
};

class CHIMERA_API ICMDismemberableTarget
{
    GENERATED_BODY()

public:
    /** Returns the number of body parts severed by this accepted attack. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Dismemberment")
    int32 ReceiveDismembermentHit(
        const FCMDismembermentHitRequest& Request
    );
};
