#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Stage/Obstacle/Data/CMObstacleBalanceTableRow.h"

#include "CMObstacleEffectTypes.generated.h"

UENUM(BlueprintType)
enum class ECMPartObstacleStatusEffect : uint8
{
    None,
    Slowed,
    Electrified
};

UENUM(BlueprintType)
enum class ECMControlObstacleStatusEffect : uint8
{
    None,
    Confused UMETA(DisplayName = "Confusion"),
    Delirious UMETA(DisplayName = "Delirium")
};

UENUM(BlueprintType)
enum class ECMControlStatusApplicationPolicy : uint8
{
    OnceOnEnter,
    WhileOverlapping
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMControlObstacleEffectConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECMControlObstacleStatusEffect StatusEffect =
        ECMControlObstacleStatusEffect::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (EditCondition = "StatusEffect != ECMControlObstacleStatusEffect::None",
            EditConditionHides))
    ECMControlStatusApplicationPolicy ApplicationPolicy =
        ECMControlStatusApplicationPolicy::OnceOnEnter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (ClampMin = "0.01",
            EditCondition = "StatusEffect != ECMControlObstacleStatusEffect::None && ApplicationPolicy == ECMControlStatusApplicationPolicy::OnceOnEnter",
            EditConditionHides, Units = "s"))
    float Duration = 5.0f;
};

UENUM(BlueprintType)
enum class ECMHeadVisionEffectApplicationPolicy : uint8
{
    OnceOnEnter,
    PeriodicWhileOverlapping,
    WhileOverlapping
};

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
        meta = (ClampMin = "0.01",
            EditCondition = "ApplicationPolicy == ECMObstacleEffectApplicationPolicy::PeriodicWhileOverlapping",
            EditConditionHides))
    float PeriodSeconds = 1.0f;

    UPROPERTY(Transient, BlueprintReadOnly)
    float DamagePerApplication = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECMPartObstacleStatusEffect StatusEffect =
        ECMPartObstacleStatusEffect::None;

    UPROPERTY(Transient, BlueprintReadOnly)
    FGameplayTag StatusTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (ClampMin = "0.01",
            EditCondition = "StatusEffect != ECMPartObstacleStatusEffect::None && ApplicationPolicy != ECMObstacleEffectApplicationPolicy::WhileOverlapping",
            EditConditionHides, Units = "s"))
    float StatusDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (ClampMin = "0.0", ClampMax = "1.0",
            EditCondition = "StatusEffect == ECMPartObstacleStatusEffect::Slowed",
            EditConditionHides))
    float MovementMultiplier = 1.0f;

    UPROPERTY(Transient, BlueprintReadOnly)
    bool bBlocksAbility = false;
};

USTRUCT(BlueprintType)
struct CHIMERA_API FCMHeadVisionObstacleEffectConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECMHeadVisionEffectApplicationPolicy ApplicationPolicy =
        ECMHeadVisionEffectApplicationPolicy::WhileOverlapping;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (ClampMin = "0.01",
            EditCondition = "ApplicationPolicy == ECMHeadVisionEffectApplicationPolicy::PeriodicWhileOverlapping",
            EditConditionHides, Units = "s"))
    float PeriodSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (ClampMin = "0.01",
            EditCondition = "ApplicationPolicy != ECMHeadVisionEffectApplicationPolicy::WhileOverlapping",
            EditConditionHides, Units = "s"))
    float StatusDuration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VisionAngleMultiplier = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VisionDistanceMultiplier = 0.5f;
};
