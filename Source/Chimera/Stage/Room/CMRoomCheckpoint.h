#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMRoomCheckpoint.generated.h"

class UArrowComponent;
class USceneComponent;

UCLASS(Blueprintable)
// 룸 진입이 확정된 뒤 공용 키메라가 복귀할 위치
class CHIMERA_API ACMRoomCheckpoint : public AActor
{
    GENERATED_BODY()

public:
    ACMRoomCheckpoint();

    UFUNCTION(BlueprintPure, Category = "Chimera|Stage|Checkpoint")
    FName GetRoomId() const { return RoomId; }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(
        FDataValidationContext& Context) const override;
#endif

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Stage|Checkpoint")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Stage|Checkpoint")
    TObjectPtr<UArrowComponent> SpawnDirection;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Stage|Checkpoint")
    FName RoomId;
};
