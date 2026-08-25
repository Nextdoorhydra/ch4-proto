#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMMechanismWeightComponent.generated.h"

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 감압판과 무게 퍼즐에서 물리 질량 대신 사용할 게임플레이 무게 제공
class CHIMERA_API UCMMechanismWeightComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMMechanismWeightComponent();

    UFUNCTION(BlueprintPure, Category = "Chimera|Mechanism|Weight")
    float GetMechanismWeight() const { return MechanismWeight; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Weight",
        meta = (ClampMin = "0.0"))
    float MechanismWeight = 10.0f;
};
