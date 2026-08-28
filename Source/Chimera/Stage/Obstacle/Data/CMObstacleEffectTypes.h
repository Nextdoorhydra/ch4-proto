#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "CMObstacleEffectTypes.generated.h"

class UGameplayEffect;

UENUM(BlueprintType)
enum class ECMObstacleEffectApplicationPolicy : uint8
{
    OnceOnEnter,              // 진입할 때 한 번 적용
    PeriodicWhileOverlapping, // 겹쳐 있는 동안 일정 주기로 적용
    WhileOverlapping,         // 진입 시 적용하고 이탈 시 제거
    KillOnEnter               // 진입한 파츠를 즉시 파괴
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMPartObstacleEffectConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "bEnabled"))
    ECMObstacleEffectApplicationPolicy ApplicationPolicy =
        ECMObstacleEffectApplicationPolicy::OnceOnEnter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "bEnabled", ClampMin = "0.01"))
    float PeriodSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
    float DamagePerApplication = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "bEnabled"))
    FGameplayTag StatusTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
    float StatusDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
    float MovementMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "bEnabled"))
    bool bBlocksAbility = false;
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMChimeraObstacleEffectConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "bEnabled"))
    ECMObstacleEffectApplicationPolicy ApplicationPolicy =
        ECMObstacleEffectApplicationPolicy::OnceOnEnter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "bEnabled", ClampMin = "0.01"))
    float PeriodSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Gameplay",
        meta = (EditCondition = "bEnabled"))
    TSubclassOf<UGameplayEffect> GameplayEffectClass;
};
