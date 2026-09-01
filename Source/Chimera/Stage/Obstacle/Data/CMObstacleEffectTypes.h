#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Stage/Obstacle/Data/CMObstacleBalanceTableRow.h"

#include "CMObstacleEffectTypes.generated.h"

class UGameplayEffect;

UENUM(BlueprintType)
enum class ECMObstacleEffectApplicationPolicy : uint8
{
    OnceOnEnter,              // 진입할 때 한 번 적용
    PeriodicWhileOverlapping, // 진입 즉시 한 번 적용 후 겹쳐 있는 동안 일정 주기로 적용
    WhileOverlapping,         // 진입 시 적용하고 이탈 시 제거
    KillOnEnter               // 진입한 파츠를 즉시 파괴
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMPartObstacleEffectConfig
{
    GENERATED_BODY()

    UPROPERTY(Transient, BlueprintReadOnly)
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECMObstacleEffectApplicationPolicy ApplicationPolicy =
        ECMObstacleEffectApplicationPolicy::OnceOnEnter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (ClampMin = "0.01"))
    float PeriodSeconds = 1.0f;

    UPROPERTY(Transient, BlueprintReadOnly)
    float DamagePerApplication = 0.0f;

    UPROPERTY(Transient, BlueprintReadOnly)
    FGameplayTag StatusTag;

    UPROPERTY(Transient, BlueprintReadOnly)
    float StatusDuration = 0.0f;

    UPROPERTY(Transient, BlueprintReadOnly)
    float MovementMultiplier = 1.0f;

    UPROPERTY(Transient, BlueprintReadOnly)
    bool bBlocksAbility = false;
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMChimeraObstacleEffectConfig
{
    GENERATED_BODY()

    UPROPERTY(Transient, BlueprintReadOnly)
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECMObstacleEffectApplicationPolicy ApplicationPolicy =
        ECMObstacleEffectApplicationPolicy::OnceOnEnter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (ClampMin = "0.01"))
    float PeriodSeconds = 1.0f;

    UPROPERTY(Transient, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Gameplay")
    float StatusDuration = 0.0f;

    UPROPERTY(Transient, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Gameplay")
    ECMObstacleStatusEffect StatusEffect = ECMObstacleStatusEffect::None;

    UPROPERTY(Transient, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Gameplay")
    float PrimaryStatusValue = 0.0f;

    UPROPERTY(Transient, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Gameplay")
    float SecondaryStatusValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Gameplay")
    TSubclassOf<UGameplayEffect> GameplayEffectClass;
};
